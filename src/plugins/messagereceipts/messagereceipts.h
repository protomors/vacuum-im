#ifndef MESSAGERECEIPTS_H
#define MESSAGERECEIPTS_H

#include <interfaces/ipluginmanager.h>
#include <interfaces/imessagereceipts.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imessagearchiver.h>
#include <interfaces/imultiuserchat.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/irostermanager.h>
#include <interfaces/ioptionsmanager.h>

class MessageReceipts :
	public QObject,
	public IPlugin,
	public IMessageReceipts,
	public IMessageEditor,
	public IArchiveHandler,
	public IOptionsDialogHolder
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IMessageReceipts IMessageEditor IArchiveHandler IOptionsDialogHolder);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.MessageReceipts");
public:
	struct Conversation {
		// Ids of sent messages, from the oldest to the newest
		QList<QString> sent;
		// Id of a sent message -> id of the displayed message it changes
		QHash<QString, QString> targets;
		// Id of a displayed message -> IMessageReceipts::MessageState
		QHash<QString, int> states;
		// Last received markable message which is not marked as displayed yet
		QString markableId;
		Jid markableFrom;
	};
public:
	MessageReceipts();
	~MessageReceipts();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return MESSAGERECEIPTS_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin() { return true; }
	//IMessageEditor
	virtual bool messageReadWrite(int AOrder, const Jid &AStreamJid, Message &AMessage, int ADirection);
	//IArchiveHandler
	virtual bool archiveMessageEdit(int AOrder, const Jid &AStreamJid, Message &AMessage, bool ADirectionIn);
	//IOptionsDialogHolder
	virtual QMultiMap<int, IOptionsDialogWidget *> optionsDialogWidgets(const QString &ANodeId, QWidget *AParent);
	//IMessageReceipts
	virtual int messageState(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessageId) const;
signals:
	void messageStateChanged(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessageId, int AState);
protected:
	Jid conversationJid(const Jid &AStreamJid, const Jid &AContactJid) const;
	bool isConference(const Jid &AStreamJid, const Jid &AContactJid) const;
	bool isPresenceAuthorized(const Jid &AStreamJid, const Jid &AContactJid) const;
	IMessageChatWindow *findWindow(const Jid &AStreamJid, const Jid &AContactJid) const;
	void processOutgoingMessage(const Jid &AStreamJid, Message &AMessage);
	void processIncomingMessage(const Jid &AStreamJid, const Message &AMessage);
	void registerSentMessage(const Jid &AStreamJid, const Jid &AContactJid, const QString &AId, const QString &ATargetId);
	void changeMessageState(const Jid &AStreamJid, const Jid &AContactJid, const QString &AId, int AState, bool APrevious);
	void updateMessageLabel(IMessageViewWidget *AView, const QString &AMessageId, int AState) const;
	void sendReceipt(const Jid &AStreamJid, const Message &AMessage);
	void sendDisplayedMarker(const Jid &AStreamJid, const Jid &AContactJid);
protected slots:
	void onMessageReceived(const Message &AMessage);
	void onActiveStreamRemoved(const Jid &AStreamJid);
	void onChatWindowCreated(IMessageChatWindow *AWindow);
	void onChatWindowActivated();
	void onViewWidgetContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions);
private:
	IMessageProcessor *FMessageProcessor;
	IStanzaProcessor *FStanzaProcessor;
	IMessageWidgets *FMessageWidgets;
	IMessageArchiver *FMessageArchiver;
	IMultiUserChatManager *FMultiChatManager;
	IServiceDiscovery *FDiscovery;
	IRosterManager *FRosterManager;
	IOptionsManager *FOptionsManager;
private:
	QHash<Jid, QHash<Jid, Conversation> > FConversations;
};

#endif // MESSAGERECEIPTS_H
