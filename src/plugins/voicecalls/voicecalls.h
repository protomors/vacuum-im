#ifndef VOICECALLS_H
#define VOICECALLS_H

#include <QMap>
#include <QList>
#include <QSoundEffect>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ivoicecalls.h>
#include <interfaces/ijingle.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/irostermanager.h>
#include <interfaces/inotifications.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/imessagecarbons.h>
#include <interfaces/imultiuserchat.h>
#include <interfaces/irostersview.h>
#include <interfaces/ioptionsmanager.h>
#include <utils/xmpperror.h>
#include "callsession.h"
#include "calldialog.h"

class VoiceCalls :
	public QObject,
	public IPlugin,
	public IVoiceCalls,
	public IStanzaHandler,
	public IJingleHandler,
	public IOptionsDialogHolder
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IVoiceCalls IStanzaHandler IJingleHandler IOptionsDialogHolder);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.VoiceCalls");
public:
	VoiceCalls();
	~VoiceCalls();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return VOICECALLS_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin() { return true; }
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IJingleHandler
	virtual bool jingleSessionInitiate(const Jid &AStreamJid, const Stanza &ARequest);
	virtual bool jingleSessionRequest(const Jid &AStreamJid, const Stanza &ARequest);
	//IOptionsDialogHolder
	virtual QMultiMap<int, IOptionsDialogWidget *> optionsDialogWidgets(const QString &ANodeId, QWidget *AParent);
	//IVoiceCalls
	virtual bool isAvailable() const;
	virtual bool isSupported(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual bool hasCall() const;
	virtual bool startCall(const Jid &AStreamJid, const Jid &AContactJid);
signals:
	void callStarted(const Jid &AStreamJid, const Jid &AContactJid, const QString &ACallId);
	void callFinished(const Jid &AStreamJid, const Jid &AContactJid, const QString &ACallId);
protected:
	bool isStreamOpen(const Jid &AStreamJid) const;
	bool isRoomJid(const Jid &AStreamJid, const Jid &AContactJid) const;
	bool isRosterContact(const Jid &AStreamJid, const Jid &AContactJid) const;
	bool isCallCapable(const Jid &AStreamJid, const Jid &AContactJid, bool AProposals) const;
	QList<IPresenceItem> contactResources(const Jid &AStreamJid, const Jid &AContactJid) const;
	CallSession *findSession(const Jid &AStreamJid, const QString &ASessionId) const;
	CallSession *findContactSession(const Jid &AStreamJid, const Jid &AContactJid) const;
	CallSession *createSession(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, bool AOutgoing);
	CallDialog *showCallDialog(CallSession *ASession, bool AActivate);
	void processJingleMessage(const Jid &AStreamJid, const Stanza &AMessage);
	void processCallProposal(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, const QDomElement &AProposal);
	void notifyIncomingCall(CallSession *ASession);
	void notifyMissedCall(const Jid &AStreamJid, const Jid &AContactJid);
	void removeCallNotify(CallSession *ASession);
	void startRinging(const Jid &AStreamJid);
	void stopRinging();
	void showCallEvent(const Jid &AStreamJid, const Jid &AContactJid, const QString &AText);
	void updateToolBarAction(IMessageToolBarWidget *AWidget);
	void updateToolBarActions();
protected slots:
	void onXmppStreamOpened(IXmppStream *AXmppStream);
	void onXmppStreamClosed(IXmppStream *AXmppStream);
	void onShutdownStarted();
	void onCarbonMessageSent(const Jid &AStreamJid, const Message &AMessage);
	void onDiscoInfoChanged(const IDiscoInfo &AInfo);
	void onPresenceItemReceived(IPresence *APresence, const IPresenceItem &AItem, const IPresenceItem &ABefore);
protected slots:
	void onSessionStateChanged();
	void onSessionEnded();
	void onCallDialogDestroyed();
protected slots:
	void onNotificationActivated(int ANotifyId);
	void onNotificationRemoved(int ANotifyId);
protected slots:
	void onToolBarWidgetCreated(IMessageToolBarWidget *AWidget);
	void onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore);
	void onToolBarWidgetDestroyed(QObject *AObject);
	void onRostersViewIndexContextMenu(const QList<IRosterIndex *> &AIndexes, quint32 ALabelId, Menu *AMenu);
	void onCallActionTriggered(bool);
private:
	IXmppStreamManager *FXmppStreamManager;
	IStanzaProcessor *FStanzaProcessor;
	IJingle *FJingle;
	IServiceDiscovery *FDiscovery;
	IPresenceManager *FPresenceManager;
	IRosterManager *FRosterManager;
	INotifications *FNotifications;
	IMessageWidgets *FMessageWidgets;
	IMessageProcessor *FMessageProcessor;
	IMessageCarbons *FMessageCarbons;
	IMultiUserChatManager *FMultiChatManager;
	IRostersViewPlugin *FRostersViewPlugin;
	IOptionsManager *FOptionsManager;
private:
	bool FAvailable;
	QMap<Jid, int> FSHIMessages;
	QList<CallSession *> FSessions;
	QMap<CallSession *, CallDialog *> FDialogs;
	QMap<CallSession *, int> FCallNotifies;
	QMap<int, QPair<Jid, Jid> > FMissedNotifies;
	QMap<IMessageToolBarWidget *, Action *> FToolBarActions;
	QSoundEffect *FRingSound;
};

#endif // VOICECALLS_H
