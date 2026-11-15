#include "mediasession.h"

#include <QUrl>
#include <QPair>
#include <QMutex>
#include <QMutexLocker>
#include <QMetaObject>
#include <utils/logger.h>

#include <gst/gst.h>
#include <gst/sdp/sdp.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#define OPUS_DEFAULT_PAYLOAD   111

static MediaSession::TestMode GlobalTestMode = MediaSession::RealDevices;

// GStreamer calls back from its own threads at any time, also while the session is being destroyed.
// The link outlives both sides and tells the callbacks whether the session is still there.
struct MediaLink
{
	MediaLink() {
		refs = 1;
		session = NULL;
		webrtc = NULL;
		answerer = false;
		offerRequested = false;
		audioReported = false;
	}
	gint refs;
	QMutex mutex;
	MediaSession *session;
	GstElement *webrtc;
	bool answerer;
	bool offerRequested;
	bool audioReported;
};

static MediaLink *linkRef(MediaLink *ALink)
{
	g_atomic_int_inc(&ALink->refs);
	return ALink;
}

static void linkUnref(gpointer AData)
{
	MediaLink *link = static_cast<MediaLink *>(AData);
	if (g_atomic_int_dec_and_test(&link->refs))
		delete link;
}

static void linkUnrefClosure(gpointer AData, GClosure *AClosure)
{
	Q_UNUSED(AClosure);
	linkUnref(AData);
}

static void objectUnrefClosure(gpointer AData, GClosure *AClosure)
{
	Q_UNUSED(AClosure);
	gst_object_unref(AData);
}

static GstElement *linkWebrtc(MediaLink *ALink)
{
	QMutexLocker locker(&ALink->mutex);
	return ALink->webrtc!=NULL ? GST_ELEMENT(gst_object_ref(ALink->webrtc)) : NULL;
}

static void postError(MediaLink *ALink, const QString &AError)
{
	QMutexLocker locker(&ALink->mutex);
	if (ALink->session)
		QMetaObject::invokeMethod(ALink->session,"handleError",Qt::QueuedConnection,Q_ARG(QString,AError));
}

static QString promiseError(GstPromise *APromise, const char *AField)
{
	if (gst_promise_wait(APromise) != GST_PROMISE_RESULT_REPLIED)
		return QString("%1 request was interrupted").arg(AField);

	const GstStructure *reply = gst_promise_get_reply(APromise);
	if (reply!=NULL && gst_structure_has_field(reply,"error"))
	{
		GError *error = NULL;
		gst_structure_get(reply,"error",G_TYPE_ERROR,&error,NULL);
		QString text = error!=NULL ? QString::fromUtf8(error->message) : QString("Unknown error");
		g_clear_error(&error);
		return text;
	}
	return QString();
}

static void onLocalDescriptionSet(GstPromise *APromise, gpointer AData)
{
	MediaLink *link = static_cast<MediaLink *>(AData);
	QString error = promiseError(APromise,"Local description");
	if (!error.isEmpty())
		postError(link,QString("Failed to apply local session description: %1").arg(error));
	gst_promise_unref(APromise);
}

// Called with the created offer or answer
static void onDescriptionCreated(GstPromise *APromise, gpointer AData)
{
	MediaLink *link = static_cast<MediaLink *>(AData);
	const char *field = link->answerer ? "answer" : "offer";

	QString error = promiseError(APromise,field);
	GstWebRTCSessionDescription *desc = NULL;
	if (error.isEmpty())
	{
		gst_structure_get(gst_promise_get_reply(APromise),field,GST_TYPE_WEBRTC_SESSION_DESCRIPTION,&desc,NULL);
		if (desc == NULL)
			error = "No session description was created";
	}
	gst_promise_unref(APromise);

	if (desc != NULL)
	{
		GstElement *webrtc = linkWebrtc(link);
		if (webrtc != NULL)
		{
			GstPromise *promise = gst_promise_new_with_change_func(onLocalDescriptionSet,linkRef(link),linkUnref);
			g_signal_emit_by_name(webrtc,"set-local-description",desc,promise);
			gst_object_unref(webrtc);

			gchar *text = gst_sdp_message_as_text(desc->sdp);
			QString sdp = QString::fromUtf8(text);
			g_free(text);

			QMutexLocker locker(&link->mutex);
			if (link->session)
				QMetaObject::invokeMethod(link->session,"handleLocalDescription",Qt::QueuedConnection,Q_ARG(QString,QString(field)),Q_ARG(QString,sdp));
		}
		gst_webrtc_session_description_free(desc);
	}
	else
	{
		postError(link,QString("Failed to create session %1: %2").arg(field,error));
	}
}

static void onRemoteDescriptionSet(GstPromise *APromise, gpointer AData)
{
	MediaLink *link = static_cast<MediaLink *>(AData);
	QString error = promiseError(APromise,"Remote description");
	gst_promise_unref(APromise);

	if (!error.isEmpty())
	{
		postError(link,QString("Failed to apply remote session description: %1").arg(error));
	}
	else if (link->answerer)
	{
		GstElement *webrtc = linkWebrtc(link);
		if (webrtc != NULL)
		{
			GstPromise *promise = gst_promise_new_with_change_func(onDescriptionCreated,linkRef(link),linkUnref);
			g_signal_emit_by_name(webrtc,"create-answer",NULL,promise);
			gst_object_unref(webrtc);
		}
	}
}

static void onNegotiationNeeded(GstElement *AWebrtc, gpointer AData)
{
	MediaLink *link = static_cast<MediaLink *>(AData);
	{
		QMutexLocker locker(&link->mutex);
		if (link->answerer || link->offerRequested || link->session==NULL)
			return;
		link->offerRequested = true;
	}
	GstPromise *promise = gst_promise_new_with_change_func(onDescriptionCreated,linkRef(link),linkUnref);
	g_signal_emit_by_name(AWebrtc,"create-offer",NULL,promise);
}

static void onIceCandidate(GstElement *AWebrtc, guint AMLineIndex, gchar *ACandidate, gpointer AData)
{
	Q_UNUSED(AWebrtc);
	MediaLink *link = static_cast<MediaLink *>(AData);
	QMutexLocker locker(&link->mutex);
	if (link->session && ACandidate!=NULL && ACandidate[0]!='\0')
		QMetaObject::invokeMethod(link->session,"handleLocalCandidate",Qt::QueuedConnection,Q_ARG(int,(int)AMLineIndex),Q_ARG(QString,QString::fromUtf8(ACandidate)));
}

static void onConnectionStateChanged(GObject *AWebrtc, GParamSpec *ASpec, gpointer AData)
{
	Q_UNUSED(ASpec);
	MediaLink *link = static_cast<MediaLink *>(AData);
	GstWebRTCPeerConnectionState state = GST_WEBRTC_PEER_CONNECTION_STATE_NEW;
	g_object_get(AWebrtc,"connection-state",&state,NULL);

	QMutexLocker locker(&link->mutex);
	if (link->session)
		QMetaObject::invokeMethod(link->session,"handleConnectionState",Qt::QueuedConnection,Q_ARG(int,(int)state));
}

static void onIceConnectionStateChanged(GObject *AWebrtc, GParamSpec *ASpec, gpointer AData)
{
	Q_UNUSED(ASpec);
	MediaLink *link = static_cast<MediaLink *>(AData);
	GstWebRTCICEConnectionState state = GST_WEBRTC_ICE_CONNECTION_STATE_NEW;
	g_object_get(AWebrtc,"ice-connection-state",&state,NULL);

	// The aggregated connection state may not report a failed ICE transport
	if (state == GST_WEBRTC_ICE_CONNECTION_STATE_FAILED)
	{
		QMutexLocker locker(&link->mutex);
		if (link->session)
			QMetaObject::invokeMethod(link->session,"handleConnectionState",Qt::QueuedConnection,Q_ARG(int,(int)GST_WEBRTC_PEER_CONNECTION_STATE_FAILED));
	}
}

static GstPadProbeReturn onRemoteAudioBuffer(GstPad *APad, GstPadProbeInfo *AInfo, gpointer AData)
{
	Q_UNUSED(APad); Q_UNUSED(AInfo);
	MediaLink *link = static_cast<MediaLink *>(AData);
	QMutexLocker locker(&link->mutex);
	if (link->session && !link->audioReported)
	{
		link->audioReported = true;
		QMetaObject::invokeMethod(link->session,"handleRemoteAudio",Qt::QueuedConnection);
	}
	return GST_PAD_PROBE_REMOVE;
}

static GstBusSyncReply onBusMessage(GstBus *ABus, GstMessage *AMessage, gpointer AData)
{
	Q_UNUSED(ABus);
	MediaLink *link = static_cast<MediaLink *>(AData);
	if (GST_MESSAGE_TYPE(AMessage) == GST_MESSAGE_ERROR)
	{
		GError *error = NULL;
		gchar *debug = NULL;
		gst_message_parse_error(AMessage,&error,&debug);
		QString text = QString("%1: %2").arg(QString::fromUtf8(GST_OBJECT_NAME(GST_MESSAGE_SRC(AMessage))),error!=NULL ? QString::fromUtf8(error->message) : QString("Unknown error"));
		g_clear_error(&error);
		g_free(debug);
		postError(link,text);
	}
	gst_message_unref(AMessage);
	return GST_BUS_DROP;
}

struct MediaSessionPrivate
{
	MediaSessionPrivate() {
		link = NULL;
		pipeline = NULL;
		webrtc = NULL;
		volume = NULL;
		playback = NULL;
		muted = false;
		connected = false;
		remoteDescriptionSet = false;
	}
	MediaLink *link;
	GstElement *pipeline;
	GstElement *webrtc;
	GstElement *volume;
	GstElement *playback;
	bool muted;
	bool connected;
	bool remoteDescriptionSet;
	QString error;
	QList<QPair<int, QString> > pendingCandidates;

	static void onPadAdded(GstElement *AWebrtc, GstPad *APad, gpointer AData);
};

// Incoming audio shows up as a new pad once the remote description is applied
void MediaSessionPrivate::onPadAdded(GstElement *AWebrtc, GstPad *APad, gpointer AData)
{
	if (GST_PAD_DIRECTION(APad) != GST_PAD_SRC)
		return;

	GstElement *playback = GST_ELEMENT(AData);
	GstPad *sinkPad = gst_element_get_static_pad(playback,"sink");
	if (sinkPad!=NULL && !gst_pad_is_linked(sinkPad) && gst_pad_link(APad,sinkPad)==GST_PAD_LINK_OK)
	{
		gst_object_unref(sinkPad);
		return;
	}
	if (sinkPad != NULL)
		gst_object_unref(sinkPad);

	// Anything that is not the audio stream we play is discarded
	GstElement *pipeline = GST_ELEMENT(gst_element_get_parent(AWebrtc));
	GstElement *fakeSink = gst_element_factory_make("fakesink",NULL);
	if (pipeline!=NULL && fakeSink!=NULL)
	{
		g_object_set(fakeSink,"sync",FALSE,"async",FALSE,NULL);
		gst_bin_add(GST_BIN(pipeline),fakeSink);
		gst_element_sync_state_with_parent(fakeSink);
		GstPad *fakePad = gst_element_get_static_pad(fakeSink,"sink");
		gst_pad_link(APad,fakePad);
		gst_object_unref(fakePad);
	}
	else if (fakeSink != NULL)
	{
		gst_object_unref(fakeSink);
	}
	if (pipeline != NULL)
		gst_object_unref(pipeline);
}

static bool initGStreamer()
{
	static bool inited = false;
	if (!inited)
		inited = gst_init_check(NULL,NULL,NULL);
	return inited;
}

static bool hasElement(const char *AName)
{
	GstElementFactory *factory = gst_element_factory_find(AName);
	if (factory != NULL)
	{
		gst_object_unref(factory);
		return true;
	}
	return false;
}

static QString iceServerUri(const MediaIceServer &AServer)
{
	QString host = AServer.host.contains(':') && !AServer.host.startsWith('[') ? "["+AServer.host+"]" : AServer.host;
	QString uri = AServer.type + "://";
	if (AServer.type != "stun")
	{
		uri += QString::fromLatin1(QUrl::toPercentEncoding(AServer.username));
		uri += ":" + QString::fromLatin1(QUrl::toPercentEncoding(AServer.password)) + "@";
	}
	uri += host;
	if (AServer.port > 0)
		uri += ":" + QString::number(AServer.port);
	if (AServer.type!="stun" && AServer.transport=="tcp")
		uri += "?transport=tcp";
	return uri;
}

MediaSession::MediaSession(QObject *AParent) : QObject(AParent)
{
	d = new MediaSessionPrivate;
}

MediaSession::~MediaSession()
{
	stop();
	delete d;
}

QStringList MediaSession::missingElements()
{
	static const char *required[] = {
		"webrtcbin", "nicesrc", "nicesink", "dtlssrtpenc", "dtlssrtpdec", "srtpenc", "srtpdec", "rtpbin",
		"opusenc", "opusdec", "rtpopuspay", "rtpopusdepay", "audioconvert", "audioresample", "volume",
		"queue", "capsfilter", "autoaudiosrc", "autoaudiosink", "fakesink", NULL
	};

	QStringList missing;
	if (!initGStreamer())
	{
		missing.append("gstreamer");
		return missing;
	}
	for (int i=0; required[i]!=NULL; i++)
		if (!hasElement(required[i]))
			missing.append(required[i]);
	return missing;
}

void MediaSession::setTestMode(TestMode AMode)
{
	GlobalTestMode = AMode;
}

bool MediaSession::isStarted() const
{
	return d->pipeline != NULL;
}

bool MediaSession::start(Role ARole, const QList<MediaIceServer> &AServers, int AOpusPayload)
{
	if (d->pipeline != NULL)
		return false;

	QStringList missing = missingElements();
	if (!missing.isEmpty())
	{
		d->error = QString("GStreamer elements are not installed: %1").arg(missing.join(", "));
		return false;
	}

	int payload = AOpusPayload>0 && AOpusPayload<128 ? AOpusPayload : OPUS_DEFAULT_PAYLOAD;
	bool testMode = GlobalTestMode==TestDevices;

	// Echo cancellation needs both halves: the probe hears what goes to the speakers
	bool echoCancel = hasElement("webrtcdsp") && hasElement("webrtcechoprobe");

	QString rawCaps = "audio/x-raw,format=S16LE,layout=interleaved,rate=48000,channels=1";
	QString launch;
	launch += testMode ? "audiotestsrc is-live=true wave=sine freq=440 volume=0.3" : "autoaudiosrc";
	launch += " ! queue leaky=downstream max-size-buffers=0 max-size-bytes=0 max-size-time=200000000";
	launch += " ! audioconvert ! audioresample ! " + rawCaps;
	if (echoCancel)
		launch += " ! webrtcdsp probe=callechoprobe";
	launch += " ! volume name=callvolume";
	launch += " ! opusenc audio-type=voice bitrate=32000 inband-fec=true packet-loss-percentage=5 frame-size=20";
	launch += QString(" ! rtpopuspay pt=%1").arg(payload);
	launch += QString(" ! application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=%1").arg(payload);
	launch += " ! webrtcbin name=callwebrtc";

	launch += "  queue name=callplayback ! rtpopusdepay ! opusdec name=calldecoder plc=true ! audioconvert ! audioresample";
	if (echoCancel)
		launch += " ! " + rawCaps + " ! webrtcechoprobe name=callechoprobe ! audioconvert";
	launch += testMode ? " ! fakesink sync=true async=false" : " ! autoaudiosink";

	GError *error = NULL;
	GstElement *pipeline = gst_parse_launch(launch.toUtf8().constData(),&error);
	if (pipeline==NULL || error!=NULL)
	{
		d->error = error!=NULL ? QString::fromUtf8(error->message) : QString("Failed to create media pipeline");
		g_clear_error(&error);
		if (pipeline != NULL)
			gst_object_unref(pipeline);
		return false;
	}

	d->pipeline = pipeline;
	d->webrtc = gst_bin_get_by_name(GST_BIN(pipeline),"callwebrtc");
	d->volume = gst_bin_get_by_name(GST_BIN(pipeline),"callvolume");
	d->playback = gst_bin_get_by_name(GST_BIN(pipeline),"callplayback");
	g_object_set(d->volume,"mute",d->muted ? TRUE : FALSE,NULL);

	gst_util_set_object_arg(G_OBJECT(d->webrtc),"bundle-policy","max-bundle");
	bool stunSet = false;
	foreach(const MediaIceServer &server, AServers)
	{
		if (server.host.isEmpty())
			continue;
		if (server.type == "stun")
		{
			if (!stunSet && server.transport!="tcp")
			{
				g_object_set(d->webrtc,"stun-server",iceServerUri(server).toUtf8().constData(),NULL);
				stunSet = true;
			}
		}
		else if (server.type=="turn" || server.type=="turns")
		{
			gboolean added = FALSE;
			g_signal_emit_by_name(d->webrtc,"add-turn-server",iceServerUri(server).toUtf8().constData(),&added);
			if (!added)
				LOG_WARNING(QString("Relay server was not accepted, type=%1, host=%2, port=%3").arg(server.type,server.host).arg(server.port));
		}
	}

	d->link = new MediaLink;
	d->link->session = this;
	d->link->answerer = ARole==Answerer;
	d->link->webrtc = GST_ELEMENT(gst_object_ref(d->webrtc));

	g_signal_connect_data(d->webrtc,"on-negotiation-needed",G_CALLBACK(onNegotiationNeeded),linkRef(d->link),linkUnrefClosure,(GConnectFlags)0);
	g_signal_connect_data(d->webrtc,"on-ice-candidate",G_CALLBACK(onIceCandidate),linkRef(d->link),linkUnrefClosure,(GConnectFlags)0);
	g_signal_connect_data(d->webrtc,"notify::connection-state",G_CALLBACK(onConnectionStateChanged),linkRef(d->link),linkUnrefClosure,(GConnectFlags)0);
	g_signal_connect_data(d->webrtc,"notify::ice-connection-state",G_CALLBACK(onIceConnectionStateChanged),linkRef(d->link),linkUnrefClosure,(GConnectFlags)0);
	g_signal_connect_data(d->webrtc,"pad-added",G_CALLBACK(MediaSessionPrivate::onPadAdded),gst_object_ref(d->playback),objectUnrefClosure,(GConnectFlags)0);

	GstElement *decoder = gst_bin_get_by_name(GST_BIN(pipeline),"calldecoder");
	GstPad *decoderPad = gst_element_get_static_pad(decoder,"src");
	gst_pad_add_probe(decoderPad,GST_PAD_PROBE_TYPE_BUFFER,onRemoteAudioBuffer,linkRef(d->link),linkUnref);
	gst_object_unref(decoderPad);
	gst_object_unref(decoder);

	GstBus *bus = gst_element_get_bus(pipeline);
	gst_bus_set_sync_handler(bus,onBusMessage,linkRef(d->link),linkUnref);
	gst_object_unref(bus);

	if (gst_element_set_state(pipeline,GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
	{
		d->error = "Failed to start audio devices";
		stop();
		return false;
	}
	return true;
}

void MediaSession::stop()
{
	if (d->link != NULL)
	{
		GstElement *webrtc = NULL;
		{
			QMutexLocker locker(&d->link->mutex);
			d->link->session = NULL;
			webrtc = d->link->webrtc;
			d->link->webrtc = NULL;
		}
		if (webrtc != NULL)
			gst_object_unref(webrtc);
		linkUnref(d->link);
		d->link = NULL;
	}

	if (d->pipeline != NULL)
	{
		GstBus *bus = gst_element_get_bus(d->pipeline);
		gst_bus_set_sync_handler(bus,NULL,NULL,NULL);
		gst_object_unref(bus);

		gst_element_set_state(d->pipeline,GST_STATE_NULL);
		gst_object_unref(d->playback);
		gst_object_unref(d->volume);
		gst_object_unref(d->webrtc);
		gst_object_unref(d->pipeline);
		d->playback = NULL;
		d->volume = NULL;
		d->webrtc = NULL;
		d->pipeline = NULL;
	}

	d->connected = false;
	d->remoteDescriptionSet = false;
	d->pendingCandidates.clear();
}

bool MediaSession::setRemoteDescription(const QString &AType, const QString &ASdp)
{
	if (d->pipeline==NULL || d->remoteDescriptionSet)
		return false;

	GstSDPMessage *sdp = NULL;
	if (gst_sdp_message_new_from_text(ASdp.toUtf8().constData(),&sdp) != GST_SDP_OK)
	{
		d->error = "Remote session description is not valid";
		return false;
	}

	GstWebRTCSDPType type = AType=="offer" ? GST_WEBRTC_SDP_TYPE_OFFER : GST_WEBRTC_SDP_TYPE_ANSWER;
	GstWebRTCSessionDescription *desc = gst_webrtc_session_description_new(type,sdp);
	GstPromise *promise = gst_promise_new_with_change_func(onRemoteDescriptionSet,linkRef(d->link),linkUnref);
	g_signal_emit_by_name(d->webrtc,"set-remote-description",desc,promise);
	gst_webrtc_session_description_free(desc);

	d->remoteDescriptionSet = true;
	for (int i=0; i<d->pendingCandidates.count(); i++)
		g_signal_emit_by_name(d->webrtc,"add-ice-candidate",(guint)d->pendingCandidates.at(i).first,d->pendingCandidates.at(i).second.toUtf8().constData());
	d->pendingCandidates.clear();

	return true;
}

void MediaSession::addRemoteCandidate(int AMLineIndex, const QString &ACandidate)
{
	if (AMLineIndex<0 || ACandidate.isEmpty())
		return;
	if (d->pipeline!=NULL && d->remoteDescriptionSet)
		g_signal_emit_by_name(d->webrtc,"add-ice-candidate",(guint)AMLineIndex,ACandidate.toUtf8().constData());
	else
		d->pendingCandidates.append(qMakePair(AMLineIndex,ACandidate));
}

bool MediaSession::isMuted() const
{
	return d->muted;
}

void MediaSession::setMuted(bool AMuted)
{
	d->muted = AMuted;
	if (d->volume != NULL)
		g_object_set(d->volume,"mute",AMuted ? TRUE : FALSE,NULL);
}

QString MediaSession::errorString() const
{
	return d->error;
}

void MediaSession::handleLocalDescription(const QString &AType, const QString &ASdp)
{
	if (d->pipeline != NULL)
		emit localDescriptionCreated(AType,ASdp);
}

void MediaSession::handleLocalCandidate(int AMLineIndex, const QString &ACandidate)
{
	if (d->pipeline != NULL)
		emit localCandidateFound(AMLineIndex,ACandidate);
}

void MediaSession::handleConnectionState(int AState)
{
	if (d->pipeline == NULL)
		return;

	if (AState == GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED)
	{
		if (!d->connected)
		{
			d->connected = true;
			emit connected();
		}
	}
	else if (AState == GST_WEBRTC_PEER_CONNECTION_STATE_DISCONNECTED)
	{
		if (d->connected)
		{
			d->connected = false;
			emit disconnected();
		}
	}
	else if (AState == GST_WEBRTC_PEER_CONNECTION_STATE_FAILED)
	{
		d->connected = false;
		emit connectionFailed();
	}
}

void MediaSession::handleError(const QString &AError)
{
	if (d->pipeline != NULL)
	{
		d->error = AError;
		emit failed(AError);
	}
}

void MediaSession::handleRemoteAudio()
{
	if (d->pipeline == NULL)
		return;

	// Decoded audio proves that ICE and DTLS are up even if the state was not reported yet
	if (!d->connected)
	{
		d->connected = true;
		emit connected();
	}
	emit remoteAudioStarted();
}
