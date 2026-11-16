#include "messagecorrections.h"

#include <QKeyEvent>
#include <QTextEdit>
#include <definitions/namespaces.h>
#include <definitions/messagehandlerorders.h>
#include <definitions/messageeditsendhandlerorders.h>
#include <definitions/messageviewlabels.h>
#include <utils/action.h>
#include <utils/logger.h>

// Corrections remembered per view to restore them when the messages are shown again
#define MAX_VIEW_CORRECTIONS     200

MessageCorrections::MessageCorrections()
{
	FMessageProcessor = NULL;
	FMessageWidgets = NULL;
	FMultiChatManager = NULL;
	FDiscovery = NULL;
}

MessageCorrections::~MessageCorrections()
{

}

void MessageCorrections::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Message Corrections");
	APluginInfo->description = tr("Allows to correct the last sent message and shows the corrections made by contacts");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Vacuum-IM contributors";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(MESSAGEPROCESSOR_UUID);
	APluginInfo->dependences.append(MESSAGEWIDGETS_UUID);
}

bool MessageCorrections::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	IPlugin *plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
		if (FMessageProcessor)
			connect(FMessageProcessor->instance(),SIGNAL(messageSent(const Message &)),SLOT(onMessageSent(const Message &)));
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(viewWidgetCreated(IMessageViewWidget *)),SLOT(onViewWidgetCreated(IMessageViewWidget *)));
			connect(FMessageWidgets->instance(),SIGNAL(editWidgetCreated(IMessageEditWidget *)),SLOT(onEditWidgetCreated(IMessageEditWidget *)));
		}
	}

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
		FMultiChatManager = qobject_cast<IMultiUserChatManager *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IServiceDiscovery").value(0,NULL);
	if (plugin)
		FDiscovery = qobject_cast<IServiceDiscovery *>(plugin->instance());

	return FMessageProcessor!=NULL && FMessageWidgets!=NULL;
}

bool MessageCorrections::initObjects()
{
	FMessageProcessor->insertMessageHandler(MHO_MESSAGECORRECTIONS,this);
	FMessageWidgets->insertEditSendHandler(MESHO_MESSAGECORRECTIONS,this);

	if (FDiscovery)
	{
		IDiscoFeature dfeature;
		dfeature.active = true;
		dfeature.var = NS_MESSAGE_CORRECT;
		dfeature.name = tr("Last Message Correction");
		dfeature.description = tr("Supports the correction of the last sent message");
		FDiscovery->insertDiscoFeature(dfeature);
	}
	return true;
}

bool MessageCorrections::messageCheck(int AOrder, const Message &AMessage, int ADirection)
{
	Q_UNUSED(AOrder);
	IMessageViewWidget *view;
	QString contentId;
	// Corrections of messages which are not shown are shown as new messages by other handlers
	return findCorrectedMessage(AMessage,ADirection,view,contentId);
}

bool MessageCorrections::messageDisplay(const Message &AMessage, int ADirection)
{
	IMessageViewWidget *view;
	QString contentId;
	if (findCorrectedMessage(AMessage,ADirection,view,contentId))
	{
		LOG_STRM_DEBUG(ADirection==IMessageProcessor::DirectionIn ? AMessage.toJid() : AMessage.fromJid(),QString("Showing message correction, content=%1").arg(contentId));
		showCorrection(view,contentId,AMessage);
		return true;
	}
	return false;
}

INotification MessageCorrections::messageNotify(INotifications *ANotifications, const Message &AMessage, int ADirection)
{
	Q_UNUSED(ANotifications); Q_UNUSED(AMessage); Q_UNUSED(ADirection);
	// The corrected message was already notified
	return INotification();
}

IMessageWindow *MessageCorrections::messageShowNotified(int AMessageId)
{
	Q_UNUSED(AMessageId);
	return NULL;
}

IMessageWindow *MessageCorrections::messageGetWindow(const Jid &AStreamJid, const Jid &AContactJid, Message::MessageType AType)
{
	Q_UNUSED(AStreamJid); Q_UNUSED(AContactJid); Q_UNUSED(AType);
	return NULL;
}

bool MessageCorrections::messageEditSendPrepare(int AOrder, IMessageEditWidget *AWidget)
{
	Q_UNUSED(AOrder); Q_UNUSED(AWidget);
	return false;
}

bool MessageCorrections::messageEditSendProcesse(int AOrder, IMessageEditWidget *AWidget)
{
	IMessageWindow *window = AWidget->messageWindow();
	if (AOrder!=MESHO_MESSAGECORRECTIONS || !FCorrections.contains(window))
		return false;

	const Correction correction = FCorrections.value(window);

	Message message;
	if (!FMessageProcessor->textToMessage(AWidget->document(),message))
		return false;

	if (message.body() == correction.text)
	{
		// Nothing to correct
		finishCorrection(window,false);
		return true;
	}

	message.stanza().addElement("replace",NS_MESSAGE_CORRECT).setAttribute("id",correction.originalId);

	bool sent = false;
	IMultiUserChatWindow *mucWindow = qobject_cast<IMultiUserChatWindow *>(window->instance());
	IMultiUserChatWindow *privateMucWindow = mucWindow==NULL ? findConference(window->streamJid(),window->contactJid()) : NULL;
	if (mucWindow)
	{
		sent = mucWindow->multiUserChat()->sendMessage(message);
	}
	else if (privateMucWindow)
	{
		sent = privateMucWindow->multiUserChat()->sendMessage(message,window->contactJid().resource());
	}
	else
	{
		message.setType(Message::Chat).setTo(window->contactJid().full());
		sent = FMessageProcessor->sendMessage(window->streamJid(),message,IMessageProcessor::DirectionOut);
	}

	if (sent)
	{
		LOG_STRM_INFO(window->streamJid(),QString("Message correction sent, to=%1, replace=%2").arg(window->contactJid().full(),correction.originalId));
		finishCorrection(window,false);
	}
	else
	{
		LOG_STRM_WARNING(window->streamJid(),QString("Failed to send message correction, to=%1, replace=%2").arg(window->contactJid().full(),correction.originalId));
	}
	return sent;
}

bool MessageCorrections::isCorrecting(IMessageWindow *AWindow) const
{
	return FCorrections.contains(AWindow);
}

bool MessageCorrections::startCorrection(IMessageWindow *AWindow)
{
	if (FCorrections.contains(AWindow))
		return true;

	IMessageEditWidget *editWidget = AWindow!=NULL ? AWindow->editWidget() : NULL;
	SentMessage sent = FSentMessages.value(AWindow);
	if (editWidget==NULL || sent.originalId.isEmpty())
		return false;

	// Without the original message in the view the correction would be shown as a new message
	if (AWindow->viewWidget()==NULL || !AWindow->viewWidget()->hasMessage(sentContentId(AWindow,sent.originalId)))
		return false;

	QTextEdit *textEdit = editWidget->textEdit();

	Correction correction;
	correction.originalId = sent.originalId;
	correction.text = sent.text;
	correction.palette = textEdit->palette();
	correction.toolTip = textEdit->toolTip();
	FCorrections.insert(AWindow,correction);

	// Tint the editor with the selection color while correcting
	QPalette palette = correction.palette;
	QColor base = palette.color(QPalette::Base);
	QColor highlight = palette.color(QPalette::Highlight);
	palette.setColor(QPalette::Base,QColor((base.red()*4+highlight.red())/5,(base.green()*4+highlight.green())/5,(base.blue()*4+highlight.blue())/5));
	textEdit->setPalette(palette);
	textEdit->setToolTip(tr("Correcting the last message, press Esc to cancel"));

	textEdit->setPlainText(sent.text);
	textEdit->moveCursor(QTextCursor::End);

	LOG_STRM_DEBUG(AWindow->streamJid(),QString("Message correction started, with=%1, replace=%2").arg(AWindow->contactJid().full(),sent.originalId));
	emit correctionStarted(AWindow);
	return true;
}

void MessageCorrections::cancelCorrection(IMessageWindow *AWindow)
{
	finishCorrection(AWindow,true);
}

IMultiUserChatWindow *MessageCorrections::findConference(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FMultiChatManager!=NULL ? FMultiChatManager->findMultiChatWindow(AStreamJid,AContactJid.bare()) : NULL;
}

IMessageWindow *MessageCorrections::findWindow(const Jid &AStreamJid, const Jid &AContactJid, Message::MessageType AType) const
{
	if (AType == Message::GroupChat)
		return findConference(AStreamJid,AContactJid);
	if (AType == Message::Chat)
	{
		// Private chats with conference occupants are bound to the full JID
		bool exact = findConference(AStreamJid,AContactJid)!=NULL;
		return FMessageWidgets->findChatWindow(AStreamJid,AContactJid,exact);
	}
	return NULL;
}

QString MessageCorrections::sentContentId(IMessageWindow *AWindow, const QString &AMessageId) const
{
	// Messages sent to a conference are shown when the room returns them with our nick
	IMultiUserChatWindow *mucWindow = qobject_cast<IMultiUserChatWindow *>(AWindow->instance());
	if (mucWindow)
		return messageViewContentId(false,AMessageId,mucWindow->multiUserChat()->nickname());
	return messageViewContentId(true,AMessageId);
}

bool MessageCorrections::findCorrectedMessage(const Message &AMessage, int ADirection, IMessageViewWidget *&AView, QString &AContentId) const
{
	AView = NULL;
	QString replaceId = AMessage.stanza().firstElement("replace",NS_MESSAGE_CORRECT).attribute("id");
	if (replaceId.isEmpty() || !FMessageProcessor->messageHasText(AMessage))
		return false;

	bool directionIn = ADirection==IMessageProcessor::DirectionIn;
	Jid streamJid = directionIn ? AMessage.toJid() : AMessage.fromJid();
	Jid contactJid = directionIn ? AMessage.fromJid() : AMessage.toJid();

	if (AMessage.type() == Message::GroupChat)
	{
		// Messages to a conference are shown when the room sends them back
		if (!directionIn || !contactJid.hasResource())
			return false;
		// The nick in the content id lets occupants correct only their own messages
		AContentId = messageViewContentId(false,replaceId,contactJid.resource());
	}
	else if (AMessage.type() == Message::Chat)
	{
		AContentId = messageViewContentId(!directionIn,replaceId);
	}
	else
	{
		return false;
	}

	IMessageWindow *window = findWindow(streamJid,contactJid,AMessage.type());
	AView = window!=NULL ? window->viewWidget() : NULL;
	return AView!=NULL && AView->hasMessage(AContentId);
}

void MessageCorrections::showCorrection(IMessageViewWidget *AView, const QString &AContentId, const Message &AMessage)
{
	QHash<QString, Message> &corrections = FViewCorrections[AView->instance()];
	corrections.remove(AContentId);
	if (corrections.count() >= MAX_VIEW_CORRECTIONS)
		corrections.erase(corrections.begin());
	corrections.insert(AContentId,AMessage);

	AView->replaceMessage(AContentId,AMessage);

	QString toolTip = tr("Edited at %1").arg(AMessage.dateTime().toString(QLocale().timeFormat(QLocale::ShortFormat)));
	AView->setMessageLabel(AContentId,MVL_MESSAGECORRECTIONS,QString("<span style='color:gray' title='%1'>&#x270E;</span>").arg(toolTip.toHtmlEscaped()));
}

void MessageCorrections::finishCorrection(IMessageWindow *AWindow, bool AClearText)
{
	if (FCorrections.contains(AWindow))
	{
		Correction correction = FCorrections.take(AWindow);
		IMessageEditWidget *editWidget = AWindow->editWidget();
		if (editWidget)
		{
			editWidget->textEdit()->setPalette(correction.palette);
			editWidget->textEdit()->setToolTip(correction.toolTip);
			if (AClearText)
				editWidget->textEdit()->clear();
		}
		emit correctionFinished(AWindow);
	}
}

IMessageWindow *MessageCorrections::editorWindow(QObject *AEditor) const
{
	for (QObject *object = AEditor; object!=NULL; object = object->parent())
		if (FEditWindows.contains(object))
			return FEditWindows.value(object);
	return NULL;
}

bool MessageCorrections::eventFilter(QObject *AWatched, QEvent *AEvent)
{
	if (AEvent->type() == QEvent::ShortcutOverride)
	{
		// Esc closes the message window by default, while correcting it cancels the correction
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(AEvent);
		if (keyEvent->key()==Qt::Key_Escape && keyEvent->modifiers()==Qt::NoModifier && FCorrections.contains(editorWindow(AWatched)))
		{
			AEvent->accept();
			return true;
		}
	}
	return QObject::eventFilter(AWatched,AEvent);
}

void MessageCorrections::onMessageSent(const Message &AMessage)
{
	if ((AMessage.type()==Message::Chat || AMessage.type()==Message::GroupChat) && !AMessage.body().isEmpty())
	{
		IMessageWindow *window = findWindow(AMessage.fromJid(),AMessage.toJid(),AMessage.type());
		if (window)
		{
			// Every correction refers to the original message
			QString replaceId = AMessage.stanza().firstElement("replace",NS_MESSAGE_CORRECT).attribute("id");

			SentMessage sent;
			sent.originalId = !replaceId.isEmpty() ? replaceId : messageViewMessageId(AMessage);
			sent.text = AMessage.body();
			FSentMessages.insert(window,sent);
		}
	}
}

void MessageCorrections::onViewWidgetCreated(IMessageViewWidget *AWidget)
{
	connect(AWidget->instance(),SIGNAL(contentAppended(const QString &, const IMessageStyleContentOptions &)),
		SLOT(onViewWidgetContentAppended(const QString &, const IMessageStyleContentOptions &)));
	connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onViewWidgetDestroyed(QObject *)));
}

void MessageCorrections::onViewWidgetContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions)
{
	Q_UNUSED(AHtml);
	IMessageViewWidget *view = qobject_cast<IMessageViewWidget *>(sender());
	if (view && !AOptions.contentId.isEmpty())
	{
		QHash<QObject *, QHash<QString, Message> >::const_iterator it = FViewCorrections.constFind(view->instance());
		if (it!=FViewCorrections.constEnd() && it->contains(AOptions.contentId))
			showCorrection(view,AOptions.contentId,it->value(AOptions.contentId));
	}
}

void MessageCorrections::onViewWidgetDestroyed(QObject *AObject)
{
	FViewCorrections.remove(AObject);
}

void MessageCorrections::onEditWidgetCreated(IMessageEditWidget *AWidget)
{
	FEditWindows.insert(AWidget->instance(),AWidget->messageWindow());
	connect(AWidget->instance(),SIGNAL(keyEventReceived(QKeyEvent *, bool &)),SLOT(onEditWidgetKeyEventReceived(QKeyEvent *, bool &)));
	connect(AWidget->instance(),SIGNAL(contextMenuRequested(const QPoint &, Menu *)),SLOT(onEditWidgetContextMenuRequested(const QPoint &, Menu *)));
	connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onEditWidgetDestroyed(QObject *)));
	AWidget->textEdit()->installEventFilter(this);
}

void MessageCorrections::onEditWidgetKeyEventReceived(QKeyEvent *AKeyEvent, bool &AHooked)
{
	IMessageWindow *window = FEditWindows.value(sender());
	if (window==NULL || AHooked)
		return;

	if (AKeyEvent->key()==Qt::Key_Up && AKeyEvent->modifiers()==Qt::NoModifier)
	{
		// Up in an empty editor starts the correction, as in other clients
		IMessageEditWidget *editWidget = window->editWidget();
		if (editWidget && editWidget->document()->isEmpty() && !FCorrections.contains(window))
			AHooked = startCorrection(window);
	}
	else if (AKeyEvent->key()==Qt::Key_Escape && AKeyEvent->modifiers()==Qt::NoModifier && FCorrections.contains(window))
	{
		cancelCorrection(window);
		AHooked = true;
	}
}

void MessageCorrections::onEditWidgetContextMenuRequested(const QPoint &APosition, Menu *AMenu)
{
	Q_UNUSED(APosition);
	IMessageWindow *window = FEditWindows.value(sender());
	if (window!=NULL && FSentMessages.contains(window))
	{
		Action *action = new Action(AMenu);
		if (FCorrections.contains(window))
		{
			action->setText(tr("Cancel Correction"));
			action->setData(Action::DR_Parametr1,false);
		}
		else
		{
			action->setText(tr("Correct Last Message"));
			action->setData(Action::DR_Parametr1,true);
		}
		action->setData(Action::DR_Parametr2,QVariant::fromValue<qint64>((qint64)window->instance()));
		connect(action,SIGNAL(triggered(bool)),SLOT(onCorrectLastMessageAction(bool)));
		AMenu->addAction(action,AG_DEFAULT,true);
	}
}

void MessageCorrections::onEditWidgetDestroyed(QObject *AObject)
{
	IMessageWindow *window = FEditWindows.take(AObject);
	FSentMessages.remove(window);
	FCorrections.remove(window);
}

void MessageCorrections::onCorrectLastMessageAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		QWidget *windowWidget = (QWidget *)action->data(Action::DR_Parametr2).toLongLong();
		foreach(IMessageWindow *window, FEditWindows)
		{
			if (window->instance() == windowWidget)
			{
				if (action->data(Action::DR_Parametr1).toBool())
					startCorrection(window);
				else
					cancelCorrection(window);
				break;
			}
		}
	}
}
