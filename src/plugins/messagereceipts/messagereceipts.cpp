#include "messagereceipts.h"

#include <definitions/namespaces.h>
#include <definitions/optionvalues.h>
#include <definitions/optionnodes.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/messageeditororders.h>
#include <definitions/messageviewlabels.h>
#include <definitions/archivehandlerorders.h>
#include <utils/options.h>
#include <utils/logger.h>

// Sent messages remembered per conversation to match receipts and markers
#define MAX_SENT_MESSAGES        500

MessageReceipts::MessageReceipts()
{
	FMessageProcessor = NULL;
	FStanzaProcessor = NULL;
	FMessageWidgets = NULL;
	FMessageArchiver = NULL;
	FMultiChatManager = NULL;
	FDiscovery = NULL;
	FRosterManager = NULL;
	FOptionsManager = NULL;
}

MessageReceipts::~MessageReceipts()
{

}

void MessageReceipts::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Message Receipts");
	APluginInfo->description = tr("Shows whether the sent messages were delivered and read, and notifies contacts about received messages");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Vacuum-IM contributors";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(MESSAGEPROCESSOR_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool MessageReceipts::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	IPlugin *plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
		if (FMessageProcessor)
		{
			connect(FMessageProcessor->instance(),SIGNAL(messageReceived(const Message &)),SLOT(onMessageReceived(const Message &)));
			connect(FMessageProcessor->instance(),SIGNAL(activeStreamRemoved(const Jid &)),SLOT(onActiveStreamRemoved(const Jid &)));
		}
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
			connect(FMessageWidgets->instance(),SIGNAL(chatWindowCreated(IMessageChatWindow *)),SLOT(onChatWindowCreated(IMessageChatWindow *)));
	}

	plugin = APluginManager->pluginInterface("IMessageArchiver").value(0,NULL);
	if (plugin)
		FMessageArchiver = qobject_cast<IMessageArchiver *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
		FMultiChatManager = qobject_cast<IMultiUserChatManager *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IServiceDiscovery").value(0,NULL);
	if (plugin)
		FDiscovery = qobject_cast<IServiceDiscovery *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IRosterManager").value(0,NULL);
	if (plugin)
		FRosterManager = qobject_cast<IRosterManager *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());

	return FMessageProcessor!=NULL && FStanzaProcessor!=NULL;
}

bool MessageReceipts::initObjects()
{
	FMessageProcessor->insertMessageEditor(MEO_MESSAGERECEIPTS,this);

	if (FMessageArchiver)
		FMessageArchiver->insertArchiveHandler(AHO_DEFAULT,this);

	if (FDiscovery)
	{
		IDiscoFeature dfeature;
		dfeature.active = true;
		dfeature.var = NS_RECEIPTS;
		dfeature.name = tr("Message Delivery Receipts");
		dfeature.description = tr("Supports the notifications of message delivery");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_CHAT_MARKERS;
		dfeature.name = tr("Chat Markers");
		dfeature.description = tr("Supports the notifications of read messages");
		FDiscovery->insertDiscoFeature(dfeature);
	}
	return true;
}

bool MessageReceipts::initSettings()
{
	Options::setDefaultValue(OPV_MESSAGES_SENDRECEIPTS,true);
	Options::setDefaultValue(OPV_MESSAGES_SENDDISPLAYEDMARKERS,true);

	if (FOptionsManager)
		FOptionsManager->insertOptionsDialogHolder(this);

	return true;
}

bool MessageReceipts::messageReadWrite(int AOrder, const Jid &AStreamJid, Message &AMessage, int ADirection)
{
	if (AOrder==MEO_MESSAGERECEIPTS && (AMessage.type()==Message::Chat || AMessage.type()==Message::Normal))
	{
		if (ADirection == IMessageProcessor::DirectionOut)
			processOutgoingMessage(AStreamJid,AMessage);
		else
			processIncomingMessage(AStreamJid,AMessage);
	}
	return false;
}

bool MessageReceipts::archiveMessageEdit(int AOrder, const Jid &AStreamJid, Message &AMessage, bool ADirectionIn)
{
	Q_UNUSED(AOrder); Q_UNUSED(AStreamJid); Q_UNUSED(ADirectionIn);
	QDomElement requestElem = AMessage.stanza().firstElement("request",NS_RECEIPTS);
	QDomElement markableElem = AMessage.stanza().firstElement("markable",NS_CHAT_MARKERS);
	if (!requestElem.isNull() || !markableElem.isNull())
	{
		AMessage.detach();
		foreach(QDomElement elem, QList<QDomElement>() << AMessage.stanza().firstElement("request",NS_RECEIPTS) << AMessage.stanza().firstElement("markable",NS_CHAT_MARKERS))
			if (!elem.isNull())
				elem.parentNode().removeChild(elem);
	}
	return false;
}

QMultiMap<int, IOptionsDialogWidget *> MessageReceipts::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *> widgets;
	if (FOptionsManager && ANodeId==OPN_MESSAGES)
	{
		widgets.insert(OWO_MESSAGES_SENDRECEIPTS, FOptionsManager->newOptionsDialogWidget(Options::node(OPV_MESSAGES_SENDRECEIPTS),tr("Notify contacts that their messages were delivered"),AParent));
		widgets.insert(OWO_MESSAGES_SENDDISPLAYEDMARKERS, FOptionsManager->newOptionsDialogWidget(Options::node(OPV_MESSAGES_SENDDISPLAYEDMARKERS),tr("Notify contacts that you have read their messages"),AParent));
	}
	return widgets;
}

int MessageReceipts::messageState(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessageId) const
{
	QHash<Jid, QHash<Jid, Conversation> >::const_iterator streamIt = FConversations.constFind(AStreamJid);
	if (streamIt != FConversations.constEnd())
	{
		QHash<Jid, Conversation>::const_iterator it = streamIt->constFind(conversationJid(AStreamJid,AContactJid));
		if (it != streamIt->constEnd())
			return it->states.value(AMessageId,StateUnknown);
	}
	return StateUnknown;
}

Jid MessageReceipts::conversationJid(const Jid &AStreamJid, const Jid &AContactJid) const
{
	// Private chats with conference occupants share the bare JID of the room
	return isConference(AStreamJid,AContactJid) ? AContactJid : AContactJid.bare();
}

bool MessageReceipts::isConference(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FMultiChatManager!=NULL && FMultiChatManager->findMultiChatWindow(AStreamJid,AContactJid.bare())!=NULL;
}

bool MessageReceipts::isPresenceAuthorized(const Jid &AStreamJid, const Jid &AContactJid) const
{
	// Receipts and markers tell when the user is online and active, so like
	// the presence they are only sent to contacts allowed to see the presence
	IRoster *roster = FRosterManager!=NULL ? FRosterManager->findRoster(AStreamJid) : NULL;
	if (roster)
	{
		QString subscription = roster->findItem(AContactJid.bare()).subscription;
		return subscription==SUBSCRIPTION_BOTH || subscription==SUBSCRIPTION_FROM;
	}
	return false;
}

IMessageChatWindow *MessageReceipts::findWindow(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FMessageWidgets!=NULL ? FMessageWidgets->findChatWindow(AStreamJid,AContactJid,isConference(AStreamJid,AContactJid)) : NULL;
}

void MessageReceipts::processOutgoingMessage(const Jid &AStreamJid, Message &AMessage)
{
	// Copies of messages sent by other clients of the user are processed as outgoing too
	bool carbonCopy = !AMessage.stanza().firstElement("sent",NS_MESSAGE_CARBONS).isNull();
	Jid contactJid = AMessage.to();

	QDomElement displayedElem = AMessage.stanza().firstElement("displayed",NS_CHAT_MARKERS);
	if (carbonCopy && !displayedElem.isNull())
	{
		// Read on another client
		Conversation &conversation = FConversations[AStreamJid][conversationJid(AStreamJid,contactJid)];
		if (conversation.markableId == displayedElem.attribute("id"))
			conversation.markableId.clear();
	}

	if (!AMessage.body().isEmpty() && !AMessage.id().isEmpty())
	{
		if (!carbonCopy)
		{
			AMessage.detach();
			if (AMessage.stanza().firstElement("request",NS_RECEIPTS).isNull())
				AMessage.stanza().addElement("request",NS_RECEIPTS);
			if (AMessage.stanza().firstElement("markable",NS_CHAT_MARKERS).isNull())
				AMessage.stanza().addElement("markable",NS_CHAT_MARKERS);
		}

		// Receipts for a correction are shown at the message it corrects
		QString replaceId = AMessage.stanza().firstElement("replace",NS_MESSAGE_CORRECT).attribute("id");
		registerSentMessage(AStreamJid,contactJid,AMessage.id(),!replaceId.isEmpty() ? replaceId : AMessage.id());
	}
}

void MessageReceipts::processIncomingMessage(const Jid &AStreamJid, const Message &AMessage)
{
	bool carbonCopy = !AMessage.stanza().firstElement("received",NS_MESSAGE_CARBONS).isNull();
	Jid contactJid = AMessage.from();

	QDomElement receiptElem = AMessage.stanza().firstElement("received",NS_RECEIPTS);
	if (!receiptElem.isNull())
		changeMessageState(AStreamJid,contactJid,receiptElem.attribute("id"),StateDelivered,false);

	// A marker applies to the marked message and to all messages sent before it
	QDomElement markerElem = AMessage.stanza().firstElement(QString(),NS_CHAT_MARKERS);
	while (!markerElem.isNull())
	{
		if (markerElem.namespaceURI() == NS_CHAT_MARKERS)
		{
			if (markerElem.tagName()=="displayed" || markerElem.tagName()=="acknowledged")
				changeMessageState(AStreamJid,contactJid,markerElem.attribute("id"),StateDisplayed,true);
			else if (markerElem.tagName() == "received")
				changeMessageState(AStreamJid,contactJid,markerElem.attribute("id"),StateDelivered,true);
		}
		markerElem = markerElem.nextSiblingElement();
	}

	if (!AMessage.body().isEmpty() && !AMessage.id().isEmpty())
	{
		// The client which received the original message sends the receipt
		if (!carbonCopy && !AMessage.stanza().firstElement("request",NS_RECEIPTS).isNull())
			sendReceipt(AStreamJid,AMessage);

		if (!AMessage.stanza().firstElement("markable",NS_CHAT_MARKERS).isNull())
		{
			Conversation &conversation = FConversations[AStreamJid][conversationJid(AStreamJid,contactJid)];
			conversation.markableId = AMessage.id();
			conversation.markableFrom = contactJid;
		}
	}
}

void MessageReceipts::registerSentMessage(const Jid &AStreamJid, const Jid &AContactJid, const QString &AId, const QString &ATargetId)
{
	Conversation &conversation = FConversations[AStreamJid][conversationJid(AStreamJid,AContactJid)];
	conversation.sent.append(AId);
	conversation.targets.insert(AId,ATargetId);

	while (conversation.sent.count() > MAX_SENT_MESSAGES)
	{
		QString target = conversation.targets.take(conversation.sent.takeFirst());
		if (!conversation.targets.values().contains(target))
			conversation.states.remove(target);
	}
}

void MessageReceipts::changeMessageState(const Jid &AStreamJid, const Jid &AContactJid, const QString &AId, int AState, bool APrevious)
{
	QHash<Jid, Conversation> &conversations = FConversations[AStreamJid];
	Jid convJid = conversationJid(AStreamJid,AContactJid);
	QHash<Jid, Conversation>::iterator convIt = conversations.find(convJid);
	if (convIt==conversations.end() || AId.isEmpty())
		return;

	// Only messages sent in this conversation can change their state
	int index = convIt->sent.lastIndexOf(AId);
	if (index < 0)
		return;

	QList<QString> changed;
	for (int i=index; i>=0; i--)
	{
		QString target = convIt->targets.value(convIt->sent.at(i));
		int state = convIt->states.value(target,StateUnknown);
		if (state < AState)
		{
			convIt->states.insert(target,AState);
			changed.append(target);
		}
		else if (APrevious && i<index)
		{
			// Earlier messages were marked by an earlier marker
			break;
		}
		if (!APrevious)
			break;
	}

	IMessageChatWindow *window = findWindow(AStreamJid,AContactJid);
	foreach(const QString &target, changed)
	{
		LOG_STRM_DEBUG(AStreamJid,QString("Message state changed, with=%1, id=%2, state=%3").arg(convJid.full(),target).arg(AState));
		if (window)
			updateMessageLabel(window->viewWidget(),target,AState);
		emit messageStateChanged(AStreamJid,convJid,target,AState);
	}
}

void MessageReceipts::updateMessageLabel(IMessageViewWidget *AView, const QString &AMessageId, int AState) const
{
	QString html;
	if (AState == StateDelivered)
		html = QString("<span style='color:gray' title='%1'>&#x2713;</span>").arg(tr("Delivered").toHtmlEscaped());
	else if (AState == StateDisplayed)
		html = QString("<span style='color:gray' title='%1'>&#x2713;&#x2713;</span>").arg(tr("Read").toHtmlEscaped());
	if (AView)
		AView->setMessageLabel(messageViewContentId(true,AMessageId),MVL_MESSAGERECEIPTS,html);
}

void MessageReceipts::sendReceipt(const Jid &AStreamJid, const Message &AMessage)
{
	if (Options::node(OPV_MESSAGES_SENDRECEIPTS).value().toBool() && isPresenceAuthorized(AStreamJid,AMessage.fromJid()))
	{
		Message receipt;
		receipt.setType(AMessage.type()).setTo(AMessage.from()).setRandomId();
		receipt.stanza().addElement("received",NS_RECEIPTS).setAttribute("id",AMessage.id());
		if (FStanzaProcessor->sendStanzaOut(AStreamJid,receipt.stanza()))
			LOG_STRM_DEBUG(AStreamJid,QString("Message receipt sent, to=%1, id=%2").arg(AMessage.from(),AMessage.id()));
		else
			LOG_STRM_WARNING(AStreamJid,QString("Failed to send message receipt, to=%1, id=%2").arg(AMessage.from(),AMessage.id()));
	}
}

void MessageReceipts::sendDisplayedMarker(const Jid &AStreamJid, const Jid &AContactJid)
{
	QHash<Jid, Conversation> &conversations = FConversations[AStreamJid];
	QHash<Jid, Conversation>::iterator convIt = conversations.find(conversationJid(AStreamJid,AContactJid));
	if (convIt==conversations.end() || convIt->markableId.isEmpty())
		return;

	QString markableId = convIt->markableId;
	Jid markableFrom = convIt->markableFrom;
	convIt->markableId.clear();

	if (Options::node(OPV_MESSAGES_SENDDISPLAYEDMARKERS).value().toBool() && isPresenceAuthorized(AStreamJid,markableFrom))
	{
		Message marker;
		marker.setType(Message::Chat).setTo(markableFrom.full()).setRandomId();
		marker.stanza().addElement("displayed",NS_CHAT_MARKERS).setAttribute("id",markableId);
		// Lets the other clients of the user find out the read messages from the archive
		marker.stanza().addElement("store",NS_MESSAGE_HINTS);
		if (FStanzaProcessor->sendStanzaOut(AStreamJid,marker.stanza()))
			LOG_STRM_DEBUG(AStreamJid,QString("Displayed marker sent, to=%1, id=%2").arg(markableFrom.full(),markableId));
		else
			LOG_STRM_WARNING(AStreamJid,QString("Failed to send displayed marker, to=%1, id=%2").arg(markableFrom.full(),markableId));
	}
}

void MessageReceipts::onMessageReceived(const Message &AMessage)
{
	if (AMessage.type()==Message::Chat && !AMessage.body().isEmpty())
	{
		// Called after the message is displayed
		IMessageChatWindow *window = findWindow(AMessage.to(),AMessage.from());
		if (window && window->isActiveTabPage())
			sendDisplayedMarker(AMessage.to(),AMessage.from());
	}
}

void MessageReceipts::onActiveStreamRemoved(const Jid &AStreamJid)
{
	FConversations.remove(AStreamJid);
}

void MessageReceipts::onChatWindowCreated(IMessageChatWindow *AWindow)
{
	connect(AWindow->instance(),SIGNAL(tabPageActivated()),SLOT(onChatWindowActivated()));
	connect(AWindow->viewWidget()->instance(),SIGNAL(contentAppended(const QString &, const IMessageStyleContentOptions &)),
		SLOT(onViewWidgetContentAppended(const QString &, const IMessageStyleContentOptions &)));
}

void MessageReceipts::onChatWindowActivated()
{
	IMessageChatWindow *window = qobject_cast<IMessageChatWindow *>(sender());
	if (window)
		sendDisplayedMarker(window->streamJid(),window->contactJid());
}

void MessageReceipts::onViewWidgetContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions)
{
	Q_UNUSED(AHtml);
	// Restores the labels when the messages are shown again, e.g. after the history is loaded
	static const QString sentPrefix = messageViewContentId(true,QStringLiteral("-")).chopped(1);
	IMessageViewWidget *view = qobject_cast<IMessageViewWidget *>(sender());
	if (view && AOptions.contentId.startsWith(sentPrefix))
	{
		IMessageWindow *window = view->messageWindow();
		QString messageId = AOptions.contentId.mid(sentPrefix.length());
		int state = messageState(window->streamJid(),window->contactJid(),messageId);
		if (state != StateUnknown)
			updateMessageLabel(view,messageId,state);
	}
}
