#include "voicecalls.h"

#include <QUrl>
#include <QUuid>
#include <definitions/namespaces.h>
#include <definitions/menuicons.h>
#include <definitions/resources.h>
#include <definitions/shortcuts.h>
#include <definitions/soundfiles.h>
#include <definitions/actiongroups.h>
#include <definitions/toolbargroups.h>
#include <definitions/optionnodes.h>
#include <definitions/optionvalues.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/rosterindexkinds.h>
#include <definitions/rosterindexroles.h>
#include <definitions/stanzahandlerorders.h>
#include <definitions/notificationtypes.h>
#include <definitions/notificationdataroles.h>
#include <definitions/notificationtypeorders.h>
#include <utils/advanceditemdelegate.h>
#include <utils/widgetmanager.h>
#include <utils/iconstorage.h>
#include <utils/filestorage.h>
#include <utils/shortcuts.h>
#include <utils/options.h>
#include <utils/action.h>
#include <utils/logger.h>
#include <utils/menu.h>
#include "mediasession.h"

#define SHC_JINGLE_REQUEST            "/iq[@type='set']/jingle[@xmlns='" NS_JINGLE "']"
#define SHC_JINGLE_MESSAGE(action)    "/message/" action "[@xmlns='" NS_JINGLE_MESSAGE "']"

#define ADR_STREAM_JID                Action::DR_StreamJid
#define ADR_CONTACT_JID               Action::DR_Parametr1

VoiceCalls::VoiceCalls()
{
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FDiscovery = NULL;
	FPresenceManager = NULL;
	FRosterManager = NULL;
	FNotifications = NULL;
	FMessageWidgets = NULL;
	FMessageProcessor = NULL;
	FMessageCarbons = NULL;
	FMultiChatManager = NULL;
	FRostersViewPlugin = NULL;
	FOptionsManager = NULL;

	FAvailable = false;
	FRingSound = NULL;
}

VoiceCalls::~VoiceCalls()
{
	foreach(CallDialog *dialog, FDialogs.values())
		delete dialog;
}

void VoiceCalls::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Voice Calls");
	APluginInfo->description = tr("Allows to make and receive voice calls");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Vacuum-IM contributors";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool VoiceCalls::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);

	IPlugin *plugin = APluginManager->pluginInterface("IXmppStreamManager").value(0,NULL);
	if (plugin)
	{
		FXmppStreamManager = qobject_cast<IXmppStreamManager *>(plugin->instance());
		if (FXmppStreamManager)
		{
			connect(FXmppStreamManager->instance(),SIGNAL(streamOpened(IXmppStream *)),SLOT(onXmppStreamOpened(IXmppStream *)));
			connect(FXmppStreamManager->instance(),SIGNAL(streamClosed(IXmppStream *)),SLOT(onXmppStreamClosed(IXmppStream *)));
		}
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
	{
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IServiceDiscovery").value(0,NULL);
	if (plugin)
	{
		FDiscovery = qobject_cast<IServiceDiscovery *>(plugin->instance());
		if (FDiscovery)
		{
			connect(FDiscovery->instance(),SIGNAL(discoInfoReceived(const IDiscoInfo &)),SLOT(onDiscoInfoChanged(const IDiscoInfo &)));
			connect(FDiscovery->instance(),SIGNAL(discoInfoRemoved(const IDiscoInfo &)),SLOT(onDiscoInfoChanged(const IDiscoInfo &)));
		}
	}

	plugin = APluginManager->pluginInterface("IPresenceManager").value(0,NULL);
	if (plugin)
	{
		FPresenceManager = qobject_cast<IPresenceManager *>(plugin->instance());
		if (FPresenceManager)
		{
			connect(FPresenceManager->instance(),SIGNAL(presenceItemReceived(IPresence *, const IPresenceItem &, const IPresenceItem &)),
				SLOT(onPresenceItemReceived(IPresence *, const IPresenceItem &, const IPresenceItem &)));
		}
	}

	plugin = APluginManager->pluginInterface("IRosterManager").value(0,NULL);
	if (plugin)
	{
		FRosterManager = qobject_cast<IRosterManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("INotifications").value(0,NULL);
	if (plugin)
	{
		FNotifications = qobject_cast<INotifications *>(plugin->instance());
		if (FNotifications)
		{
			connect(FNotifications->instance(),SIGNAL(notificationActivated(int)),SLOT(onNotificationActivated(int)));
			connect(FNotifications->instance(),SIGNAL(notificationRemoved(int)),SLOT(onNotificationRemoved(int)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(toolBarWidgetCreated(IMessageToolBarWidget *)),SLOT(onToolBarWidgetCreated(IMessageToolBarWidget *)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMessageCarbons").value(0,NULL);
	if (plugin)
	{
		FMessageCarbons = qobject_cast<IMessageCarbons *>(plugin->instance());
		if (FMessageCarbons)
		{
			connect(FMessageCarbons->instance(),SIGNAL(messageSent(const Jid &, const Message &)),SLOT(onCarbonMessageSent(const Jid &, const Message &)));
		}
	}

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
	{
		FMultiChatManager = qobject_cast<IMultiUserChatManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
	if (plugin)
	{
		FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
		if (FRostersViewPlugin)
		{
			connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(indexContextMenu(const QList<IRosterIndex *> &, quint32, Menu *)),
				SLOT(onRostersViewIndexContextMenu(const QList<IRosterIndex *> &, quint32, Menu *)));
		}
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
	}

	connect(APluginManager->instance(),SIGNAL(shutdownStarted()),SLOT(onShutdownStarted()));

	return FXmppStreamManager!=NULL && FStanzaProcessor!=NULL;
}

bool VoiceCalls::initObjects()
{
	Shortcuts::declareShortcut(SCT_MESSAGEWINDOWS_VOICECALL,tr("Voice call"),QKeySequence::UnknownKey);

	QStringList missing = MediaSession::missingElements();
	FAvailable = missing.isEmpty();
	if (!FAvailable)
		LOG_WARNING(QString("Voice calls are disabled, GStreamer elements not found: %1").arg(missing.join(", ")));

	// Other clients offer calls only to those who announce all of these
	if (FDiscovery && FAvailable)
	{
		IDiscoFeature dfeature;
		dfeature.active = true;
		dfeature.icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_VOICECALLS_CALL);

		dfeature.var = NS_JINGLE;
		dfeature.name = tr("Jingle");
		dfeature.description = tr("Supports the negotiation of media sessions with another contact");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_JINGLE_APPS_RTP;
		dfeature.name = tr("Jingle RTP Sessions");
		dfeature.description = tr("Supports voice calls with another contact");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_JINGLE_APPS_RTP_AUDIO;
		dfeature.name = tr("Jingle Audio");
		dfeature.description = tr("Supports the audio streams in calls");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_JINGLE_TRANSPORTS_ICE_UDP;
		dfeature.name = tr("Jingle ICE-UDP Transport");
		dfeature.description = tr("Supports calls through firewalls and network address translators");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_JINGLE_APPS_DTLS;
		dfeature.name = tr("Jingle DTLS-SRTP");
		dfeature.description = tr("Supports the encryption of calls");
		FDiscovery->insertDiscoFeature(dfeature);

		dfeature.var = NS_JINGLE_MESSAGE;
		dfeature.name = tr("Jingle Message Initiation");
		dfeature.description = tr("Supports calls that ring on all devices of a contact");
		FDiscovery->insertDiscoFeature(dfeature);
	}

	if (FNotifications)
	{
		INotificationType notifyType;
		notifyType.order = NTO_VOICE_CALL;
		notifyType.icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_VOICECALLS_CALL);
		notifyType.title = tr("When receiving a voice call");
		notifyType.kindMask = INotification::RosterNotify|INotification::PopupWindow|INotification::TrayNotify|INotification::TrayAction|INotification::SoundPlay|INotification::AlertWidget|INotification::AutoActivate;
		notifyType.kindDefs = notifyType.kindMask & ~(INotification::AutoActivate);
		FNotifications->registerNotificationType(NNT_VOICE_CALL,notifyType);
	}

	if (FOptionsManager)
	{
		FOptionsManager->insertOptionsDialogHolder(this);
	}

	return true;
}

bool VoiceCalls::initSettings()
{
	Options::setDefaultValue(OPV_VOICECALLS_CONTACTSONLY,true);
	Options::setDefaultValue(OPV_VOICECALLS_STUNSERVER,QString());
	return true;
}

bool VoiceCalls::stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	if (FSHIRequests.value(AStreamJid) == AHandleId)
	{
		AAccept = true;
		processJingleRequest(AStreamJid,AStanza);
		return true;
	}
	else if (FSHIMessages.value(AStreamJid) == AHandleId)
	{
		if (AStanza.type() == "groupchat")
			return false;

		AAccept = true;
		if (AStanza.type() != STANZA_TYPE_ERROR)
			processJingleMessage(AStreamJid,AStanza);
		return true;
	}
	return false;
}

QMultiMap<int, IOptionsDialogWidget *> VoiceCalls::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *> widgets;
	if (FOptionsManager && ANodeId==OPN_MESSAGES)
	{
		widgets.insert(OHO_MESSAGES_VOICECALLS,FOptionsManager->newOptionsDialogHeader(tr("Voice calls"),AParent));
		widgets.insert(OWO_MESSAGES_VOICECALLS_CONTACTSONLY,FOptionsManager->newOptionsDialogWidget(Options::node(OPV_VOICECALLS_CONTACTSONLY),tr("Accept calls only from people in my contact list"),AParent));
		widgets.insert(OWO_MESSAGES_VOICECALLS_STUNSERVER,FOptionsManager->newOptionsDialogWidget(Options::node(OPV_VOICECALLS_STUNSERVER),tr("STUN server to use if my server offers none:"),AParent));
	}
	return widgets;
}

bool VoiceCalls::isAvailable() const
{
	return FAvailable;
}

bool VoiceCalls::isSupported(const Jid &AStreamJid, const Jid &AContactJid) const
{
	if (!FAvailable || !AContactJid.isValid() || !isStreamOpen(AStreamJid))
		return false;
	if (AStreamJid.pBare()==AContactJid.pBare() || isRoomJid(AStreamJid,AContactJid))
		return false;

	QList<IPresenceItem> resources = contactResources(AStreamJid,AContactJid);
	if (resources.isEmpty())
	{
		// A phone that is asleep looks offline, the call wakes it up through its push service
		IRoster *roster = FRosterManager!=NULL ? FRosterManager->findRoster(AStreamJid) : NULL;
		IRosterItem ritem = roster!=NULL ? roster->findItem(AContactJid) : IRosterItem();
		return ritem.subscription==SUBSCRIPTION_BOTH || ritem.subscription==SUBSCRIPTION_TO;
	}

	foreach(const IPresenceItem &pitem, resources)
		if (isCallCapable(AStreamJid,pitem.itemJid,true) || isCallCapable(AStreamJid,pitem.itemJid,false))
			return true;
	return false;
}

bool VoiceCalls::hasCall() const
{
	return !FSessions.isEmpty();
}

bool VoiceCalls::startCall(const Jid &AStreamJid, const Jid &AContactJid)
{
	if (hasCall())
	{
		showCallDialog(FSessions.first(),true);
		return false;
	}

	if (!isSupported(AStreamJid,AContactJid))
	{
		LOG_STRM_WARNING(AStreamJid,QString("Failed to start call to=%1: Not supported").arg(AContactJid.full()));
		return false;
	}

	// A proposal rings on every device of the contact, a session is started with one of them
	QList<IPresenceItem> resources = contactResources(AStreamJid,AContactJid);
	bool propose = resources.isEmpty();
	foreach(const IPresenceItem &pitem, resources)
		propose = propose || isCallCapable(AStreamJid,pitem.itemJid,true);

	Jid target = AContactJid.bare();
	if (!propose)
	{
		target = Jid::null;
		if (AContactJid.hasResource() && isCallCapable(AStreamJid,AContactJid,false))
			target = AContactJid;
		for (int i=0; target.isEmpty() && i<resources.count(); i++)
			if (isCallCapable(AStreamJid,resources.at(i).itemJid,false))
				target = resources.at(i).itemJid;
		if (target.isEmpty())
			return false;
	}

	CallSession *session = createSession(AStreamJid,target,QUuid::createUuid().toString(QUuid::WithoutBraces),true);
	showCallDialog(session,true);
	if (propose)
		session->startProposal();
	else
		session->startSession();

	emit callStarted(AStreamJid,session->contactJid(),session->sessionId());
	updateToolBarActions();
	return true;
}

bool VoiceCalls::isStreamOpen(const Jid &AStreamJid) const
{
	IXmppStream *stream = FXmppStreamManager!=NULL ? FXmppStreamManager->findXmppStream(AStreamJid) : NULL;
	return stream!=NULL && stream->isOpen();
}

bool VoiceCalls::isRoomJid(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return FMultiChatManager!=NULL && FMultiChatManager->findMultiUserChat(AStreamJid,AContactJid.bare())!=NULL;
}

bool VoiceCalls::isRosterContact(const Jid &AStreamJid, const Jid &AContactJid) const
{
	IRoster *roster = FRosterManager!=NULL ? FRosterManager->findRoster(AStreamJid) : NULL;
	return roster==NULL || !roster->findItem(AContactJid).isNull();
}

bool VoiceCalls::isCallCapable(const Jid &AStreamJid, const Jid &AContactJid, bool AProposals) const
{
	if (FDiscovery == NULL)
		return false;

	QStringList features = FDiscovery->discoInfo(AStreamJid,AContactJid).features;
	if (AProposals)
		return features.contains(NS_JINGLE_MESSAGE);

	return features.contains(NS_JINGLE) && features.contains(NS_JINGLE_APPS_RTP) && features.contains(NS_JINGLE_APPS_RTP_AUDIO)
		&& features.contains(NS_JINGLE_TRANSPORTS_ICE_UDP) && features.contains(NS_JINGLE_APPS_DTLS);
}

QList<IPresenceItem> VoiceCalls::contactResources(const Jid &AStreamJid, const Jid &AContactJid) const
{
	QList<IPresenceItem> resources;
	IPresence *presence = FPresenceManager!=NULL ? FPresenceManager->findPresence(AStreamJid) : NULL;
	if (presence)
	{
		foreach(const IPresenceItem &pitem, FPresenceManager->sortPresenceItems(presence->findItems(AContactJid.bare())))
			if (pitem.show!=IPresence::Offline && pitem.show!=IPresence::Error)
				resources.append(pitem);
	}
	return resources;
}

CallSession *VoiceCalls::findSession(const Jid &AStreamJid, const QString &ASessionId) const
{
	foreach(CallSession *session, FSessions)
		if (session->streamJid()==AStreamJid && session->sessionId()==ASessionId)
			return session;
	return NULL;
}

CallSession *VoiceCalls::findContactSession(const Jid &AStreamJid, const Jid &AContactJid) const
{
	foreach(CallSession *session, FSessions)
		if (session->streamJid()==AStreamJid && session->contactJid().pBare()==AContactJid.pBare())
			return session;
	return NULL;
}

CallSession *VoiceCalls::createSession(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, bool AOutgoing)
{
	CallSession *session = new CallSession(FStanzaProcessor,AStreamJid,AContactJid,ASessionId,AOutgoing,this);
	connect(session,SIGNAL(stateChanged()),SLOT(onSessionStateChanged()));
	connect(session,SIGNAL(ended()),SLOT(onSessionEnded()));
	FSessions.append(session);

	session->setFallbackStunServer(Options::node(OPV_VOICECALLS_STUNSERVER).value().toString());
	if (FDiscovery && FDiscovery->discoInfo(AStreamJid,AStreamJid.domain()).features.contains(NS_EXTERNAL_SERVICES))
		session->requestIceServers();

	return session;
}

CallDialog *VoiceCalls::showCallDialog(CallSession *ASession, bool AActivate)
{
	CallDialog *dialog = FDialogs.value(ASession);
	if (dialog == NULL)
	{
		QString name = FNotifications!=NULL ? FNotifications->contactName(ASession->streamJid(),ASession->contactJid()) : ASession->contactJid().uBare();
		QImage avatar = FNotifications!=NULL ? FNotifications->contactAvatar(ASession->contactJid()) : QImage();
		dialog = new CallDialog(ASession,name,avatar);
		connect(dialog,SIGNAL(dialogDestroyed()),SLOT(onCallDialogDestroyed()));
		FDialogs.insert(ASession,dialog);
	}

	if (AActivate)
	{
		WidgetManager::showActivateRaiseWindow(dialog);
	}
	else if (!dialog->isVisible())
	{
		// An incoming call must not take the keyboard from what the user is typing
		dialog->setAttribute(Qt::WA_ShowWithoutActivating,true);
		dialog->show();
		dialog->setAttribute(Qt::WA_ShowWithoutActivating,false);
	}
	return dialog;
}

void VoiceCalls::processJingleRequest(const Jid &AStreamJid, const Stanza &ARequest)
{
	QDomElement jingle = ARequest.firstElement("jingle",NS_JINGLE);
	QString sessionId = jingle.attribute("sid");
	QString action = jingle.attribute("action");
	Jid contactJid = ARequest.fromJid();

	CallSession *session = findSession(AStreamJid,sessionId);
	if (session)
	{
		if (!session->processRequest(ARequest))
			replyJingleError(AStreamJid,ARequest,XmppStanzaError::EC_ITEM_NOT_FOUND,"unknown-session");
	}
	else if (action != "session-initiate")
	{
		replyJingleError(AStreamJid,ARequest,XmppStanzaError::EC_ITEM_NOT_FOUND,"unknown-session");
	}
	else if (!FAvailable || sessionId.isEmpty() || !contactJid.hasResource() || contactJid.pBare()==AStreamJid.pBare() || isRoomJid(AStreamJid,contactJid))
	{
		replyJingleError(AStreamJid,ARequest,XmppStanzaError::EC_SERVICE_UNAVAILABLE);
	}
	else if (Options::node(OPV_VOICECALLS_CONTACTSONLY).value().toBool() && !isRosterContact(AStreamJid,contactJid))
	{
		LOG_STRM_INFO(AStreamJid,QString("Call from unknown contact ignored, from=%1").arg(contactJid.full()));
		replyJingleError(AStreamJid,ARequest,XmppStanzaError::EC_SERVICE_UNAVAILABLE);
	}
	else if (hasCall())
	{
		LOG_STRM_INFO(AStreamJid,QString("Call rejected while another call is in progress, from=%1").arg(contactJid.full()));
		Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
		FStanzaProcessor->sendStanzaOut(AStreamJid,result);

		Stanza terminate(STANZA_KIND_IQ);
		terminate.setType(STANZA_TYPE_SET).setTo(contactJid.full()).setUniqueId();
		QDomElement terminateJingle = terminate.addElement("jingle",NS_JINGLE);
		terminateJingle.setAttribute("action","session-terminate");
		terminateJingle.setAttribute("sid",sessionId);
		terminateJingle.appendChild(terminate.createElement("reason")).appendChild(terminate.createElement("busy"));
		FStanzaProcessor->sendStanzaOut(AStreamJid,terminate);

		notifyMissedCall(AStreamJid,contactJid);
	}
	else
	{
		session = createSession(AStreamJid,contactJid,sessionId,false);
		if (session->receiveSession(ARequest))
		{
			showCallDialog(session,false);
			notifyIncomingCall(session);
			emit callStarted(AStreamJid,contactJid,sessionId);
			updateToolBarActions();
		}
	}
}

void VoiceCalls::processJingleMessage(const Jid &AStreamJid, const Stanza &AMessage)
{
	// A call that was stored while we were away is over long ago
	if (!AMessage.firstElement("delay",NS_XMPP_DELAY).isNull())
		return;

	QDomElement elem = AMessage.firstElement(QString(),NS_JINGLE_MESSAGE);
	QString sessionId = elem.attribute("id");
	Jid contactJid = AMessage.fromJid();
	if (elem.isNull() || sessionId.isEmpty() || !contactJid.isValid() || contactJid==AStreamJid)
		return;

	bool fromSelf = contactJid.pBare()==AStreamJid.pBare();
	if (elem.tagName() == "propose")
	{
		if (!fromSelf)
			processCallProposal(AStreamJid,contactJid,sessionId,elem);
	}
	else
	{
		CallSession *session = findSession(AStreamJid,sessionId);
		if (session!=NULL && (fromSelf || contactJid.pBare()==session->contactJid().pBare()))
			session->processMessage(contactJid,elem,fromSelf);
	}
}

void VoiceCalls::processCallProposal(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, const QDomElement &AProposal)
{
	if (!FAvailable || !AContactJid.hasResource() || isRoomJid(AStreamJid,AContactJid) || findSession(AStreamJid,ASessionId)!=NULL)
		return;

	// Calls with video are left to the devices that can show it
	bool audio = false;
	for (QDomElement descElem = AProposal.firstChildElement("description"); !descElem.isNull(); descElem = descElem.nextSiblingElement("description"))
	{
		if (descElem.namespaceURI()!=NS_JINGLE_APPS_RTP || descElem.attribute("media")!="audio")
			return;
		audio = true;
	}
	if (!audio)
		return;

	if (Options::node(OPV_VOICECALLS_CONTACTSONLY).value().toBool() && !isRosterContact(AStreamJid,AContactJid))
	{
		LOG_STRM_INFO(AStreamJid,QString("Call from unknown contact ignored, from=%1").arg(AContactJid.full()));
		return;
	}

	if (hasCall())
	{
		// Not rejected: another device of this account may still take it
		LOG_STRM_INFO(AStreamJid,QString("Call ignored while another call is in progress, from=%1").arg(AContactJid.full()));
		showCallEvent(AStreamJid,AContactJid,tr("Incoming call was not answered: another call was in progress"));
		return;
	}

	CallSession *session = createSession(AStreamJid,AContactJid,ASessionId,false);
	session->receiveProposal();
	showCallDialog(session,false);
	notifyIncomingCall(session);
	emit callStarted(AStreamJid,AContactJid,ASessionId);
	updateToolBarActions();
}

void VoiceCalls::replyJingleError(const Jid &AStreamJid, const Stanza &ARequest, XmppStanzaError::ErrorCondition ACondition, const QString &AJingleCondition)
{
	XmppStanzaError err(ACondition);
	if (!AJingleCondition.isEmpty())
		err.setAppCondition(NS_JINGLE_ERRORS,AJingleCondition);
	Stanza error = FStanzaProcessor->makeReplyError(ARequest,err);
	FStanzaProcessor->sendStanzaOut(AStreamJid,error);
}

void VoiceCalls::notifyIncomingCall(CallSession *ASession)
{
	bool ring = true;
	if (FNotifications)
	{
		INotification notify;
		notify.kinds = FNotifications->enabledTypeNotificationKinds(NNT_VOICE_CALL);

		// The ringing is ours: it has to repeat and to stop when the call is answered
		ring = (notify.kinds & INotification::SoundPlay)>0;
		notify.kinds &= ~INotification::SoundPlay;

		if (notify.kinds > 0)
		{
			QString name = FNotifications->contactName(ASession->streamJid(),ASession->contactJid());
			CallDialog *dialog = FDialogs.value(ASession);

			notify.typeId = NNT_VOICE_CALL;
			notify.data.insert(NDR_ICON,IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_VOICECALLS_CALL));
			notify.data.insert(NDR_TOOLTIP,tr("Incoming call from %1").arg(name));
			notify.data.insert(NDR_STREAM_JID,ASession->streamJid().full());
			notify.data.insert(NDR_CONTACT_JID,ASession->contactJid().full());
			notify.data.insert(NDR_POPUP_CAPTION,tr("Incoming call"));
			notify.data.insert(NDR_POPUP_TITLE,name);
			notify.data.insert(NDR_POPUP_IMAGE,FNotifications->contactAvatar(ASession->contactJid()));
			notify.data.insert(NDR_POPUP_TEXT,tr("%1 is calling you").arg(name));
			notify.data.insert(NDR_ALERT_WIDGET,(qint64)dialog);
			FCallNotifies.insert(ASession,FNotifications->appendNotification(notify));
		}
	}
	if (ring)
		startRinging(ASession->streamJid());
}

void VoiceCalls::notifyMissedCall(const Jid &AStreamJid, const Jid &AContactJid)
{
	if (FNotifications)
	{
		INotification notify;
		notify.kinds = FNotifications->enabledTypeNotificationKinds(NNT_VOICE_CALL);
		notify.kinds &= INotification::RosterNotify|INotification::PopupWindow|INotification::TrayNotify|INotification::TrayAction;
		if (notify.kinds > 0)
		{
			QString name = FNotifications->contactName(AStreamJid,AContactJid);
			notify.typeId = NNT_VOICE_CALL;
			notify.data.insert(NDR_ICON,IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_VOICECALLS_CALL));
			notify.data.insert(NDR_TOOLTIP,tr("Missed call from %1").arg(name));
			notify.data.insert(NDR_STREAM_JID,AStreamJid.full());
			notify.data.insert(NDR_CONTACT_JID,AContactJid.full());
			notify.data.insert(NDR_POPUP_CAPTION,tr("Missed call"));
			notify.data.insert(NDR_POPUP_TITLE,name);
			notify.data.insert(NDR_POPUP_IMAGE,FNotifications->contactAvatar(AContactJid));
			notify.data.insert(NDR_POPUP_TEXT,tr("%1 tried to call you").arg(name));
			FMissedNotifies.insert(FNotifications->appendNotification(notify),qMakePair(AStreamJid,AContactJid));
		}
	}
}

void VoiceCalls::removeCallNotify(CallSession *ASession)
{
	if (FNotifications && FCallNotifies.contains(ASession))
		FNotifications->removeNotification(FCallNotifies.take(ASession));
}

void VoiceCalls::startRinging(const Jid &AStreamJid)
{
	// Same rules as for the sounds of other notifications
	IPresence *presence = FPresenceManager!=NULL ? FPresenceManager->findPresence(AStreamJid) : NULL;
	if (presence)
	{
		if (presence->show()==IPresence::DoNotDisturb && Options::node(OPV_NOTIFICATIONS_SILENTIFDND).value().toBool())
			return;
		if (presence->show()==IPresence::Away && Options::node(OPV_NOTIFICATIONS_SILENTIFAWAY).value().toBool())
			return;
	}

	QString soundFile = FileStorage::staticStorage(RSR_STORAGE_SOUNDS)->fileFullName(SDF_VOICECALLS_INCOMING);
	if (!soundFile.isEmpty())
	{
		if (FRingSound == NULL)
			FRingSound = new QSoundEffect(this);
		FRingSound->setSource(QUrl::fromLocalFile(soundFile));
		FRingSound->setLoopCount(QSoundEffect::Infinite);
		FRingSound->play();
	}
}

void VoiceCalls::stopRinging()
{
	if (FRingSound)
		FRingSound->stop();
}

void VoiceCalls::showCallEvent(const Jid &AStreamJid, const Jid &AContactJid, const QString &AText)
{
	IMessageChatWindow *window = FMessageWidgets!=NULL ? FMessageWidgets->findChatWindow(AStreamJid,AContactJid) : NULL;
	if (window!=NULL && !AText.isEmpty())
	{
		IMessageStyleContentOptions options;
		options.kind = IMessageStyleContentOptions::KindStatus;
		options.type |= IMessageStyleContentOptions::TypeEvent;
		options.direction = IMessageStyleContentOptions::DirectionIn;
		options.time = QDateTime::currentDateTime();
		window->viewWidget()->appendHtml(AText.toHtmlEscaped(),options);
	}
}

void VoiceCalls::updateToolBarAction(IMessageToolBarWidget *AWidget)
{
	IMessageChatWindow *window = qobject_cast<IMessageChatWindow *>(AWidget->messageWindow()->instance());
	if (window == NULL)
		return;

	Jid streamJid = window->streamJid();
	Jid contactJid = window->contactJid();

	Action *action = FToolBarActions.value(AWidget);
	if (action == NULL)
	{
		if (isRoomJid(streamJid,contactJid))
			return;

		action = new Action(AWidget->toolBarChanger()->toolBar());
		action->setShortcutId(SCT_MESSAGEWINDOWS_VOICECALL);
		connect(action,SIGNAL(triggered(bool)),SLOT(onCallActionTriggered(bool)));
		AWidget->toolBarChanger()->insertAction(action,TBG_MWTBW_VOICECALLS);
		FToolBarActions.insert(AWidget,action);
	}

	action->setData(ADR_STREAM_JID,streamJid.full());
	action->setData(ADR_CONTACT_JID,contactJid.full());

	if (findContactSession(streamJid,contactJid) != NULL)
	{
		action->setText(tr("Show Call"));
		action->setIcon(RSR_STORAGE_MENUICONS,MNI_VOICECALLS_HANGUP);
		action->setEnabled(true);
	}
	else
	{
		action->setText(FAvailable ? tr("Call") : tr("Call (GStreamer WebRTC components are not installed)"));
		action->setIcon(RSR_STORAGE_MENUICONS,MNI_VOICECALLS_CALL);
		action->setEnabled(!hasCall() && isSupported(streamJid,contactJid));
	}
}

void VoiceCalls::updateToolBarActions()
{
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		updateToolBarAction(widget);
}

void VoiceCalls::onXmppStreamOpened(IXmppStream *AXmppStream)
{
	IStanzaHandle requestHandle;
	requestHandle.handler = this;
	requestHandle.order = SHO_DEFAULT;
	requestHandle.direction = IStanzaHandle::DirectionIn;
	requestHandle.streamJid = AXmppStream->streamJid();
	requestHandle.conditions.append(SHC_JINGLE_REQUEST);
	FSHIRequests.insert(requestHandle.streamJid,FStanzaProcessor->insertStanzaHandle(requestHandle));

	IStanzaHandle messageHandle;
	messageHandle.handler = this;
	messageHandle.order = SHO_MI_VOICECALLS;
	messageHandle.direction = IStanzaHandle::DirectionIn;
	messageHandle.streamJid = AXmppStream->streamJid();
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("propose"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("ringing"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("proceed"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("accept"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("reject"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("retract"));
	messageHandle.conditions.append(SHC_JINGLE_MESSAGE("finish"));
	FSHIMessages.insert(messageHandle.streamJid,FStanzaProcessor->insertStanzaHandle(messageHandle));

	updateToolBarActions();
}

void VoiceCalls::onXmppStreamClosed(IXmppStream *AXmppStream)
{
	Jid streamJid = AXmppStream->streamJid();
	if (FSHIRequests.contains(streamJid))
		FStanzaProcessor->removeStanzaHandle(FSHIRequests.take(streamJid));
	if (FSHIMessages.contains(streamJid))
		FStanzaProcessor->removeStanzaHandle(FSHIMessages.take(streamJid));

	foreach(CallSession *session, FSessions)
		if (session->streamJid() == streamJid)
			session->abort(CallSession::Failed,tr("Connection to the server is lost"));

	updateToolBarActions();
}

void VoiceCalls::onShutdownStarted()
{
	foreach(CallSession *session, FSessions)
		session->hangup();
}

void VoiceCalls::onCarbonMessageSent(const Jid &AStreamJid, const Message &AMessage)
{
	// Another device of this account has answered a call that is ringing here too
	QDomElement elem = AMessage.stanza().firstElement(QString(),NS_JINGLE_MESSAGE);
	if (!elem.isNull() && elem.tagName()!="propose")
	{
		CallSession *session = findSession(AStreamJid,elem.attribute("id"));
		if (session)
			session->processMessage(AStreamJid.bare(),elem,true);
	}
}

void VoiceCalls::onDiscoInfoChanged(const IDiscoInfo &AInfo)
{
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		if (AInfo.contactJid.pBare()==widget->messageWindow()->contactJid().pBare() || AInfo.contactJid==AInfo.streamJid.domain())
			updateToolBarAction(widget);
}

void VoiceCalls::onPresenceItemReceived(IPresence *APresence, const IPresenceItem &AItem, const IPresenceItem &ABefore)
{
	Q_UNUSED(APresence);
	if (AItem.show != ABefore.show)
	{
		foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
			if (AItem.itemJid.pBare() == widget->messageWindow()->contactJid().pBare())
				updateToolBarAction(widget);
	}
}

void VoiceCalls::onSessionStateChanged()
{
	CallSession *session = qobject_cast<CallSession *>(sender());
	if (session && session->state()!=CallSession::Ringing)
	{
		removeCallNotify(session);
		stopRinging();
	}
}

void VoiceCalls::onSessionEnded()
{
	CallSession *session = qobject_cast<CallSession *>(sender());
	if (session && FSessions.contains(session))
	{
		removeCallNotify(session);
		stopRinging();

		Jid streamJid = session->streamJid();
		Jid contactJid = session->contactJid();

		if (session->endReason() == CallSession::RemoteCancel)
		{
			showCallEvent(streamJid,contactJid,tr("Missed call"));
			notifyMissedCall(streamJid,contactJid);
		}
		else
		{
			showCallEvent(streamJid,contactJid,QString("%1: %2").arg(session->isOutgoing() ? tr("Outgoing call") : tr("Incoming call"),CallDialog::stateText(session)));
		}

		// The window keeps showing how the call ended
		FSessions.removeAll(session);
		FDialogs.remove(session);
		session->deleteLater();

		emit callFinished(streamJid,contactJid,session->sessionId());
		updateToolBarActions();
	}
}

void VoiceCalls::onCallDialogDestroyed()
{
	QObject *dialog = sender();
	for (QMap<CallSession *, CallDialog *>::iterator it=FDialogs.begin(); it!=FDialogs.end(); )
	{
		if (static_cast<QObject *>(it.value()) == dialog)
			it = FDialogs.erase(it);
		else
			++it;
	}
}

void VoiceCalls::onNotificationActivated(int ANotifyId)
{
	if (FMissedNotifies.contains(ANotifyId))
	{
		QPair<Jid, Jid> address = FMissedNotifies.value(ANotifyId);
		if (FMessageProcessor)
			FMessageProcessor->getMessageWindow(address.first,address.second,Message::Chat,IMessageProcessor::ActionShowNormal);
		FNotifications->removeNotification(ANotifyId);
	}
	else
	{
		CallSession *session = FCallNotifies.key(ANotifyId,NULL);
		if (session)
		{
			showCallDialog(session,true);
			FNotifications->removeNotification(ANotifyId);
		}
	}
}

void VoiceCalls::onNotificationRemoved(int ANotifyId)
{
	FMissedNotifies.remove(ANotifyId);
	FCallNotifies.remove(FCallNotifies.key(ANotifyId,NULL));
}

void VoiceCalls::onToolBarWidgetCreated(IMessageToolBarWidget *AWidget)
{
	if (qobject_cast<IMessageChatWindow *>(AWidget->messageWindow()->instance()) != NULL)
	{
		connect(AWidget->messageWindow()->address()->instance(),SIGNAL(addressChanged(const Jid &,const Jid &)),SLOT(onToolBarWidgetAddressChanged(const Jid &,const Jid &)));
		connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onToolBarWidgetDestroyed(QObject *)));
		updateToolBarAction(AWidget);
	}
}

void VoiceCalls::onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore)
{
	Q_UNUSED(AStreamBefore); Q_UNUSED(AContactBefore);
	IMessageAddress *address = qobject_cast<IMessageAddress *>(sender());
	if (address)
	{
		foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
			if (widget->messageWindow()->address() == address)
				updateToolBarAction(widget);
	}
}

void VoiceCalls::onToolBarWidgetDestroyed(QObject *AObject)
{
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		if (qobject_cast<QObject *>(widget->instance()) == AObject)
			FToolBarActions.remove(widget);
}

void VoiceCalls::onRostersViewIndexContextMenu(const QList<IRosterIndex *> &AIndexes, quint32 ALabelId, Menu *AMenu)
{
	if (ALabelId==AdvancedDelegateItem::DisplayId && AIndexes.count()==1)
	{
		IRosterIndex *index = AIndexes.first();
		if (index->kind()==RIK_CONTACT || index->kind()==RIK_METACONTACT_ITEM)
		{
			Jid streamJid = index->data(RDR_STREAM_JID).toString();
			Jid contactJid = index->data(RDR_FULL_JID).toString();
			if (isSupported(streamJid,contactJid))
			{
				bool inCall = findContactSession(streamJid,contactJid)!=NULL;
				Action *action = new Action(AMenu);
				action->setText(inCall ? tr("Show Call") : tr("Call"));
				action->setIcon(RSR_STORAGE_MENUICONS,inCall ? MNI_VOICECALLS_HANGUP : MNI_VOICECALLS_CALL);
				action->setData(ADR_STREAM_JID,streamJid.full());
				action->setData(ADR_CONTACT_JID,contactJid.full());
				action->setEnabled(inCall || !hasCall());
				connect(action,SIGNAL(triggered(bool)),SLOT(onCallActionTriggered(bool)));
				AMenu->addAction(action,AG_RVCM_VOICECALLS_CALL,true);
			}
		}
	}
}

void VoiceCalls::onCallActionTriggered(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		Jid streamJid = action->data(ADR_STREAM_JID).toString();
		Jid contactJid = action->data(ADR_CONTACT_JID).toString();

		CallSession *session = findContactSession(streamJid,contactJid);
		if (session)
			showCallDialog(session,true);
		else
			startCall(streamJid,contactJid);
	}
}
