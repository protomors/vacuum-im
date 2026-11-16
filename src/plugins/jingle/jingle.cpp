#include "jingle.h"

#include <QUuid>
#include <definitions/namespaces.h>
#include <definitions/stanzahandlerorders.h>
#include <utils/logger.h>

#define SHC_JINGLE_REQUEST            "/iq[@type='set']/jingle[@xmlns='" NS_JINGLE "']"

Jingle::Jingle()
{
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FDiscovery = NULL;

	FFeatureInserted = false;
}

Jingle::~Jingle()
{

}

void Jingle::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Jingle");
	APluginInfo->description = tr("Allows to negotiate peer-to-peer sessions with contacts");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Vacuum-IM contributors";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool Jingle::initConnections(IPluginManager *APluginManager, int &AInitOrder)
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
	}

	return FXmppStreamManager!=NULL && FStanzaProcessor!=NULL;
}

bool Jingle::stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	if (FSHIRequests.value(AStreamJid) == AHandleId)
	{
		AAccept = true;
		processRequest(AStreamJid,AStanza);
		return true;
	}
	return false;
}

QList<QString> Jingle::applications() const
{
	return FApplications.keys();
}

IJingleHandler *Jingle::applicationHandler(const QString &ADescriptionNS) const
{
	return FApplications.value(ADescriptionNS);
}

bool Jingle::insertApplication(const QString &ADescriptionNS, IJingleHandler *AHandler)
{
	if (AHandler==NULL || ADescriptionNS.isEmpty() || FApplications.contains(ADescriptionNS))
		return false;

	LOG_DEBUG(QString("Jingle application inserted, ns=%1").arg(ADescriptionNS));
	FApplications.insert(ADescriptionNS,AHandler);
	watchHandler(AHandler);
	updateDiscoFeature();
	return true;
}

void Jingle::removeApplication(const QString &ADescriptionNS)
{
	if (FApplications.remove(ADescriptionNS) > 0)
	{
		LOG_DEBUG(QString("Jingle application removed, ns=%1").arg(ADescriptionNS));
		updateDiscoFeature();
	}
}

QString Jingle::newSessionId() const
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

IJingleHandler *Jingle::sessionHandler(const Jid &AStreamJid, const QString &ASessionId) const
{
	return FSessions.value(AStreamJid).value(ASessionId);
}

bool Jingle::insertSession(const Jid &AStreamJid, const QString &ASessionId, IJingleHandler *AHandler)
{
	if (AHandler==NULL || ASessionId.isEmpty() || sessionHandler(AStreamJid,ASessionId)!=NULL)
		return false;

	LOG_STRM_DEBUG(AStreamJid,QString("Jingle session inserted, sid=%1").arg(ASessionId));
	FSessions[AStreamJid].insert(ASessionId,AHandler);
	watchHandler(AHandler);
	emit sessionInserted(AStreamJid,ASessionId);
	return true;
}

void Jingle::removeSession(const Jid &AStreamJid, const QString &ASessionId)
{
	QMap<Jid, QMap<QString, IJingleHandler *> >::iterator it = FSessions.find(AStreamJid);
	if (it!=FSessions.end() && it->remove(ASessionId)>0)
	{
		if (it->isEmpty())
			FSessions.erase(it);
		LOG_STRM_DEBUG(AStreamJid,QString("Jingle session removed, sid=%1").arg(ASessionId));
		emit sessionRemoved(AStreamJid,ASessionId);
	}
}

Stanza Jingle::makeRequest(const Jid &AContactJid, const QString &ASessionId, const QString &AAction, QDomElement &AJingle) const
{
	Stanza request(STANZA_KIND_IQ);
	request.setType(STANZA_TYPE_SET).setTo(AContactJid.full()).setUniqueId();
	AJingle = request.addElement("jingle",NS_JINGLE);
	AJingle.setAttribute("action",AAction);
	AJingle.setAttribute("sid",ASessionId);
	return request;
}

bool Jingle::sendResult(const Jid &AStreamJid, const Stanza &ARequest) const
{
	Stanza result = FStanzaProcessor->makeReplyResult(ARequest);
	return FStanzaProcessor->sendStanzaOut(AStreamJid,result);
}

bool Jingle::sendError(const Jid &AStreamJid, const Stanza &ARequest, XmppStanzaError::ErrorCondition ACondition, const QString &AJingleCondition) const
{
	XmppStanzaError err(ACondition);
	if (!AJingleCondition.isEmpty())
		err.setAppCondition(NS_JINGLE_ERRORS,AJingleCondition);
	Stanza error = FStanzaProcessor->makeReplyError(ARequest,err);
	return FStanzaProcessor->sendStanzaOut(AStreamJid,error);
}

bool Jingle::sendTerminate(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, const QString &AReason, const QString &AText) const
{
	QDomElement jingle;
	Stanza request = makeRequest(AContactJid,ASessionId,"session-terminate",jingle);
	QDomElement reasonElem = jingle.appendChild(request.createElement("reason")).toElement();
	reasonElem.appendChild(request.createElement(AReason));
	if (!AText.isEmpty())
		reasonElem.appendChild(request.createElement("text")).appendChild(request.createTextNode(AText));
	return FStanzaProcessor->sendStanzaOut(AStreamJid,request);
}

void Jingle::processRequest(const Jid &AStreamJid, const Stanza &ARequest)
{
	QDomElement jingle = ARequest.firstElement("jingle",NS_JINGLE);
	QString sessionId = jingle.attribute("sid");
	QString action = jingle.attribute("action");
	Jid contactJid = ARequest.fromJid();

	if (sessionId.isEmpty() || action.isEmpty())
	{
		sendError(AStreamJid,ARequest,XmppStanzaError::EC_BAD_REQUEST);
		return;
	}

	IJingleHandler *handler = sessionHandler(AStreamJid,sessionId);
	if (handler != NULL)
	{
		if (!handler->jingleSessionRequest(AStreamJid,ARequest))
			sendError(AStreamJid,ARequest,XmppStanzaError::EC_ITEM_NOT_FOUND,"unknown-session");
	}
	else if (action != "session-initiate")
	{
		sendError(AStreamJid,ARequest,XmppStanzaError::EC_ITEM_NOT_FOUND,"unknown-session");
	}
	else if (!contactJid.hasResource())
	{
		sendError(AStreamJid,ARequest,XmppStanzaError::EC_BAD_REQUEST);
	}
	else
	{
		QString descriptionNS = jingle.firstChildElement("content").firstChildElement("description").namespaceURI();
		handler = FApplications.value(descriptionNS);
		if (handler == NULL)
		{
			LOG_STRM_INFO(AStreamJid,QString("Jingle session with unsupported application rejected, from=%1, sid=%2, ns=%3").arg(contactJid.full(),sessionId,descriptionNS));
			if (FApplications.isEmpty())
			{
				sendError(AStreamJid,ARequest,XmppStanzaError::EC_SERVICE_UNAVAILABLE);
			}
			else
			{
				sendResult(AStreamJid,ARequest);
				sendTerminate(AStreamJid,contactJid,sessionId,"unsupported-applications");
			}
		}
		else if (!handler->jingleSessionInitiate(AStreamJid,ARequest))
		{
			sendError(AStreamJid,ARequest,XmppStanzaError::EC_SERVICE_UNAVAILABLE);
		}
	}
}

void Jingle::updateDiscoFeature()
{
	// Jingle is announced only while some application can use it
	bool needed = !FApplications.isEmpty();
	if (FDiscovery!=NULL && needed!=FFeatureInserted)
	{
		if (needed)
		{
			IDiscoFeature dfeature;
			dfeature.active = true;
			dfeature.var = NS_JINGLE;
			dfeature.name = tr("Jingle");
			dfeature.description = tr("Supports the negotiation of peer-to-peer sessions with another contact");
			FDiscovery->insertDiscoFeature(dfeature);
		}
		else
		{
			FDiscovery->removeDiscoFeature(NS_JINGLE);
		}
		FFeatureInserted = needed;
	}
}

void Jingle::watchHandler(IJingleHandler *AHandler)
{
	// The handler can not be asked for its object once that object is being destroyed
	if (!FHandlerObjects.contains(AHandler))
	{
		FHandlerObjects.insert(AHandler,AHandler->instance());
		connect(AHandler->instance(),SIGNAL(destroyed(QObject *)),SLOT(onHandlerDestroyed(QObject *)));
	}
}

void Jingle::onXmppStreamOpened(IXmppStream *AXmppStream)
{
	IStanzaHandle requestHandle;
	requestHandle.handler = this;
	requestHandle.order = SHO_DEFAULT;
	requestHandle.direction = IStanzaHandle::DirectionIn;
	requestHandle.streamJid = AXmppStream->streamJid();
	requestHandle.conditions.append(SHC_JINGLE_REQUEST);
	FSHIRequests.insert(requestHandle.streamJid,FStanzaProcessor->insertStanzaHandle(requestHandle));
}

void Jingle::onXmppStreamClosed(IXmppStream *AXmppStream)
{
	Jid streamJid = AXmppStream->streamJid();
	if (FSHIRequests.contains(streamJid))
		FStanzaProcessor->removeStanzaHandle(FSHIRequests.take(streamJid));

	// Applications end their sessions of the stream on their own
	foreach(const QString &sessionId, FSessions.value(streamJid).keys())
		removeSession(streamJid,sessionId);
}

void Jingle::onHandlerDestroyed(QObject *AObject)
{
	foreach(IJingleHandler *handler, FHandlerObjects.keys(AObject))
	{
		FHandlerObjects.remove(handler);

		foreach(const QString &ns, FApplications.keys(handler))
			removeApplication(ns);

		foreach(const Jid &streamJid, FSessions.keys())
			foreach(const QString &sessionId, FSessions.value(streamJid).keys(handler))
				removeSession(streamJid,sessionId);
	}
}
