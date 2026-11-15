#include "callsession.h"

#include <definitions/namespaces.h>
#include <utils/xmpperror.h>
#include <utils/logger.h>
#include "jinglesdp.h"

#define JINGLE_TIMEOUT            30000
#define SERVICES_TIMEOUT          5000

#define PROPOSING_TIMEOUT         60000
#define RINGING_TIMEOUT           120000
#define PROCEEDING_TIMEOUT        30000
#define INITIATING_TIMEOUT        60000
#define CONNECTING_TIMEOUT        30000
#define RECONNECT_TIMEOUT         15000
#define CANDIDATES_DELAY          100

#define MAX_ICE_SERVERS           8
#define MAX_PENDING_CANDIDATES    100
#define STUN_DEFAULT_PORT         3478

CallSession::CallSession(IStanzaProcessor *AStanzaProcessor, const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, bool AOutgoing, QObject *AParent) : QObject(AParent)
{
	FStanzaProcessor = AStanzaProcessor;

	FStreamJid = AStreamJid;
	FContactJid = AContactJid;
	FSessionId = ASessionId;
	FOutgoing = AOutgoing;
	FProposed = false;
	FState = AOutgoing ? Proposing : Ringing;
	FEndReason = NoReason;
	FRemoteRinging = false;
	FAnswered = false;
	FAccepted = false;
	FMuted = false;

	FMedia = NULL;
	FMediaWaiting = false;
	FDescriptionSent = false;
	FHasOffer = false;
	FServicesReady = true;

	FStateTimer.setSingleShot(true);
	connect(&FStateTimer,SIGNAL(timeout()),SLOT(onStateTimeout()));

	FCandidatesTimer.setSingleShot(true);
	connect(&FCandidatesTimer,SIGNAL(timeout()),SLOT(onCandidatesTimeout()));

	FReconnectTimer.setSingleShot(true);
	connect(&FReconnectTimer,SIGNAL(timeout()),SLOT(onReconnectTimeout()));
}

CallSession::~CallSession()
{
	if (FMedia)
		FMedia->stop();
}

void CallSession::stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza)
{
	Q_UNUSED(AStreamJid);
	if (AStanza.id() == FServicesRequest)
	{
		FServicesRequest.clear();
		if (AStanza.isResult())
			parseIceServers(AStanza.firstElement("services",NS_EXTERNAL_SERVICES));
		else
			LOG_STRM_WARNING(FStreamJid,QString("Failed to load external services for call, sid=%1: %2").arg(FSessionId,XmppStanzaError(AStanza).condition()));

		FServicesReady = true;
		if (FMediaWaiting && FState!=Ended)
			startMedia();
	}
	else if (FRequests.contains(AStanza.id()))
	{
		QString action = FRequests.take(AStanza.id());
		if (AStanza.isError() && FState!=Ended)
		{
			XmppStanzaError err(AStanza);
			LOG_STRM_WARNING(FStreamJid,QString("Jingle request rejected, sid=%1, action=%2: %3").arg(FSessionId,action,err.condition()));
			if (action=="session-initiate" || action=="session-accept")
				end(Failed,err.errorMessage());
		}
	}
}

Jid CallSession::streamJid() const
{
	return FStreamJid;
}

Jid CallSession::contactJid() const
{
	return FContactJid;
}

QString CallSession::sessionId() const
{
	return FSessionId;
}

bool CallSession::isOutgoing() const
{
	return FOutgoing;
}

CallSession::State CallSession::state() const
{
	return FState;
}

CallSession::EndReason CallSession::endReason() const
{
	return FEndReason;
}

QString CallSession::errorString() const
{
	return FError;
}

bool CallSession::isRemoteRinging() const
{
	return FRemoteRinging;
}

bool CallSession::isAnswered() const
{
	return FAnswered;
}

bool CallSession::wasActive() const
{
	return FActiveTime.isValid();
}

int CallSession::duration() const
{
	if (!FActiveTime.isValid())
		return 0;
	return FActiveTime.secsTo(FEndTime.isValid() ? FEndTime : QDateTime::currentDateTime());
}

bool CallSession::isMuted() const
{
	return FMuted;
}

void CallSession::setMuted(bool AMuted)
{
	if (FMuted != AMuted)
	{
		FMuted = AMuted;
		if (FMedia)
			FMedia->setMuted(AMuted);
		emit muteChanged();
	}
}

void CallSession::requestIceServers()
{
	Stanza request(STANZA_KIND_IQ);
	request.setType(STANZA_TYPE_GET).setTo(FStreamJid.domain()).setUniqueId();
	request.addElement("services",NS_EXTERNAL_SERVICES);
	if (FStanzaProcessor->sendStanzaRequest(this,FStreamJid,request,SERVICES_TIMEOUT))
	{
		FServicesReady = false;
		FServicesRequest = request.id();
	}
	else
	{
		LOG_STRM_WARNING(FStreamJid,QString("Failed to send external services request for call, sid=%1").arg(FSessionId));
	}
}

void CallSession::setFallbackStunServer(const QString &AServer)
{
	FFallbackStun = AServer.trimmed();
}

void CallSession::startProposal()
{
	if (FOutgoing && FState==Proposing && !FProposed)
	{
		LOG_STRM_INFO(FStreamJid,QString("Proposing call to=%1, sid=%2").arg(FContactJid.bare(),FSessionId));
		FProposed = true;
		sendJingleMessage("propose",FContactJid.bare());
		setState(Proposing);
	}
}

void CallSession::startSession()
{
	if (FOutgoing && FState==Proposing && !FProposed)
	{
		LOG_STRM_INFO(FStreamJid,QString("Starting call session with=%1, sid=%2").arg(FContactJid.full(),FSessionId));
		setState(Initiating);
		startMedia();
	}
}

void CallSession::receiveProposal()
{
	if (!FOutgoing && FState==Ringing && !FProposed)
	{
		LOG_STRM_INFO(FStreamJid,QString("Call proposed by=%1, sid=%2").arg(FContactJid.full(),FSessionId));
		FProposed = true;
		sendJingleMessage("ringing",FContactJid);
		setState(Ringing);
	}
}

bool CallSession::receiveSession(const Stanza &ARequest)
{
	if (FOutgoing || FHasOffer || FState==Ended)
		return false;

	Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
	FStanzaProcessor->sendStanzaOut(FStreamJid,result);

	FHasOffer = true;
	FSessionOffer = ARequest;

	QString reason, error;
	if (!acceptSessionOffer(ARequest,reason,error))
	{
		LOG_STRM_WARNING(FStreamJid,QString("Call session offer rejected, from=%1, sid=%2: %3").arg(FContactJid.full(),FSessionId,error));
		terminate(reason,Failed,error);
		return false;
	}

	LOG_STRM_INFO(FStreamJid,QString("Call session offered by=%1, sid=%2").arg(FContactJid.full(),FSessionId));
	if (FAccepted)
	{
		setState(Connecting);
		startMedia();
	}
	else
	{
		// Tell the caller that the user is being alerted
		QDomElement jingle;
		Stanza info = makeJingleRequest("session-info",jingle);
		jingle.appendChild(info.createElement("ringing",NS_JINGLE_APPS_RTP_INFO));
		sendJingleRequest(info);
		setState(Ringing);
	}
	return true;
}

void CallSession::accept()
{
	if (!FOutgoing && FState==Ringing && !FAccepted)
	{
		LOG_STRM_INFO(FStreamJid,QString("Call accepted, from=%1, sid=%2").arg(FContactJid.full(),FSessionId));
		FAccepted = true;
		FAnswered = true;
		if (FHasOffer)
		{
			setState(Connecting);
			startMedia();
		}
		else
		{
			// Other devices of this account stop ringing, the caller starts the session with this one
			sendJingleMessage("accept",FStreamJid.bare());
			sendJingleMessage("proceed",FContactJid);
			setState(Proceeding);
		}
	}
}

void CallSession::decline()
{
	if (!FOutgoing && FState==Ringing)
	{
		LOG_STRM_INFO(FStreamJid,QString("Call declined, from=%1, sid=%2").arg(FContactJid.full(),FSessionId));
		if (FHasOffer)
		{
			terminate("decline",LocalDecline);
		}
		else
		{
			sendJingleMessage("reject",FStreamJid.bare());
			sendJingleMessage("reject",FContactJid);
			end(LocalDecline);
		}
	}
}

void CallSession::hangup()
{
	switch (FState)
	{
	case Proposing:
		sendJingleMessage("retract",FContactJid.bare());
		end(LocalHangup);
		break;
	case Ringing:
		decline();
		break;
	case Proceeding:
		sendJingleMessage("reject",FContactJid);
		end(LocalHangup);
		break;
	case Initiating:
		if (FDescriptionSent)
		{
			terminate("cancel",LocalHangup);
		}
		else
		{
			if (FProposed)
				sendJingleMessage("retract",FContactJid);
			end(LocalHangup);
		}
		break;
	case Connecting:
	case Active:
		terminate("success",LocalHangup);
		break;
	default:
		break;
	}
}

void CallSession::processMessage(const Jid &AFrom, const QDomElement &AElem, bool AFromSelf)
{
	if (FState == Ended)
		return;

	QString action = AElem.tagName();
	if (AFromSelf)
	{
		// Another device of this account has answered the call
		if (!FOutgoing && FState==Ringing && !FAccepted)
		{
			if (action=="accept" || action=="proceed")
				end(AnsweredElsewhere);
			else if (action == "reject")
				end(DeclinedElsewhere);
		}
	}
	else if (FOutgoing)
	{
		if (FState == Proposing)
		{
			if (action == "ringing")
			{
				if (!FRemoteRinging)
				{
					FRemoteRinging = true;
					emit stateChanged();
				}
			}
			else if (action=="proceed" && AFrom.hasResource())
			{
				LOG_STRM_INFO(FStreamJid,QString("Call taken by=%1, sid=%2").arg(AFrom.full(),FSessionId));
				FContactJid = AFrom;
				FAnswered = true;
				setState(Initiating);
				startMedia();
			}
			else if (action == "reject")
			{
				end(RemoteDecline);
			}
		}
	}
	else if (AFrom==FContactJid && !FHasOffer)
	{
		if (action=="retract" || action=="finish")
			end(RemoteCancel);
	}
}

bool CallSession::processRequest(const Stanza &ARequest)
{
	if (FState==Ended || ARequest.fromJid()!=FContactJid)
		return false;

	QDomElement jingle = ARequest.firstElement("jingle",NS_JINGLE);
	QString action = jingle.attribute("action");

	bool outOfOrder = false;
	if (action == "session-initiate")
	{
		if (!FOutgoing && !FHasOffer)
			receiveSession(ARequest);
		else
			outOfOrder = true;
		if (!outOfOrder)
			return true;
	}
	else if (action == "session-accept")
	{
		if (FOutgoing && FState==Initiating && FDescriptionSent)
		{
			Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
			FStanzaProcessor->sendStanzaOut(FStreamJid,result);
			applySessionAccept(jingle);
			return true;
		}
		outOfOrder = true;
	}
	else if (action == "session-terminate")
	{
		Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
		FStanzaProcessor->sendStanzaOut(FStreamJid,result);

		QDomElement reasonElem = jingle.firstChildElement("reason");
		QString reason = reasonElem.firstChildElement().tagName();
		QString text = reasonElem.firstChildElement("text").text();
		if (reason == "text")
			reason = reasonElem.firstChildElement().nextSiblingElement().tagName();

		LOG_STRM_INFO(FStreamJid,QString("Call terminated by=%1, sid=%2, reason=%3").arg(FContactJid.full(),FSessionId,reason));
		EndReason endReason = endReasonFromJingle(reason,FState,FOutgoing);
		end(endReason,endReason==Failed ? (text.isEmpty() ? reason : text) : QString());
		return true;
	}
	else if (action == "transport-info")
	{
		if (hasJingleSession())
		{
			Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
			FStanzaProcessor->sendStanzaOut(FStreamJid,result);
			applyTransportInfo(jingle);
			return true;
		}
		outOfOrder = true;
	}
	else if (action == "session-info")
	{
		Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
		FStanzaProcessor->sendStanzaOut(FStreamJid,result);

		if (FOutgoing && !FRemoteRinging && !jingle.firstChildElement("ringing").isNull() && (FState==Initiating || FState==Proposing))
		{
			FRemoteRinging = true;
			emit stateChanged();
		}
		return true;
	}
	else if (action == "content-add")
	{
		// Only the audio stream the call started with is supported
		Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
		FStanzaProcessor->sendStanzaOut(FStreamJid,result);

		QDomElement rejectJingle;
		Stanza reject = makeJingleRequest("content-reject",rejectJingle);
		for (QDomElement contentElem = jingle.firstChildElement("content"); !contentElem.isNull(); contentElem = contentElem.nextSiblingElement("content"))
		{
			QDomElement rejectElem = rejectJingle.appendChild(reject.createElement("content")).toElement();
			rejectElem.setAttribute("creator",contentElem.attribute("creator"));
			rejectElem.setAttribute("name",contentElem.attribute("name"));
		}
		rejectJingle.appendChild(reject.createElement("reason")).appendChild(reject.createElement("unsupported-applications"));
		sendJingleRequest(reject);
		return true;
	}

	XmppStanzaError err(outOfOrder ? XmppStanzaError::EC_UNEXPECTED_REQUEST : XmppStanzaError::EC_FEATURE_NOT_IMPLEMENTED);
	if (outOfOrder)
		err.setAppCondition(NS_JINGLE_ERRORS,"out-of-order");
	Stanza error = FStanzaProcessor->makeReplyError(ARequest,err);
	FStanzaProcessor->sendStanzaOut(FStreamJid,error);
	return true;
}

void CallSession::abort(EndReason AReason, const QString &AError)
{
	end(AReason,AError);
}

void CallSession::setState(State AState)
{
	FState = AState;

	int timeout = 0;
	switch (AState)
	{
	case Proposing:
		timeout = PROPOSING_TIMEOUT;
		break;
	case Ringing:
		timeout = RINGING_TIMEOUT;
		break;
	case Proceeding:
		timeout = PROCEEDING_TIMEOUT;
		break;
	case Initiating:
		timeout = INITIATING_TIMEOUT;
		break;
	case Connecting:
		timeout = CONNECTING_TIMEOUT;
		break;
	default:
		break;
	}

	if (timeout > 0)
		FStateTimer.start(timeout);
	else
		FStateTimer.stop();

	emit stateChanged();
}

void CallSession::end(EndReason AReason, const QString &AError)
{
	if (FState != Ended)
	{
		LOG_STRM_INFO(FStreamJid,QString("Call ended, with=%1, sid=%2, reason=%3, duration=%4").arg(FContactJid.full(),FSessionId).arg(AReason).arg(duration()));

		FStateTimer.stop();
		FCandidatesTimer.stop();
		FReconnectTimer.stop();
		FMediaWaiting = false;

		if (FMedia)
		{
			// May be called from a signal of the media session, so it is destroyed later
			FMedia->disconnect(this);
			FMedia->stop();
			FMedia->deleteLater();
			FMedia = NULL;
		}

		FState = Ended;
		FEndReason = AReason;
		FError = AError;
		FEndTime = QDateTime::currentDateTime();

		emit stateChanged();
		emit ended();
	}
}

void CallSession::terminate(const QString &AJingleReason, EndReason AReason, const QString &AError)
{
	if (FState!=Ended && hasJingleSession())
	{
		QDomElement jingle;
		Stanza request = makeJingleRequest("session-terminate",jingle);
		QDomElement reasonElem = jingle.appendChild(request.createElement("reason")).toElement();
		reasonElem.appendChild(request.createElement(AJingleReason));
		sendJingleRequest(request);
	}
	end(AReason,AError);
}

bool CallSession::hasJingleSession() const
{
	return FHasOffer || FDescriptionSent;
}

void CallSession::startMedia()
{
	if (FMedia!=NULL || FState==Ended)
		return;

	if (!FServicesReady)
	{
		FMediaWaiting = true;
		return;
	}
	FMediaWaiting = false;

	QList<MediaIceServer> servers = FIceServers;

	bool hasStun = false;
	foreach(const MediaIceServer &server, servers)
		hasStun = hasStun || server.type=="stun";
	if (!hasStun && !FFallbackStun.isEmpty())
	{
		QString address = FFallbackStun;
		if (address.startsWith("stun://"))
			address.remove(0,7);
		else if (address.startsWith("stun:"))
			address.remove(0,5);

		MediaIceServer server;
		server.type = "stun";
		server.transport = "udp";
		server.port = STUN_DEFAULT_PORT;

		int colon = address.lastIndexOf(':');
		bool bracketed = address.startsWith('[');
		if (colon>0 && address.count(':')==1)
		{
			server.host = address.left(colon);
			server.port = address.mid(colon+1).toInt();
		}
		else if (bracketed && address.contains("]:"))
		{
			server.host = address.mid(1,address.indexOf("]:")-1);
			server.port = address.mid(address.indexOf("]:")+2).toInt();
		}
		else
		{
			server.host = bracketed ? address.mid(1,address.length()-2) : address;
		}

		if (!server.host.isEmpty() && server.port>0 && server.port<65536)
			servers.append(server);
	}

	int opusPayload = 0;
	QString remoteSdp;
	if (!FOutgoing)
	{
		QString error;
		QDomElement jingle = FSessionOffer.firstElement("jingle",NS_JINGLE);
		remoteSdp = JingleSdp::jingleToSdp(jingle,true,FContentNames,&error);
		if (remoteSdp.isEmpty())
		{
			terminate("failed-application",Failed,error);
			return;
		}
		opusPayload = JingleSdp::findPayloadType(jingle.firstChildElement("content"),"opus",48000);
	}

	FMedia = new MediaSession(this);
	connect(FMedia,SIGNAL(localDescriptionCreated(const QString &, const QString &)),SLOT(onMediaLocalDescription(const QString &, const QString &)));
	connect(FMedia,SIGNAL(localCandidateFound(int, const QString &)),SLOT(onMediaLocalCandidate(int, const QString &)));
	connect(FMedia,SIGNAL(connected()),SLOT(onMediaConnected()));
	connect(FMedia,SIGNAL(disconnected()),SLOT(onMediaDisconnected()));
	connect(FMedia,SIGNAL(failed(const QString &)),SLOT(onMediaFailed(const QString &)));
	connect(FMedia,SIGNAL(connectionFailed()),SLOT(onMediaConnectionFailed()));
	FMedia->setMuted(FMuted);

	LOG_STRM_INFO(FStreamJid,QString("Starting call media, sid=%1, servers=%2").arg(FSessionId).arg(servers.count()));
	if (!FMedia->start(FOutgoing ? MediaSession::Offerer : MediaSession::Answerer,servers,opusPayload))
	{
		QString error = FMedia->errorString();
		LOG_STRM_ERROR(FStreamJid,QString("Failed to start call media, sid=%1: %2").arg(FSessionId,error));
		terminate("failed-application",Failed,error);
		return;
	}

	if (!FOutgoing)
	{
		if (!FMedia->setRemoteDescription("offer",remoteSdp))
		{
			terminate("failed-application",Failed,FMedia->errorString());
			return;
		}
		for (int i=0; i<FRemoteCandidates.count(); i++)
			FMedia->addRemoteCandidate(FRemoteCandidates.at(i).first,FRemoteCandidates.at(i).second);
		FRemoteCandidates.clear();
	}
}

void CallSession::sendJingleMessage(const QString &AAction, const Jid &ATo)
{
	Stanza message(STANZA_KIND_MESSAGE);
	message.setType("chat").setTo(ATo.full()).setUniqueId();

	QDomElement elem = message.addElement(AAction,NS_JINGLE_MESSAGE);
	elem.setAttribute("id",FSessionId);
	if (AAction == "propose")
		elem.appendChild(message.createElement("description",NS_JINGLE_APPS_RTP)).toElement().setAttribute("media","audio");

	// Lets the server keep the message for devices that are woken up by a push notification
	message.addElement("store",NS_MESSAGE_HINTS);

	if (!FStanzaProcessor->sendStanzaOut(FStreamJid,message))
		LOG_STRM_WARNING(FStreamJid,QString("Failed to send call message, sid=%1, action=%2").arg(FSessionId,AAction));
}

Stanza CallSession::makeJingleRequest(const QString &AAction, QDomElement &AJingle)
{
	Stanza request(STANZA_KIND_IQ);
	request.setType(STANZA_TYPE_SET).setTo(FContactJid.full()).setUniqueId();
	AJingle = request.addElement("jingle",NS_JINGLE);
	AJingle.setAttribute("action",AAction);
	AJingle.setAttribute("sid",FSessionId);
	return request;
}

bool CallSession::sendJingleRequest(Stanza &ARequest)
{
	QString action = ARequest.firstElement("jingle",NS_JINGLE).attribute("action");
	if (FStanzaProcessor->sendStanzaRequest(this,FStreamJid,ARequest,JINGLE_TIMEOUT))
	{
		FRequests.insert(ARequest.id(),action);
		return true;
	}
	LOG_STRM_WARNING(FStreamJid,QString("Failed to send jingle request, sid=%1, action=%2").arg(FSessionId,action));
	return false;
}

void CallSession::sendLocalCandidates()
{
	if (!FDescriptionSent || FLocalCandidates.isEmpty() || FState==Ended)
		return;

	QDomElement jingle;
	Stanza request = makeJingleRequest("transport-info",jingle);

	QMap<int, QDomElement> transports;
	for (int i=0; i<FLocalCandidates.count(); i++)
	{
		int index = FLocalCandidates.at(i).first;
		QString name = FContentNames.value(index);
		if (name.isEmpty() || !FLocalCredentials.contains(name))
			continue;

		if (!transports.contains(index))
		{
			QDomElement contentElem = jingle.appendChild(request.createElement("content")).toElement();
			contentElem.setAttribute("creator","initiator");
			contentElem.setAttribute("name",name);

			// The credentials tell the remote party that this is not an ICE restart
			QDomElement transportElem = contentElem.appendChild(request.createElement("transport",NS_JINGLE_TRANSPORTS_ICE_UDP)).toElement();
			transportElem.setAttribute("ufrag",FLocalCredentials.value(name).first);
			transportElem.setAttribute("pwd",FLocalCredentials.value(name).second);
			transports.insert(index,transportElem);
		}
		JingleSdp::candidateToJingle(FLocalCandidates.at(i).second,transports[index]);
	}
	FLocalCandidates.clear();

	if (!transports.isEmpty())
		sendJingleRequest(request);
}

void CallSession::storeLocalCredentials(const QDomElement &AJingle)
{
	FLocalCredentials.clear();
	for (QDomElement contentElem = AJingle.firstChildElement("content"); !contentElem.isNull(); contentElem = contentElem.nextSiblingElement("content"))
	{
		QDomElement transportElem = contentElem.firstChildElement("transport");
		FLocalCredentials.insert(contentElem.attribute("name"),qMakePair(transportElem.attribute("ufrag"),transportElem.attribute("pwd")));
	}
}

bool CallSession::acceptSessionOffer(const Stanza &ARequest, QString &AJingleReason, QString &AError)
{
	QDomElement jingle = ARequest.firstElement("jingle",NS_JINGLE);

	int contents = 0;
	for (QDomElement contentElem = jingle.firstChildElement("content"); !contentElem.isNull(); contentElem = contentElem.nextSiblingElement("content"))
	{
		contents++;
		if (JingleSdp::contentMedia(contentElem) != "audio")
		{
			AJingleReason = "unsupported-applications";
			AError = tr("Only voice calls are supported");
			return false;
		}
		if (contentElem.firstChildElement("transport").namespaceURI() != NS_JINGLE_TRANSPORTS_ICE_UDP)
		{
			AJingleReason = "unsupported-transports";
			AError = tr("Unsupported connection method");
			return false;
		}
	}
	if (contents != 1)
	{
		AJingleReason = "unsupported-applications";
		AError = tr("Only voice calls are supported");
		return false;
	}

	if (JingleSdp::findPayloadType(jingle.firstChildElement("content"),"opus",48000) < 0)
	{
		AJingleReason = "failed-application";
		AError = tr("No supported audio codec");
		return false;
	}

	QString error;
	if (JingleSdp::jingleToSdp(jingle,true,FContentNames,&error).isEmpty())
	{
		AJingleReason = "failed-application";
		AError = error;
		return false;
	}
	return true;
}

void CallSession::applySessionAccept(const QDomElement &AJingle)
{
	QString error;
	QStringList names;
	QString sdp = JingleSdp::jingleToSdp(AJingle,false,names,&error);
	if (sdp.isEmpty())
	{
		LOG_STRM_WARNING(FStreamJid,QString("Call session answer rejected, sid=%1: %2").arg(FSessionId,error));
		terminate("failed-application",Failed,error);
	}
	else if (FMedia==NULL || !FMedia->setRemoteDescription("answer",sdp))
	{
		terminate("failed-application",Failed,FMedia!=NULL ? FMedia->errorString() : QString());
	}
	else
	{
		LOG_STRM_INFO(FStreamJid,QString("Call session accepted by=%1, sid=%2").arg(FContactJid.full(),FSessionId));
		FAnswered = true;
		setState(Connecting);
	}
}

void CallSession::applyTransportInfo(const QDomElement &AJingle)
{
	for (QDomElement contentElem = AJingle.firstChildElement("content"); !contentElem.isNull(); contentElem = contentElem.nextSiblingElement("content"))
	{
		int index = FContentNames.indexOf(contentElem.attribute("name"));
		QDomElement transportElem = contentElem.firstChildElement("transport");
		if (index<0 || transportElem.namespaceURI()!=NS_JINGLE_TRANSPORTS_ICE_UDP)
			continue;

		for (QDomElement candElem = transportElem.firstChildElement("candidate"); !candElem.isNull(); candElem = candElem.nextSiblingElement("candidate"))
		{
			QString candidate = JingleSdp::candidateToSdp(candElem);
			if (candidate.isEmpty())
				continue;

			if (FMedia)
				FMedia->addRemoteCandidate(index,candidate);
			else if (FRemoteCandidates.count() < MAX_PENDING_CANDIDATES)
				FRemoteCandidates.append(qMakePair(index,candidate));
		}
	}
}

void CallSession::parseIceServers(const QDomElement &AServices)
{
	FIceServers.clear();
	for (QDomElement serviceElem = AServices.firstChildElement("service"); !serviceElem.isNull() && FIceServers.count()<MAX_ICE_SERVERS; serviceElem = serviceElem.nextSiblingElement("service"))
	{
		MediaIceServer server;
		server.type = serviceElem.attribute("type");
		server.host = serviceElem.attribute("host");
		server.port = serviceElem.attribute("port").toInt();
		server.transport = serviceElem.attribute("transport","udp");
		server.username = serviceElem.attribute("username");
		server.password = serviceElem.attribute("password");

		if (server.type!="stun" && server.type!="turn" && server.type!="turns")
			continue;
		if (server.host.isEmpty() || server.port<=0 || server.port>65535)
			continue;
		if (server.transport!="udp" && server.transport!="tcp")
			continue;
		if (server.type=="turns" && server.transport!="tcp")
			continue;
		if (server.type!="stun" && server.username.isEmpty())
			continue;

		FIceServers.append(server);
	}
	LOG_STRM_INFO(FStreamJid,QString("External services for call loaded, sid=%1, count=%2").arg(FSessionId).arg(FIceServers.count()));
}

CallSession::EndReason CallSession::endReasonFromJingle(const QString &AReason, State AState, bool AOutgoing)
{
	bool answered = AState==Connecting || AState==Active;
	if (AReason=="success" || AReason=="gone")
		return RemoteHangup;
	if (AReason == "decline")
		return RemoteDecline;
	if (AReason == "busy")
		return RemoteBusy;
	if (AReason == "cancel")
		return !AOutgoing && !answered ? RemoteCancel : RemoteHangup;
	if (AReason=="timeout" || AReason=="expired")
		return answered ? RemoteHangup : (AOutgoing ? NoAnswer : RemoteCancel);
	if (AReason=="connectivity-error" || AReason=="failed-transport")
		return ConnectionLost;
	return Failed;
}

void CallSession::onStateTimeout()
{
	switch (FState)
	{
	case Proposing:
		sendJingleMessage("retract",FContactJid.bare());
		end(NoAnswer);
		break;
	case Ringing:
		if (FHasOffer)
			terminate("timeout",RemoteCancel);
		else
			end(RemoteCancel);
		break;
	case Proceeding:
		end(Failed,tr("The caller did not start the call"));
		break;
	case Initiating:
		if (FDescriptionSent)
		{
			terminate("cancel",NoAnswer);
		}
		else
		{
			if (FProposed)
				sendJingleMessage("retract",FContactJid);
			end(Failed,tr("Could not prepare the call"));
		}
		break;
	case Connecting:
		terminate("connectivity-error",ConnectionLost,tr("Could not connect to the other side"));
		break;
	default:
		break;
	}
}

void CallSession::onCandidatesTimeout()
{
	sendLocalCandidates();
}

void CallSession::onReconnectTimeout()
{
	terminate("connectivity-error",ConnectionLost);
}

void CallSession::onMediaLocalDescription(const QString &AType, const QString &ASdp)
{
	if (FState==Ended || FDescriptionSent)
		return;

	bool offer = AType=="offer";
	if (offer != FOutgoing)
		return;

	QDomElement jingle;
	Stanza request = makeJingleRequest(offer ? "session-initiate" : "session-accept",jingle);
	jingle.setAttribute(offer ? "initiator" : "responder",FStreamJid.full());

	QString error;
	QStringList names;
	if (!JingleSdp::sdpToJingle(ASdp,FOutgoing,jingle,names,&error))
	{
		LOG_STRM_ERROR(FStreamJid,QString("Failed to describe call session, sid=%1: %2").arg(FSessionId,error));
		terminate("failed-application",Failed,error);
		return;
	}
	if (FOutgoing)
		FContentNames = names;
	storeLocalCredentials(jingle);

	if (!sendJingleRequest(request))
	{
		end(Failed,tr("Connection to the server is lost"));
		return;
	}

	FDescriptionSent = true;
	sendLocalCandidates();
}

void CallSession::onMediaLocalCandidate(int AMLineIndex, const QString &ACandidate)
{
	if (FState != Ended)
	{
		FLocalCandidates.append(qMakePair(AMLineIndex,ACandidate));
		if (FDescriptionSent && !FCandidatesTimer.isActive())
			FCandidatesTimer.start(CANDIDATES_DELAY);
	}
}

void CallSession::onMediaConnected()
{
	FReconnectTimer.stop();
	if (FState == Connecting)
	{
		if (!FActiveTime.isValid())
			FActiveTime = QDateTime::currentDateTime();
		setState(Active);
	}
}

void CallSession::onMediaDisconnected()
{
	if (FState == Active)
		FReconnectTimer.start(RECONNECT_TIMEOUT);
}

void CallSession::onMediaFailed(const QString &AError)
{
	LOG_STRM_ERROR(FStreamJid,QString("Call media failed, sid=%1: %2").arg(FSessionId,AError));
	terminate("failed-application",Failed,AError);
}

void CallSession::onMediaConnectionFailed()
{
	LOG_STRM_WARNING(FStreamJid,QString("Call media connection failed, sid=%1").arg(FSessionId));
	terminate("connectivity-error",ConnectionLost,FActiveTime.isValid() ? QString() : tr("Could not connect to the other side"));
}
