#include "viewwidget.h"
#include <QRegularExpression>

#include <QTextFrame>
#include <QTextTable>
#include <QTextBrowser>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTextDocumentFragment>
#include <interfaces/imessagedocumentcontext.h>
#include <utils/textmanager.h>
#include <utils/pluginhelper.h>

ViewWidget::ViewWidget(IMessageWidgets *AMessageWidgets, IMessageWindow *AWindow, QWidget *AParent) : QWidget(AParent)
{
	ui.setupUi(this);
	setAcceptDrops(true);

	FStyleWidget = NULL;
	FMessageStyle = NULL;

	FWindow = AWindow;
	FMessageWidgets = AMessageWidgets;
	FMessageProcessor = PluginHelper::pluginInstance<IMessageProcessor>();

	QVBoxLayout *layout = new QVBoxLayout(ui.wdtViewer);
	layout->setContentsMargins(0, 0, 0, 0);
}

ViewWidget::~ViewWidget()
{

}

bool ViewWidget::isVisibleOnWindow() const
{
	return isVisibleTo(FWindow->instance());
}

IMessageWindow *ViewWidget::messageWindow() const
{
	return FWindow;
}

void ViewWidget::clearContent()
{
	if (FMessageStyle)
		FMessageStyle->changeOptions(FStyleWidget,FStyleOptions,true);
}

QWidget *ViewWidget::styleWidget() const
{
	return FStyleWidget;
}

IMessageStyle *ViewWidget::messageStyle() const
{
	return FMessageStyle;
}

void ViewWidget::setMessageStyle(IMessageStyle *AStyle, const IMessageStyleOptions &AOptions)
{
	if (FMessageStyle != AStyle)
	{
		if (FMessageStyle)
		{
			// Invalidate pending feature-plugin operations before the old style
			// widget is destroyed/replaced.
			resetMessageDocumentContexts(FStyleWidget);

			disconnect(FMessageStyle->instance(),SIGNAL(urlClicked(QWidget *, const QUrl &)),
				this,SLOT(onMessageStyleUrlClicked(QWidget *, const QUrl &)));
			disconnect(FStyleWidget,SIGNAL(customContextMenuRequested(const QPoint &)),
				this,SLOT(onMessageStyleWidgetCustomContextMenuRequested(const QPoint &)));
			disconnect(FMessageStyle->instance(),SIGNAL(contentAppended(QWidget *, const QString &, const IMessageStyleContentOptions &)),
				this, SLOT(onMessageStyleContentAppended(QWidget *, const QString &, const IMessageStyleContentOptions &)));
			disconnect(FMessageStyle->instance(),SIGNAL(optionsChanged(QWidget *, const IMessageStyleOptions &, bool)),
				this,SLOT(onMessageStyleOptionsChanged(QWidget *, const IMessageStyleOptions &, bool)));

			ui.wdtViewer->layout()->removeWidget(FStyleWidget);
			FStyleWidget->deleteLater();
			FStyleWidget = NULL;
		}

		IMessageStyle *before = FMessageStyle;
		FMessageStyle = AStyle;

		if (FMessageStyle)
		{
			FStyleWidget = FMessageStyle->createWidget(AOptions,ui.wdtViewer);
			FStyleWidget->setContextMenuPolicy(Qt::CustomContextMenu);
			ui.wdtViewer->layout()->addWidget(FStyleWidget);

			connect(FMessageStyle->instance(),SIGNAL(urlClicked(QWidget *, const QUrl &)),
				SLOT(onMessageStyleUrlClicked(QWidget *, const QUrl &)));
			connect(FStyleWidget,SIGNAL(customContextMenuRequested(const QPoint &)),
				SLOT(onMessageStyleWidgetCustomContextMenuRequested(const QPoint &)));
			connect(FMessageStyle->instance(),SIGNAL(contentAppended(QWidget *, const QString &, const IMessageStyleContentOptions &)),
				SLOT(onMessageStyleContentAppended(QWidget *, const QString &, const IMessageStyleContentOptions &)));
			connect(FMessageStyle->instance(),SIGNAL(optionsChanged(QWidget *, const IMessageStyleOptions &, bool)),
				SLOT(onMessageStyleOptionsChanged(QWidget *, const IMessageStyleOptions &, bool)));
		}

		FStyleOptions = AOptions;
		emit messageStyleChanged(before,AOptions);
	}
}

bool ViewWidget::appendHtml(const QString &AHtml, const IMessageStyleContentOptions &AOptions)
{
	return FMessageStyle!=NULL && !AHtml.isEmpty() ? FMessageStyle->appendContent(FStyleWidget,AHtml,AOptions) : false;
}

bool ViewWidget::appendText(const QString &AText, const IMessageStyleContentOptions &AOptions)
{
	if (!AText.isEmpty())
	{
		Message message;
		message.setBody(AText);
		return appendMessage(message,AOptions);
	}
	return false;
}

bool ViewWidget::appendMessage(const Message &AMessage, const IMessageStyleContentOptions &AOptions)
{
	IMessageStyleContentOptions options = AOptions;
	bool meCommand = false;
	bool *meCommandCheck = AOptions.kind==IMessageStyleContentOptions::KindMessage && !AOptions.senderName.isEmpty() ? &meCommand : NULL;

	QString html;
	if (messageToHtml(AMessage,html,meCommandCheck))
	{
		if (meCommand)
			options.kind = IMessageStyleContentOptions::KindMeCommand;
		if (options.contentId.isEmpty() && options.kind!=IMessageStyleContentOptions::KindStatus && options.kind!=IMessageStyleContentOptions::KindTopic)
			options.contentId = messageViewContentId(options.direction==IMessageStyleContentOptions::DirectionOut, messageViewMessageId(AMessage));
		return appendHtml(html,options);
	}
	return false;
}

bool ViewWidget::hasMessage(const QString &AContentId) const
{
	return FMessageStyle!=NULL && !AContentId.isEmpty() ? FMessageStyle->hasContent(FStyleWidget,AContentId) : false;
}

bool ViewWidget::replaceMessage(const QString &AContentId, const Message &AMessage)
{
	QString html;
	if (FMessageStyle!=NULL && !AContentId.isEmpty() && messageToHtml(AMessage,html,NULL))
		return FMessageStyle->replaceContent(FStyleWidget,AContentId,html);
	return false;
}

bool ViewWidget::setMessageLabel(const QString &AContentId, int ALabelId, const QString &AHtml)
{
	return FMessageStyle!=NULL && !AContentId.isEmpty() ? FMessageStyle->setContentLabel(FStyleWidget,AContentId,ALabelId,AHtml) : false;
}

void ViewWidget::contextMenuForView(const QPoint &APosition, Menu *AMenu)
{
	emit viewContextMenu(APosition, AMenu);
}

QTextDocumentFragment ViewWidget::selection() const
{
	return FMessageStyle!=NULL ? FMessageStyle->selection(FStyleWidget) : QTextDocumentFragment();
}

QTextCharFormat ViewWidget::textFormatAt(const QPoint &APosition) const
{
	if (FMessageStyle && FStyleWidget)
		return FMessageStyle->textFormatAt(FStyleWidget,FStyleWidget->mapFromGlobal(mapToGlobal(APosition)));
	return QTextCharFormat();
}

QTextDocumentFragment ViewWidget::textFragmentAt(const QPoint &APosition) const
{
	if (FMessageStyle && FStyleWidget)
		return FMessageStyle->textFragmentAt(FStyleWidget,FStyleWidget->mapFromGlobal(mapToGlobal(APosition)));
	return QTextDocumentFragment();
}

void ViewWidget::initialize()
{
}

bool ViewWidget::messageToHtml(const Message &AMessage, QString &AHtml, bool *AMeCommand) const
{
	bool hasText = false;

	QTextDocument doc;
	if (FMessageProcessor)
	{
		hasText = FMessageProcessor->messageToText(AMessage,&doc);
	}
	else if (!AMessage.body().isEmpty())
	{
		hasText = true;
		doc.setPlainText(AMessage.body());
	}

	if (hasText)
	{
		// "/me" command
		if (AMeCommand)
		{
			static const QRegularExpression me(QRegularExpression::anchoredPattern("/me\\s"));

			QTextCursor cursor(&doc);
			cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,4);
			*AMeCommand = me.match(cursor.selectedText()).hasMatch();
			if (*AMeCommand)
				cursor.removeSelectedText();
		}
		AHtml = TextManager::getDocumentBody(doc);
	}
	return hasText;
}

void ViewWidget::dropEvent(QDropEvent *AEvent)
{
	Menu *dropMenu = new Menu(this);

	bool accepted = false;
	foreach(IMessageViewDropHandler *handler, FActiveDropHandlers)
		if (handler->messageViewDropAction(this, AEvent, dropMenu))
			accepted = true;

	if (accepted && !dropMenu->isEmpty())
	{
		if (dropMenu->exec(mapToGlobal(AEvent->position().toPoint())))
			AEvent->acceptProposedAction();
		else
			AEvent->ignore();
	}
	else
	{
		AEvent->ignore();
	}
	delete dropMenu;
}

void ViewWidget::dragEnterEvent(QDragEnterEvent *AEvent)
{
	FActiveDropHandlers.clear();
	foreach(IMessageViewDropHandler *handler, FMessageWidgets->viewDropHandlers())
		if (handler->messageViewDragEnter(this, AEvent))
			FActiveDropHandlers.append(handler);

	if (!FActiveDropHandlers.isEmpty())
		AEvent->acceptProposedAction();
	else
		AEvent->ignore();
}

void ViewWidget::dragMoveEvent(QDragMoveEvent *AEvent)
{
	bool accepted = false;
	foreach(IMessageViewDropHandler *handler, FActiveDropHandlers)
		if (handler->messageViewDragMove(this, AEvent))
			accepted = true;

	if (accepted)
		AEvent->acceptProposedAction();
	else
		AEvent->ignore();
}

void ViewWidget::dragLeaveEvent(QDragLeaveEvent *AEvent)
{
	foreach(IMessageViewDropHandler *handler, FActiveDropHandlers)
		handler->messageViewDragLeave(this, AEvent);
}

void ViewWidget::onMessageStyleUrlClicked(QWidget *AWidget, const QUrl &AUrl)
{
	if (AWidget == FStyleWidget)
	{
		QMultiMap<int,IMessageViewUrlHandler *> handlers = FMessageWidgets->viewUrlHandlers();
		for (QMultiMap<int,IMessageViewUrlHandler *>::const_iterator it = handlers.constBegin(); it!=handlers.constEnd(); ++it)
			if (it.value()->messageViewUrlOpen(it.key(),this,AUrl))
				break;
		emit urlClicked(AUrl);
	}
}

void ViewWidget::onMessageStyleWidgetCustomContextMenuRequested(const QPoint &APosition)
{
	Menu *menu = new Menu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose, true);

	contextMenuForView(APosition, menu);

	if (!menu->isEmpty())
		menu->popup(FStyleWidget->mapToGlobal(APosition));
	else
		delete menu;
}

void ViewWidget::onMessageStyleOptionsChanged(QWidget *AWidget, const IMessageStyleOptions &AOptions, bool AClean)
{
	if (AWidget == FStyleWidget)
	{
		FStyleOptions = AOptions;
		if (AClean)
			resetMessageDocumentContexts(AWidget);
		emit messageStyleOptionsChanged(AOptions,AClean);
	}
}

void ViewWidget::resetMessageDocumentContexts(QWidget *AStyleWidget)
{
	if (!FMessageWidgets || !AStyleWidget)
		return;

	QTextBrowser *browser = qobject_cast<QTextBrowser *>(AStyleWidget);
	IMessageDocumentContextManager *manager = qobject_cast<IMessageDocumentContextManager *>(FMessageWidgets->instance());
	if (browser && manager)
		manager->resetMessageContexts(browser->document());
}

void ViewWidget::onMessageStyleContentAppended(QWidget *AWidget, const QString &AHtml, const IMessageStyleContentOptions &AOptions)
{
	if (AWidget == FStyleWidget)
		emit contentAppended(AHtml,AOptions);
}
