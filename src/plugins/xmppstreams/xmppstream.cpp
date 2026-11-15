#include "xmppstream.h"
#include <utils/qtcompat.h>

#include <QTextDocument>
#include <QGuiApplication>
#include <definitions/namespaces.h>
#include <definitions/optionvalues.h>
#include <definitions/internalerrors.h>
#include <definitions/xmppstanzahandlerorders.h>
#include <utils/versionparser.h>
#include <utils/options.h>
#include <utils/logger.h>

#define SM_REQUEST_DELAY          1000
#define SM_DEFAULT_RESUME_TIME    300
#define SM_RESEND_MAX_AGE         600
#define CSI_INACTIVE_DELAY        60000

XmppStream::XmppStream(IXmppStreamManager *AXmppStreamManager, const Jid &AStreamJid) : QObject(AXmppStreamManager->instance())
{
	FXmppStreamManager = AXmppStreamManager;

	FReady = false;
	FClosed = true;
	FEncrypt = true;
	FNodeChanged = false;
	FDomainChanged = false;
	FPasswordRequested = false;
	FConnection = NULL;
	FStreamState = SS_OFFLINE;

	FStreamJid = AStreamJid;
	FOfflineJid = FStreamJid;

	connect(&FParser,SIGNAL(opened(const QDomElement &)), SLOT(onParserOpened(const QDomElement &)));
	connect(&FParser,SIGNAL(element(const QDomElement &)), SLOT(onParserElement(const QDomElement &)));
	connect(&FParser,SIGNAL(error(const XmppError &)), SLOT(onParserError(const XmppError &)));
	connect(&FParser,SIGNAL(closed()), SLOT(onParserClosed()));

	FKeepAliveTimer.setSingleShot(false);
	connect(&FKeepAliveTimer,SIGNAL(timeout()),SLOT(onKeepAliveTimeout()));

	FSmEnabled = false;
	FSmEnabling = false;
	FSmResumable = false;
	FSmResuming = false;
	FSmResumeSent = false;
	FSmAckRequested = false;
	FSmMaxResume = 0;
	FSmResumeAttempt = 0;
	FSmInHandled = 0;
	FSmOutAcked = 0;

	FSmRequestTimer.setSingleShot(true);
	connect(&FSmRequestTimer,SIGNAL(timeout()),SLOT(onSmRequestTimeout()));

	FSmResumeTimer.setSingleShot(true);
	connect(&FSmResumeTimer,SIGNAL(timeout()),SLOT(onSmResumeTimeout()));

	FCsiSupported = false;
	FCsiActive = true;
	FCsiSentActive = true;
	FCsiInactiveTimer.setSingleShot(true);
	connect(&FCsiInactiveTimer,SIGNAL(timeout()),SLOT(onClientInactiveTimeout()));
	if (QGuiApplication::applicationState() != Qt::ApplicationActive)
		FCsiInactiveTimer.start(CSI_INACTIVE_DELAY);
	connect(qApp,SIGNAL(applicationStateChanged(Qt::ApplicationState)),SLOT(onApplicationStateChanged(Qt::ApplicationState)));
}

XmppStream::~XmppStream()
{
	abort(XmppError(IERR_XMPPSTREAM_DESTROYED));
	clearActiveFeatures();
	emit streamDestroyed();
}

bool XmppStream::xmppStanzaIn(IXmppStream *AXmppStream, Stanza &AStanza, int AOrder)
{
	if (AXmppStream==this && AOrder==XSHO_XMPP_STREAM && AStanza.namespaceURI()==NS_JABBER_STREAMS)
	{
		if (FStreamState==SS_INITIALIZE && AStanza.kind()=="stream")
		{
			FStreamId = AStanza.id();
			setStreamState(SS_FEATURES);
			if (VersionParser(AStanza.attribute("version","0.0")) < VersionParser(1,0))
			{
				LOG_STRM_WARNING(streamJid(),QString("Unsupported pre-XMPP 1.0 stream, version=%1").arg(AStanza.attribute("version")));
				abort(XmppError(IERR_XMPPSTREAM_UNSUPPORTED_VERSION));
			}
			return true;
		}
		else if (FStreamState==SS_FEATURES && AStanza.kind()=="features")
		{
			FServerFeatures = AStanza.element().cloneNode(true).toElement();
			if (FSmResuming && !findServerFeature(NS_FEATURE_BIND).isNull())
			{
				// Authenticated on a new connection, resume the previous session instead of binding a new one
				if (isEncryptionRequired() && !connection()->isEncrypted())
					failSmResume(XmppError(IERR_XMPPSTREAM_NOT_SECURE));
				else if (findServerFeature(NS_STREAM_MANAGEMENT).isNull())
					failSmResume(XmppError(IERR_XMPPSTREAM_RESUME_FAILED));
				else
					sendSmResume();
			}
			else
			{
				FAvailFeatures = FXmppStreamManager->xmppFeatures();
				processFeatures();
			}
			return true;
		}
		else if (AStanza.kind() == "error")
		{
			abort(XmppStreamError(AStanza.element()));
			return true;
		}
	}
	else if (AXmppStream==this && AOrder==XSHO_XMPP_STREAM && AStanza.namespaceURI()==NS_STREAM_MANAGEMENT)
	{
		processSmElement(AStanza.element());
		return true;
	}
	return false;
}

bool XmppStream::xmppStanzaOut(IXmppStream *AXmppStream, Stanza &AStanza, int AOrder)
{
	Q_UNUSED(AXmppStream); Q_UNUSED(AStanza); Q_UNUSED(AOrder);
	return false;
}

bool XmppStream::isOpen() const
{
	return FReady && (!FClosed || FSmResuming);
}

bool XmppStream::isConnected() const
{
	return FStreamState!=SS_OFFLINE;
}

bool XmppStream::open()
{
	if (FConnection && FStreamState==SS_OFFLINE)
	{
		FError = XmppError::null;

		LOG_STRM_INFO(streamJid(),"Opening XMPP stream");
		if (FConnection->connectToHost())
		{
			FNodeChanged = false;
			FDomainChanged = false;
			FOnlineJid = FOfflineJid;
			setStreamState(SS_CONNECTING);
			return true;
		}
		else
		{
			abort(XmppError(IERR_XMPPSTREAM_FAILED_START_CONNECTION));
		}
	}
	else if (!FConnection)
	{
		LOG_STRM_ERROR(streamJid(),"Failed to open XMPP stream: Connection not set");
		emit error(tr("Connection not specified"));
	}
	return false;
}

void XmppStream::close()
{
	if (FConnection && FStreamState!=SS_OFFLINE && FStreamState!=SS_ERROR && FStreamState!=SS_DISCONNECTING)
	{
		LOG_STRM_INFO(streamJid(),"Closing XMPP stream");
		if (!FSmOutQueue.isEmpty())
			LOG_STRM_WARNING(streamJid(),QString("Closing XMPP stream with %1 unacknowledged stanza(s)").arg(FSmOutQueue.count()));
		finishSmSession(false);
		FSmResendMessages.clear();
		setStreamState(SS_DISCONNECTING);
		if (FConnection->isOpen())
		{
			emit aboutToClose();
			sendData("</stream:stream>");
			LOG_STRM_INFO(streamJid(),"XMPP stream finish request sent");

			setKeepAliveTimerActive(true);
			FClosed = true;
		}
		else
		{
			FClosed = true;
			FConnection->disconnectFromHost();
		}
	}
}

void XmppStream::abort(const XmppError &AError)
{
	if (FStreamState!=SS_OFFLINE && FStreamState!=SS_ERROR)
	{
		LOG_STRM_WARNING(streamJid(),QString("Aborting XMPP stream: %1").arg(AError.condition()));
		if (FSmResuming)
		{
			FSmResuming = false;
			FSmResumeTimer.stop();
		}

		if (FStreamState != SS_DISCONNECTING)
		{
			setStreamState(SS_ERROR);
			FError = AError;
			emit error(AError);
		}

		FClosed = true;
		FConnection->disconnectFromHost();
	}
}

QString XmppStream::streamId() const
{
	return FStreamId;
}

XmppError XmppStream::error() const
{
	return FError;
}

Jid XmppStream::streamJid() const
{
	return FStreamJid;
}

void XmppStream::setStreamJid(const Jid &AStreamJid)
{
	if (FStreamJid!=AStreamJid && AStreamJid.isValid())
	{
		if (FStreamState==SS_OFFLINE || !FStreamJid.hasNode())
		{
			LOG_STRM_INFO(streamJid(),QString("Changing offline XMPP stream JID, from=%1, to=%2").arg(FOfflineJid.full(),AStreamJid.full()));

			Jid before = FStreamJid;
			Jid after = AStreamJid;
			emit jidAboutToBeChanged(after);

			FOfflineJid = after;
			FStreamJid = after;
			emit jidChanged(before);
		}
		else if (FStreamState == SS_FEATURES)
		{
			LOG_STRM_INFO(streamJid(),QString("Changing online XMPP stream JID, from=%1, to=%2").arg(FOnlineJid.full(),AStreamJid.full()));

			Jid before = FStreamJid;
			Jid after(FStreamJid.node(),FStreamJid.domain(),AStreamJid.resource());
			emit jidAboutToBeChanged(after);

			FOnlineJid = AStreamJid;
			FStreamJid = after;
			FNodeChanged = FOnlineJid.pNode()!=FOfflineJid.pNode();
			FDomainChanged = FOnlineJid.pDomain()!=FOfflineJid.pDomain();
			emit jidChanged(before);
		}
		else
		{
			LOG_STRM_WARNING(streamJid(),QString("Failed to change stream jid to=%1: Wrong stream state").arg(AStreamJid.full()));
		}
	}
	else if (!AStreamJid.isValid())
	{
		REPORT_ERROR("Failed to change stream jid: Invalid parameters");
	}
}

bool XmppStream::requestPassword()
{
	if (!FPasswordRequested)
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP stream password request"));
		emit passwordRequested(FPasswordRequested);
	}
	return FPasswordRequested;
}

QString XmppStream::password() const
{
	return FPassword;
}

void XmppStream::setPassword(const QString &APassword)
{
	if (FPassword != APassword)
	{
		FPassword = APassword;
		LOG_STRM_DEBUG(streamJid(),"XMPP stream password changed");
	}
	if (FPasswordRequested)
	{
		FPasswordRequested = false;
		LOG_STRM_DEBUG(streamJid(),"XMPP stream password provided");
		QMetaObject::invokeMethod(this,"passwordProvided",Qt::QueuedConnection,Q_ARG(QString,APassword));
	}
}

QString XmppStream::defaultLang() const
{
	return FDefLang;
}

void XmppStream::setDefaultLang(const QString &ADefLang)
{
	if (FDefLang != ADefLang)
	{
		FDefLang = ADefLang;
		LOG_STRM_DEBUG(streamJid(),QString("Default XMPP stream language changed to=%1").arg(ADefLang));
	}
}

bool XmppStream::isEncryptionRequired() const
{
	return FEncrypt;
}

void XmppStream::setEncryptionRequired(bool ARequire)
{
	if (FEncrypt != ARequire)
	{
		FEncrypt = ARequire;
		LOG_STRM_DEBUG(streamJid(),QString("XMPP stream encryption require changed to=%1").arg(ARequire));
	}
}

IConnection *XmppStream::connection() const
{
	return FConnection;
}

void XmppStream::setConnection(IConnection *AConnection)
{
	if (FStreamState==SS_OFFLINE && FConnection!=AConnection)
	{
		if (FConnection)
			FConnection->instance()->disconnect(this);

		if (AConnection)
		{
			connect(AConnection->instance(),SIGNAL(connected()),SLOT(onConnectionConnected()));
			connect(AConnection->instance(),SIGNAL(readyRead(qint64)),SLOT(onConnectionReadyRead(qint64)));
			connect(AConnection->instance(),SIGNAL(error(const XmppError &)),SLOT(onConnectionError(const XmppError &)));
			connect(AConnection->instance(),SIGNAL(disconnected()),SLOT(onConnectionDisconnected()));
			LOG_STRM_INFO(streamJid(),QString("XMPP stream connection changed to=%1").arg(AConnection->instance()->metaObject()->className()));
		}
		else
		{
			LOG_STRM_INFO(streamJid(),"XMPP stream connection removed");
		}

		FConnection = AConnection;
		emit connectionChanged(AConnection);
	}
	else if (FStreamState != SS_OFFLINE)
	{
		LOG_STRM_WARNING(streamJid(),"Failed to change XMPP stream connection: Stream is not offline");
	}
}

bool XmppStream::isKeepAliveTimerActive() const
{
	return FKeepAliveTimer.isActive();
}

void XmppStream::setKeepAliveTimerActive(bool AActive)
{
	if (AActive)
	{
		switch (FStreamState)
		{
		case SS_OFFLINE:
		case SS_CONNECTING:
			FKeepAliveTimer.stop();
			break;
		case SS_INITIALIZE:
		case SS_FEATURES:
			FKeepAliveTimer.start(Options::node(OPV_XMPPSTREAMS_TIMEOUT_HANDSHAKE).value().toInt());
			break;
		case SS_ONLINE:
		case SS_ERROR:
			FKeepAliveTimer.start(Options::node(OPV_XMPPSTREAMS_TIMEOUT_KEEPALIVE).value().toInt());
			break;
		case SS_DISCONNECTING:
			FKeepAliveTimer.start(Options::node(OPV_XMPPSTREAMS_TIMEOUT_DISCONNECT).value().toInt());
			break;
		}
	}
	else
	{
		FKeepAliveTimer.stop();
	}
}

qint64 XmppStream::sendStanza(Stanza &AStanza)
{
	if (FSmResuming || (FStreamState!=SS_OFFLINE && FStreamState!=SS_ERROR))
	{
		bool counted = FSmEnabled && isSmCountedStanza(AStanza.element());
		if (FSmResuming && FClosed && !counted)
		{
			LOG_STRM_WARNING(streamJid(),"Failed to send XMPP stream element: Stream is resuming");
		}
		else if ((!FClosed || FSmResuming) && !processStanzaHandlers(AStanza,true))
		{
			if (FNodeChanged || FDomainChanged)
			{
				Jid toJid = AStanza.to();
				if (FNodeChanged && toJid.pBare()==FOfflineJid.pBare())
				{
					// Replace node and domain of destination
					Jid newToJid = Jid(FOnlineJid.node(),FOnlineJid.domain(),toJid.resource());
					AStanza.setTo(newToJid.full());
				}
				else if (FDomainChanged && toJid.pBare()==FOfflineJid.pDomain())
				{
					// Replace domain of destination
					Jid newToJid = Jid(toJid.node(),FOnlineJid.domain(),toJid.resource());
					AStanza.setTo(newToJid.full());
				}
			}

			QByteArray data = AStanza.toByteArray();
			if (counted)
			{
				queueSmStanza(AStanza,data);
				if (FSmResuming)
				{
					// Will be sent when the session is resumed
					return 0;
				}
				qint64 bytes = sendData(data);
				if (!FSmRequestTimer.isActive())
					FSmRequestTimer.start(SM_REQUEST_DELAY);
				return bytes;
			}
			return sendData(data);
		}
		else if (FClosed && !FSmResuming)
		{
			LOG_STRM_WARNING(streamJid(),"Failed to send XMPP stream stanza: XML stream is finished");
		}
	}
	else
	{
		LOG_STRM_WARNING(streamJid(),"Failed to send XMPP stream stanza: Stream is not connected");
	}
	return -1;
}

void XmppStream::insertXmppDataHandler(int AOrder, IXmppDataHandler *AHandler)
{
	if (AHandler && !FDataHandlers.contains(AOrder, AHandler))
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP data handler inserted, order=%1, address=%2").arg(AOrder).arg((quint64)AHandler));
		FDataHandlers.insert(AOrder, AHandler);
		emit dataHandlerInserted(AOrder,AHandler);
	}
}

void XmppStream::removeXmppDataHandler(int AOrder, IXmppDataHandler *AHandler)
{
	if (FDataHandlers.contains(AOrder, AHandler))
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP data handler removed, order=%1, address=%2").arg(AOrder).arg((quint64)AHandler));
		FDataHandlers.remove(AOrder, AHandler);
		emit dataHandlerRemoved(AOrder,AHandler);
	}
}

void XmppStream::insertXmppStanzaHandler(int AOrder, IXmppStanzaHadler *AHandler)
{
	if (AHandler && !FStanzaHandlers.contains(AOrder, AHandler))
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP stanza handler inserted, order=%1, address=%2").arg(AOrder).arg((quint64)AHandler));
		FStanzaHandlers.insert(AOrder, AHandler);
		emit stanzaHandlerInserted(AOrder, AHandler);
	}
}

void XmppStream::removeXmppStanzaHandler(int AOrder, IXmppStanzaHadler *AHandler)
{
	if (FStanzaHandlers.contains(AOrder, AHandler))
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP stanza handler removed, order=%1, address=%2").arg(AOrder).arg((quint64)AHandler));
		FStanzaHandlers.remove(AOrder, AHandler);
		emit stanzaHandlerRemoved(AOrder, AHandler);
	}
}

void XmppStream::startStream()
{
	LOG_STRM_DEBUG(streamJid(),"Starting XMPP stream");

	FParser.restart();
	setKeepAliveTimerActive(true);

	Stanza stream("stream:stream",NS_JABBER_STREAMS);
	stream.setAttribute("to", FStreamJid.domain());
	stream.setAttribute("xmlns", NS_JABBER_CLIENT);
	stream.setAttribute("xmlns:xml", NS_XML);
	stream.setAttribute("xml:lang", !FDefLang.isEmpty() ? FDefLang : QLocale().name().split('_').value(0));

	setStreamState(SS_INITIALIZE);
	if (!processStanzaHandlers(stream,true))
	{
		QByteArray data = QString("<?xml version=\"1.0\"?>").toUtf8() + stream.toByteArray().trimmed();
		data.remove(data.size()-2,1);
		sendData(data);
	}
}

void XmppStream::processFeatures()
{
	bool started = false;
	while (!started && !FAvailFeatures.isEmpty())
	{
		QString featureNS = FAvailFeatures.takeFirst();
		QDomElement featureElem = FServerFeatures.firstChildElement();
		while (!featureElem.isNull() && featureElem.namespaceURI()!=featureNS)
			featureElem = featureElem.nextSiblingElement();
		started = !featureElem.isNull() ? startFeature(featureNS, featureElem) : false;
	}
	if (!started)
	{
		if (isEncryptionRequired() && !connection()->isEncrypted())
			abort(XmppError(IERR_XMPPSTREAM_NOT_SECURE));
		else if (canEnableSm())
			sendSmEnable();
		else
			openStream();
	}
}

void XmppStream::openStream()
{
	FReady = true;
	setStreamState(SS_ONLINE);
	setKeepAliveTimerActive(true);
	LOG_STRM_INFO(streamJid(),QString("XMPP stream opened, stream_management=%1, resumable=%2").arg(FSmEnabled).arg(FSmResumable));
	emit opened();
	flushSmResendMessages();

	// Server assumes the client is active on a new session
	FCsiSupported = !findServerFeature(NS_CLIENT_STATE).isNull();
	FCsiSentActive = true;
	updateClientState(false);
}

void XmppStream::resetConnectionState()
{
	FClosed = true;
	setKeepAliveTimerActive(false);
	removeXmppStanzaHandler(XSHO_XMPP_STREAM,this);
	clearActiveFeatures();
}

QDomElement XmppStream::findServerFeature(const QString &AFeatureNS) const
{
	QDomElement featureElem = FServerFeatures.firstChildElement();
	while (!featureElem.isNull() && featureElem.namespaceURI()!=AFeatureNS)
		featureElem = featureElem.nextSiblingElement();
	return featureElem;
}

void XmppStream::clearActiveFeatures()
{
	foreach(IXmppFeature *feature, setFromList(FActiveFeatures))
		delete feature->instance();
	FActiveFeatures.clear();
}

void XmppStream::setStreamState(StreamState AState)
{
	if (FStreamState != AState)
	{
		LOG_STRM_DEBUG(streamJid(),QString("XMPP stream state changed to=%1").arg(AState));
		FStreamState = AState;
	}
}

bool XmppStream::startFeature(const QString &AFeatureNS, const QDomElement &AFeatureElem)
{
	LOG_STRM_DEBUG(streamJid(),QString("Starting XMPP stream feature=%1").arg(AFeatureNS));
	foreach(IXmppFeatureFactory *factory, FXmppStreamManager->xmppFeatureFactories(AFeatureNS))
	{
		IXmppFeature *feature = factory->newXmppFeature(AFeatureNS, this);
		if (feature && feature->start(AFeatureElem))
		{
			FActiveFeatures.append(feature);
			connect(feature->instance(),SIGNAL(finished(bool)),SLOT(onFeatureFinished(bool)));
			connect(feature->instance(),SIGNAL(error(const XmppError &)),SLOT(onFeatureError(const XmppError &)));
			connect(feature->instance(),SIGNAL(featureDestroyed()),SLOT(onFeatureDestroyed()));
			connect(this,SIGNAL(closed()),feature->instance(),SLOT(deleteLater()));
			return true;
		}
		else if (feature)
		{
			feature->instance()->deleteLater();
		}
	}
	return false;
}

bool XmppStream::processDataHandlers(QByteArray &AData, bool ADataOut)
{
	bool hooked = false;
	MultiMapIterator<int, IXmppDataHandler *> it(FDataHandlers);
	if (!ADataOut)
		it.toBack();
	while (!hooked && (ADataOut ? it.hasNext() : it.hasPrevious()))
	{
		if (ADataOut)
		{
			it.next();
			hooked = it.value()->xmppDataOut(this, AData, it.key());
		}
		else
		{
			it.previous();
			hooked = it.value()->xmppDataIn(this, AData, it.key());
		}
	}
	return hooked;
}

bool XmppStream::processStanzaHandlers(Stanza &AStanza, bool AStanzaOut)
{
	if (!AStanzaOut)
		LOG_STRM_TYPE(Logger::Stanza,streamJid(),QString("Stanza received:\n\n%1").arg(AStanza.toString(2)));

	bool hooked = false;
	MultiMapIterator<int, IXmppStanzaHadler *> it(FStanzaHandlers);
	if (!AStanzaOut)
	{
		AStanza.setTo(FStreamJid.full());
		it.toBack();
	}
	while (!hooked && (AStanzaOut ? it.hasNext() : it.hasPrevious()))
	{
		if (AStanzaOut)
		{
			it.next();
			hooked = it.value()->xmppStanzaOut(this, AStanza, it.key());
		}
		else
		{
			it.previous();
			hooked = it.value()->xmppStanzaIn(this, AStanza, it.key());
		}
	}

	if (AStanzaOut && !hooked)
		LOG_STRM_TYPE(Logger::Stanza,streamJid(),QString("Stanza sent:\n\n%1").arg(AStanza.toString(2)));

	return hooked;
}

qint64 XmppStream::sendData(QByteArray AData)
{
	if (!processDataHandlers(AData,true))
	{
		setKeepAliveTimerActive(true);
		return FConnection->write(AData);
	}
	return 0;
}

QByteArray XmppStream::receiveData(qint64 ABytes)
{
	return FConnection->read(ABytes);
}

void XmppStream::onConnectionConnected()
{
	if (FStreamState!=SS_OFFLINE && FStreamState!=SS_ERROR)
	{
		FClosed = false;
		insertXmppStanzaHandler(XSHO_XMPP_STREAM,this);
		startStream();
	}
}

void XmppStream::onConnectionReadyRead(qint64 ABytes)
{
	if (FSmResuming || (FStreamState!=SS_OFFLINE && FStreamState!=SS_ERROR))
	{
		QByteArray data = receiveData(ABytes);
		if (!processDataHandlers(data,false))
			if (!data.isEmpty())
				FParser.parseData(data);
	}
}

void XmppStream::onConnectionError(const XmppError &AError)
{
	if (FSmResuming)
		LOG_STRM_INFO(streamJid(),QString("Connection error while resuming XMPP stream: %1").arg(AError.condition()));
	else
		connectionLost(AError);
}

void XmppStream::onConnectionDisconnected()
{
	if (FSmResuming && FStreamState!=SS_DISCONNECTING)
	{
		// Connection attempt finished without resuming the session, try again later
		if (!FClosed || FStreamState!=SS_CONNECTING || !FSmResumeTimer.isActive())
		{
			resetConnectionState();
			setStreamState(SS_CONNECTING);
			scheduleSmResume();
		}
	}
	else if (FStreamState==SS_ONLINE && canResumeSm())
	{
		startSmResume(XmppError(IERR_XMPPSTREAM_CLOSED_UNEXPECTEDLY));
	}
	else if (FStreamState != SS_OFFLINE)
	{
		bool userClose = FStreamState==SS_DISCONNECTING;

		FReady = false;
		FClosed = true;

		if (FStreamState != SS_DISCONNECTING)
			abort(XmppError(IERR_XMPPSTREAM_CLOSED_UNEXPECTEDLY));

		setStreamState(SS_OFFLINE);
		setKeepAliveTimerActive(false);
		removeXmppStanzaHandler(XSHO_XMPP_STREAM,this);
		finishSmSession(!userClose);
		FCsiSupported = false;

		LOG_STRM_INFO(streamJid(),"XMPP stream closed");
		emit closed();

		clearActiveFeatures();
		setStreamJid(FOfflineJid);

		FNodeChanged = false;
		FDomainChanged = false;
		FPasswordRequested = false;
		FOnlineJid = Jid::null;
	}
}

void XmppStream::onParserOpened(const QDomElement &AElem)
{
	Stanza stanza(AElem);
	processStanzaHandlers(stanza,false);
}

void XmppStream::onParserElement(const QDomElement &AElem)
{
	bool counted = FSmEnabled && isSmCountedStanza(AElem);
	Stanza stanza(AElem);
	processStanzaHandlers(stanza,false);
	if (counted)
		FSmInHandled++;
}

void XmppStream::onParserError(const XmppError &AError)
{
	static const QString xmlError(
		"<stream:error>"
			"<xml-not-well-formed xmlns='urn:ietf:params:xml:ns:xmpp-streams'/>"
			"<text xmlns='urn:ietf:params:xml:ns:xmpp-streams'>%1</text>"
		"</stream:error></stream:stream>");

	sendData(xmlError.arg(AError.errorText()).toUtf8());
	abort(AError);
}

void XmppStream::onParserClosed()
{
	FClosed = true;
	LOG_STRM_INFO(streamJid(),"XMPP stream finished");
	if (FSmResuming)
	{
		// Server finished the stream during the resume handshake
		failSmResume(XmppError(IERR_XMPPSTREAM_RESUME_FAILED));
	}
	else
	{
		// The server ended the session, it can not be resumed
		FSmResumable = false;
		FConnection->disconnectFromHost();
	}
}

void XmppStream::onFeatureFinished(bool ARestart)
{
	if (!ARestart)
		processFeatures();
	else
		startStream();
}

void XmppStream::onFeatureError(const XmppError &AError)
{
	abort(AError);
}

void XmppStream::onFeatureDestroyed()
{
	IXmppFeature *feature = qobject_cast<IXmppFeature *>(sender());
	if (feature)
		FActiveFeatures.removeAll(feature);
}

void XmppStream::onKeepAliveTimeout()
{
	static const QByteArray space(1,' ');
	if (FStreamState == SS_DISCONNECTING)
	{
		FConnection->disconnectFromHost();
	}
	else if (FSmResuming)
	{
		LOG_STRM_INFO(streamJid(),"XMPP stream resume handshake timed out");
		FConnection->disconnectFromHost();
	}
	else if (FStreamState != SS_ONLINE)
	{
		abort(XmppStreamError(XmppStreamError::EC_CONNECTION_TIMEOUT));
	}
	else if (FSmEnabled)
	{
		sendSmRequest();
	}
	else
	{
		sendData(space);
	}
}

bool XmppStream::isSmCountedStanza(const QDomElement &AElem) const
{
	static const QStringList counted = QStringList() << "message" << "presence" << "iq";
	return (AElem.namespaceURI().isEmpty() || AElem.namespaceURI()==NS_JABBER_CLIENT) && counted.contains(AElem.tagName());
}

bool XmppStream::canEnableSm() const
{
	return !FSmEnabled && Options::node(OPV_XMPPSTREAMS_STREAMMANAGEMENT).value().toBool()
		&& !findServerFeature(NS_STREAM_MANAGEMENT).isNull() && !findServerFeature(NS_FEATURE_BIND).isNull();
}

bool XmppStream::canResumeSm() const
{
	return FSmEnabled && FSmResumable && !FSmId.isEmpty() && !FSmResuming;
}

void XmppStream::sendSmEnable()
{
	FSmEnabling = true;
	Stanza enable("enable",NS_STREAM_MANAGEMENT);
	enable.setAttribute("resume","true");
	LOG_STRM_INFO(streamJid(),"Enabling XMPP stream management");
	sendStanza(enable);
}

void XmppStream::sendSmResume()
{
	FSmResumeSent = true;
	Stanza resume("resume",NS_STREAM_MANAGEMENT);
	resume.setAttribute("h",QString::number(FSmInHandled));
	resume.setAttribute("previd",FSmId);
	LOG_STRM_INFO(streamJid(),QString("Resuming XMPP stream, previd=%1, h=%2").arg(FSmId).arg(FSmInHandled));
	sendStanza(resume);
}

void XmppStream::sendSmRequest()
{
	if (FSmEnabled && !FSmResuming && FStreamState==SS_ONLINE)
	{
		int timeout = Options::node(OPV_XMPPSTREAMS_TIMEOUT_KEEPALIVE).value().toInt();
		if (FSmAckRequested && FSmAckRequestTime.msecsTo(QDateTime::currentDateTime())>=timeout)
		{
			LOG_STRM_WARNING(streamJid(),"XMPP stream management acknowledgement timed out");
			connectionLost(XmppStreamError(XmppStreamError::EC_CONNECTION_TIMEOUT));
		}
		else
		{
			if (!FSmAckRequested)
			{
				FSmAckRequested = true;
				FSmAckRequestTime = QDateTime::currentDateTime();
			}
			Stanza request("r",NS_STREAM_MANAGEMENT);
			sendStanza(request);
		}
	}
}

void XmppStream::sendSmAnswer()
{
	Stanza answer("a",NS_STREAM_MANAGEMENT);
	answer.setAttribute("h",QString::number(FSmInHandled));
	sendStanza(answer);
}

void XmppStream::processSmElement(const QDomElement &AElem)
{
	QString tag = AElem.tagName();
	if (tag == "r")
	{
		if (FSmEnabled)
			sendSmAnswer();
	}
	else if (tag == "a")
	{
		if (FSmEnabled)
			processSmAck(AElem.attribute("h").toUInt());
	}
	else if (tag=="enabled" && FSmEnabling)
	{
		FSmEnabling = false;
		FSmEnabled = true;
		FSmId = AElem.attribute("id");
		FSmResumable = !FSmId.isEmpty() && (AElem.attribute("resume")=="true" || AElem.attribute("resume")=="1");
		FSmMaxResume = AElem.attribute("max").toInt();
		FSmInHandled = 0;
		FSmOutAcked = 0;
		FSmOutQueue.clear();
		FSmAckRequested = false;
		LOG_STRM_INFO(streamJid(),QString("XMPP stream management enabled, id=%1, resumable=%2, max=%3").arg(FSmId).arg(FSmResumable).arg(FSmMaxResume));
		openStream();
	}
	else if (tag=="resumed" && FSmResumeSent)
	{
		FSmResumeSent = false;
		FSmResuming = false;
		FSmResumeTimer.stop();
		processSmAck(AElem.attribute("h").toUInt());

		setStreamState(SS_ONLINE);
		setKeepAliveTimerActive(true);
		clearActiveFeatures();
		LOG_STRM_INFO(streamJid(),QString("XMPP stream resumed, id=%1, resending %2 stanza(s)").arg(FSmId).arg(FSmOutQueue.count()));

		foreach(const SmOutStanza &out, FSmOutQueue)
			sendData(out.data);
		sendSmRequest();

		FCsiSupported = !findServerFeature(NS_CLIENT_STATE).isNull();
		updateClientState(true);
	}
	else if (tag == "failed")
	{
		if (AElem.hasAttribute("h"))
			processSmAck(AElem.attribute("h").toUInt());

		if (FSmEnabling)
		{
			LOG_STRM_WARNING(streamJid(),"Failed to enable XMPP stream management");
			FSmEnabling = false;
			openStream();
		}
		else if (FSmResumeSent)
		{
			LOG_STRM_WARNING(streamJid(),QString("Server refused to resume XMPP stream, id=%1").arg(FSmId));
			failSmResume(XmppError(IERR_XMPPSTREAM_RESUME_FAILED));
		}
		else
		{
			LOG_STRM_WARNING(streamJid(),"XMPP stream management failure reported by server");
		}
	}
}

void XmppStream::processSmAck(quint32 AHandled)
{
	quint32 count = AHandled - FSmOutAcked;
	if (count > (quint32)FSmOutQueue.count())
	{
		LOG_STRM_WARNING(streamJid(),QString("Server acknowledged more stanzas than were sent, h=%1, acked=%2, queued=%3").arg(AHandled).arg(FSmOutAcked).arg(FSmOutQueue.count()));
		count = FSmOutQueue.count();
	}
	FSmOutQueue.erase(FSmOutQueue.begin(),FSmOutQueue.begin()+count);
	FSmOutAcked = AHandled;
	FSmAckRequested = false;
}

void XmppStream::queueSmStanza(const Stanza &AStanza, const QByteArray &AData)
{
	SmOutStanza out;
	out.data = AData;
	out.resendable = AStanza.kind()=="message" && !AStanza.firstElement("body").isNull();
	out.queued = QDateTime::currentDateTime();
	FSmOutQueue.append(out);
}

void XmppStream::startSmResume(const XmppError &AReason)
{
	int seconds = FSmMaxResume>0 ? FSmMaxResume : SM_DEFAULT_RESUME_TIME;
	LOG_STRM_WARNING(streamJid(),QString("XMPP stream connection lost, trying to resume for %1 seconds: %2").arg(seconds).arg(AReason.condition()));

	FSmResuming = true;
	FSmResumeSent = false;
	FSmResumeAttempt = 0;
	FSmAckRequested = false;
	FSmResumeDeadline = QDateTime::currentDateTime().addSecs(seconds);
	FSmRequestTimer.stop();

	setStreamState(SS_CONNECTING);
	resetConnectionState();
	if (FConnection->isOpen())
		FConnection->disconnectFromHost();
	else
		scheduleSmResume();
}

void XmppStream::scheduleSmResume()
{
	static const int delays[] = { 500, 2000, 5000, 10000, 20000, 30000 };
	static const int delaysCount = sizeof(delays)/sizeof(delays[0]);

	int delay = delays[qMin(FSmResumeAttempt,delaysCount-1)];
	if (QDateTime::currentDateTime().addMSecs(delay) < FSmResumeDeadline)
	{
		LOG_STRM_DEBUG(streamJid(),QString("Next XMPP stream resume attempt in %1 ms").arg(delay));
		FSmResumeTimer.start(delay);
	}
	else
	{
		failSmResume(XmppError(IERR_XMPPSTREAM_RESUME_FAILED));
	}
}

void XmppStream::failSmResume(const XmppError &AError)
{
	LOG_STRM_WARNING(streamJid(),QString("Failed to resume XMPP stream: %1").arg(AError.condition()));
	finishSmSession(true);
	abort(AError);

	// When failed inside a disconnect notification the connection will not report it again
	if (FStreamState==SS_ERROR && !FConnection->isOpen())
		QMetaObject::invokeMethod(this,"onConnectionDisconnected",Qt::QueuedConnection);
}

void XmppStream::finishSmSession(bool AKeepMessages)
{
	if (AKeepMessages)
	{
		foreach(const SmOutStanza &out, FSmOutQueue)
			if (out.resendable)
				FSmResendMessages.append(out);
	}

	FSmEnabled = false;
	FSmEnabling = false;
	FSmResumable = false;
	FSmResuming = false;
	FSmResumeSent = false;
	FSmAckRequested = false;
	FSmId.clear();
	FSmMaxResume = 0;
	FSmInHandled = 0;
	FSmOutAcked = 0;
	FSmOutQueue.clear();
	FSmRequestTimer.stop();
	FSmResumeTimer.stop();
}

void XmppStream::flushSmResendMessages()
{
	if (!FSmResendMessages.isEmpty())
	{
		QDateTime oldest = QDateTime::currentDateTime().addSecs(-SM_RESEND_MAX_AGE);
		QList<SmOutStanza> messages = FSmResendMessages;
		FSmResendMessages.clear();

		int sent = 0;
		foreach(const SmOutStanza &out, messages)
		{
			if (out.queued >= oldest && isOpen())
			{
				if (FSmEnabled)
					FSmOutQueue.append(out);
				sendData(out.data);
				sent++;
			}
		}
		if (FSmEnabled && sent>0)
			FSmRequestTimer.start(SM_REQUEST_DELAY);
		LOG_STRM_INFO(streamJid(),QString("Resent %1 of %2 unacknowledged message(s) from the previous session").arg(sent).arg(messages.count()));
	}
}

void XmppStream::connectionLost(const XmppError &AError)
{
	if (FStreamState==SS_ONLINE && canResumeSm())
		startSmResume(AError);
	else
		abort(AError);
}

void XmppStream::updateClientState(bool AForce)
{
	if (FCsiSupported && FStreamState==SS_ONLINE && !FSmResuming && (AForce || FCsiSentActive!=FCsiActive))
	{
		if (Options::node(OPV_XMPPSTREAMS_CLIENTSTATE).value().toBool() || !FCsiSentActive)
		{
			bool active = FCsiActive || !Options::node(OPV_XMPPSTREAMS_CLIENTSTATE).value().toBool();
			Stanza state(active ? "active" : "inactive",NS_CLIENT_STATE);
			if (sendStanza(state) >= 0)
			{
				FCsiSentActive = active;
				LOG_STRM_DEBUG(streamJid(),QString("Client state indicated, active=%1").arg(active));
			}
		}
	}
}

void XmppStream::onApplicationStateChanged(Qt::ApplicationState AState)
{
	if (AState == Qt::ApplicationActive)
	{
		FCsiInactiveTimer.stop();
		FCsiActive = true;
		updateClientState(false);
	}
	else if (FCsiActive && !FCsiInactiveTimer.isActive())
	{
		FCsiInactiveTimer.start(CSI_INACTIVE_DELAY);
	}
}

void XmppStream::onClientInactiveTimeout()
{
	FCsiActive = false;
	updateClientState(false);
}

void XmppStream::onSmRequestTimeout()
{
	sendSmRequest();
}

void XmppStream::onSmResumeTimeout()
{
	if (FSmResuming)
	{
		if (QDateTime::currentDateTime() >= FSmResumeDeadline)
		{
			failSmResume(XmppError(IERR_XMPPSTREAM_RESUME_FAILED));
		}
		else
		{
			FSmResumeAttempt++;
			LOG_STRM_INFO(streamJid(),QString("Reconnecting to resume XMPP stream, attempt=%1").arg(FSmResumeAttempt));
			if (!FConnection->connectToHost())
				scheduleSmResume();
		}
	}
}
