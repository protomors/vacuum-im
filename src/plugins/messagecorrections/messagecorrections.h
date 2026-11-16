#ifndef MESSAGECORRECTIONS_H
#define MESSAGECORRECTIONS_H

#include <QPalette>
#include <interfaces/ipluginmanager.h>
#include <interfaces/imessagecorrections.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imultiuserchat.h>
#include <interfaces/iservicediscovery.h>

class MessageCorrections :
	public QObject,
	public IPlugin,
	public IMessageCorrections,
	public IMessageHandler,
	public IMessageEditSendHandler
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IMessageCorrections IMessageHandler IMessageEditSendHandler);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.MessageCorrections");
public:
	struct SentMessage {
		QString originalId;
		QString text;
	};
	struct Correction {
		QString originalId;
		QString text;
		QPalette palette;
		QString toolTip;
	};
public:
	MessageCorrections();
	~MessageCorrections();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return MESSAGECORRECTIONS_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings() { return true; }
	virtual bool startPlugin() { return true; }
	//IMessageHandler
	virtual bool messageCheck(int AOrder, const Message &AMessage, int ADirection);
	virtual bool messageDisplay(const Message &AMessage, int ADirection);
	virtual INotification messageNotify(INotifications *ANotifications, const Message &AMessage, int ADirection);
	virtual IMessageWindow *messageShowNotified(int AMessageId);
	virtual IMessageWindow *messageGetWindow(const Jid &AStreamJid, const Jid &AContactJid, Message::MessageType AType);
	//IMessageEditSendHandler
	virtual bool messageEditSendPrepare(int AOrder, IMessageEditWidget *AWidget);
	virtual bool messageEditSendProcesse(int AOrder, IMessageEditWidget *AWidget);
	//IMessageCorrections
	virtual bool isCorrecting(IMessageWindow *AWindow) const;
	virtual bool startCorrection(IMessageWindow *AWindow);
	virtual void cancelCorrection(IMessageWindow *AWindow);
signals:
	void correctionStarted(IMessageWindow *AWindow);
	void correctionFinished(IMessageWindow *AWindow);
protected:
	IMultiUserChatWindow *findConference(const Jid &AStreamJid, const Jid &AContactJid) const;
	IMessageWindow *findWindow(const Jid &AStreamJid, const Jid &AContactJid, Message::MessageType AType) const;
	QString sentContentId(IMessageWindow *AWindow, const QString &AMessageId) const;
	bool findCorrectedMessage(const Message &AMessage, int ADirection, IMessageViewWidget *&AView, QString &AContentId) const;
	void showCorrection(IMessageViewWidget *AView, const QString &AContentId, const Message &AMessage);
	void finishCorrection(IMessageWindow *AWindow, bool AClearText);
	IMessageWindow *editorWindow(QObject *AEditor) const;
protected:
	bool eventFilter(QObject *AWatched, QEvent *AEvent);
protected slots:
	void onMessageSent(const Message &AMessage);
	void onViewWidgetCreated(IMessageViewWidget *AWidget);
	void onViewWidgetContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions);
	void onViewWidgetDestroyed(QObject *AObject);
	void onEditWidgetCreated(IMessageEditWidget *AWidget);
	void onEditWidgetKeyEventReceived(QKeyEvent *AKeyEvent, bool &AHooked);
	void onEditWidgetContextMenuRequested(const QPoint &APosition, Menu *AMenu);
	void onEditWidgetDestroyed(QObject *AObject);
	void onCorrectLastMessageAction(bool);
private:
	IMessageProcessor *FMessageProcessor;
	IMessageWidgets *FMessageWidgets;
	IMultiUserChatManager *FMultiChatManager;
	IServiceDiscovery *FDiscovery;
private:
	QHash<QObject *, IMessageWindow *> FEditWindows;
	QHash<IMessageWindow *, SentMessage> FSentMessages;
	QHash<IMessageWindow *, Correction> FCorrections;
	// Corrections shown in a view by content id, applied again when the content is shown again
	QHash<QObject *, QHash<QString, Message> > FViewCorrections;
};

#endif // MESSAGECORRECTIONS_H
