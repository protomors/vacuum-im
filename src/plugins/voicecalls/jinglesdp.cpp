#include "jinglesdp.h"

#include <QMap>
#include <QList>
#include <QPair>
#include <QUuid>
#include <QRandomGenerator>
#include <definitions/namespaces.h>

#define SDP_EOL "\r\n"

typedef QPair<QString, QString> SdpAttribute;

struct SdpMedia
{
	QString media;
	QString port;
	QString protocol;
	QStringList formats;
	QList<SdpAttribute> attributes;
};

struct SdpSession
{
	QList<SdpAttribute> attributes;
	QList<SdpMedia> media;
};

static SdpAttribute parseAttribute(const QString &ALine)
{
	int colon = ALine.indexOf(':');
	if (colon < 0)
		return SdpAttribute(ALine.trimmed(),QString());
	return SdpAttribute(ALine.left(colon).trimmed(),ALine.mid(colon+1).trimmed());
}

static SdpSession parseSdp(const QString &ASdp)
{
	SdpSession session;
	foreach(QString line, ASdp.split('\n'))
	{
		line = line.trimmed();
		if (line.length()<2 || line.at(1)!='=')
			continue;

		QChar type = line.at(0);
		QString value = line.mid(2);
		if (type == 'm')
		{
			QStringList parts = value.split(' ',Qt::SkipEmptyParts);
			SdpMedia media;
			media.media = parts.value(0);
			media.port = parts.value(1);
			media.protocol = parts.value(2);
			media.formats = parts.mid(3);
			session.media.append(media);
		}
		else if (type == 'a')
		{
			if (session.media.isEmpty())
				session.attributes.append(parseAttribute(value));
			else
				session.media.last().attributes.append(parseAttribute(value));
		}
	}
	return session;
}

static QString findAttribute(const QList<SdpAttribute> &AAttributes, const QString &AName, bool *AFound = NULL)
{
	foreach(const SdpAttribute &attr, AAttributes)
	{
		if (attr.first == AName)
		{
			if (AFound)
				*AFound = true;
			return attr.second;
		}
	}
	if (AFound)
		*AFound = false;
	return QString();
}

static QString mediaAttribute(const SdpSession &ASession, const SdpMedia &AMedia, const QString &AName)
{
	bool found = false;
	QString value = findAttribute(AMedia.attributes,AName,&found);
	return found ? value : findAttribute(ASession.attributes,AName);
}

static bool hasAttribute(const QList<SdpAttribute> &AAttributes, const QString &AName)
{
	bool found = false;
	findAttribute(AAttributes,AName,&found);
	return found;
}

static QDomElement appendElement(QDomElement &AParent, const QString &AName, const QString &ANamespace = QString())
{
	QDomDocument doc = AParent.ownerDocument();
	QDomElement elem = ANamespace.isEmpty() ? doc.createElement(AName) : doc.createElementNS(ANamespace,AName);
	return AParent.appendChild(elem).toElement();
}

static QList<QDomElement> childElements(const QDomElement &AParent, const QString &AName, const QString &ANamespace)
{
	QList<QDomElement> elems;
	for (QDomElement elem = AParent.firstChildElement(AName); !elem.isNull(); elem = elem.nextSiblingElement(AName))
		if (elem.namespaceURI() == ANamespace)
			elems.append(elem);
	return elems;
}

// Values taken from a remote party end up in SDP lines, so they must not be able to start a new line
static bool isSdpSafe(const QString &AValue)
{
	foreach(const QChar &ch, AValue)
		if (ch.unicode()<0x20 || ch.unicode()==0x7F)
			return false;
	return true;
}

static bool isSdpToken(const QString &AValue)
{
	if (AValue.isEmpty())
		return false;
	foreach(const QChar &ch, AValue)
		if (ch.unicode()<=0x20 || ch.unicode()>=0x7F)
			return false;
	return true;
}

static bool isNumber(const QString &AValue)
{
	if (AValue.isEmpty())
		return false;
	foreach(const QChar &ch, AValue)
		if (ch<'0' || ch>'9')
			return false;
	return true;
}

static bool setError(QString *AError, const QString &AText)
{
	if (AError)
		*AError = AText;
	return false;
}

static QString sendersFromDirection(const QList<SdpAttribute> &AAttributes, bool ALocalIsInitiator)
{
	if (hasAttribute(AAttributes,"sendonly"))
		return ALocalIsInitiator ? "initiator" : "responder";
	if (hasAttribute(AAttributes,"recvonly"))
		return ALocalIsInitiator ? "responder" : "initiator";
	if (hasAttribute(AAttributes,"inactive"))
		return "none";
	return "both";
}

static QString directionFromSenders(const QString &ASenders, bool ARemoteIsInitiator)
{
	if (ASenders == "initiator")
		return ARemoteIsInitiator ? "sendonly" : "recvonly";
	if (ASenders == "responder")
		return ARemoteIsInitiator ? "recvonly" : "sendonly";
	if (ASenders == "none")
		return "inactive";
	return "sendrecv";
}

bool JingleSdp::sdpToJingle(const QString &ASdp, bool ALocalIsInitiator, QDomElement &AJingle, QStringList &AContentNames, QString *AError)
{
	SdpSession session = parseSdp(ASdp);
	if (session.media.isEmpty())
		return setError(AError,"Session description has no media");

	AContentNames.clear();

	// a=group:BUNDLE mid1 mid2
	foreach(const SdpAttribute &attr, session.attributes)
	{
		if (attr.first == "group")
		{
			QStringList parts = attr.second.split(' ',Qt::SkipEmptyParts);
			if (parts.count() > 1)
			{
				QDomElement groupElem = appendElement(AJingle,"group",NS_JINGLE_APPS_GROUPING);
				groupElem.setAttribute("semantics",parts.takeFirst());
				foreach(const QString &mid, parts)
					appendElement(groupElem,"content").setAttribute("name",mid);
			}
		}
	}

	int index = 0;
	foreach(const SdpMedia &media, session.media)
	{
		QString mid = findAttribute(media.attributes,"mid");
		if (mid.isEmpty())
			mid = QString("%1%2").arg(media.media).arg(index);
		AContentNames.append(mid);
		index++;

		if (media.port == "0")
			continue;

		QDomElement contentElem = appendElement(AJingle,"content");
		contentElem.setAttribute("creator","initiator");
		contentElem.setAttribute("name",mid);
		contentElem.setAttribute("senders",sendersFromDirection(media.attributes,ALocalIsInitiator));

		QDomElement descElem = appendElement(contentElem,"description",NS_JINGLE_APPS_RTP);
		descElem.setAttribute("media",media.media);

		foreach(const QString &format, media.formats)
		{
			QDomElement payloadElem = appendElement(descElem,"payload-type");
			payloadElem.setAttribute("id",format);

			foreach(const SdpAttribute &attr, media.attributes)
			{
				if (attr.first == "rtpmap")
				{
					// <pt> <name>/<clockrate>[/<channels>]
					int space = attr.second.indexOf(' ');
					if (space>0 && attr.second.left(space)==format)
					{
						QStringList parts = attr.second.mid(space+1).trimmed().split('/');
						payloadElem.setAttribute("name",parts.value(0)=="OPUS" ? QString("opus") : parts.value(0));
						if (parts.count() > 1)
							payloadElem.setAttribute("clockrate",parts.value(1));
						if (parts.count()>2 && parts.value(2)!="1")
							payloadElem.setAttribute("channels",parts.value(2));
					}
				}
				else if (attr.first == "fmtp")
				{
					// <pt> <name>=<value>;<name>=<value>
					int space = attr.second.indexOf(' ');
					if (space>0 && attr.second.left(space)==format)
					{
						foreach(const QString &param, attr.second.mid(space+1).split(';',Qt::SkipEmptyParts))
						{
							int eq = param.indexOf('=');
							if (eq > 0)
							{
								QDomElement paramElem = appendElement(payloadElem,"parameter");
								paramElem.setAttribute("name",param.left(eq).trimmed());
								paramElem.setAttribute("value",param.mid(eq+1).trimmed());
							}
						}
					}
				}
				else if (attr.first == "rtcp-fb")
				{
					// <pt> <type> [<subtype>]
					QStringList parts = attr.second.split(' ',Qt::SkipEmptyParts);
					if (parts.count()>1 && parts.value(0)==format)
					{
						QDomElement fbElem = appendElement(payloadElem,"rtcp-fb",NS_JINGLE_APPS_RTP_RTCP_FB);
						fbElem.setAttribute("type",parts.value(1));
						if (parts.count() > 2)
							fbElem.setAttribute("subtype",QStringList(parts.mid(2)).join(" "));
					}
				}
			}
		}

		QMap<QString, QDomElement> sources;
		foreach(const SdpAttribute &attr, media.attributes)
		{
			if (attr.first == "rtcp-fb")
			{
				QStringList parts = attr.second.split(' ',Qt::SkipEmptyParts);
				if (parts.count()>1 && parts.value(0)=="*")
				{
					QDomElement fbElem = appendElement(descElem,"rtcp-fb",NS_JINGLE_APPS_RTP_RTCP_FB);
					fbElem.setAttribute("type",parts.value(1));
					if (parts.count() > 2)
						fbElem.setAttribute("subtype",QStringList(parts.mid(2)).join(" "));
				}
			}
			else if (attr.first == "extmap")
			{
				// <id>[/<direction>] <uri> [<attributes>]
				QStringList parts = attr.second.split(' ',Qt::SkipEmptyParts);
				if (parts.count() > 1)
				{
					QDomElement extElem = appendElement(descElem,"rtp-hdrext",NS_JINGLE_APPS_RTP_HDREXT);
					extElem.setAttribute("id",parts.value(0).section('/',0,0));
					extElem.setAttribute("uri",parts.value(1));
				}
			}
			else if (attr.first == "extmap-allow-mixed")
			{
				appendElement(descElem,"extmap-allow-mixed",NS_JINGLE_APPS_RTP_HDREXT);
			}
			else if (attr.first == "ssrc-group")
			{
				// <semantics> <ssrc> <ssrc>
				QStringList parts = attr.second.split(' ',Qt::SkipEmptyParts);
				if (parts.count() > 1)
				{
					QDomElement groupElem = appendElement(descElem,"ssrc-group",NS_JINGLE_APPS_RTP_SSMA);
					groupElem.setAttribute("semantics",parts.takeFirst());
					foreach(const QString &ssrc, parts)
						appendElement(groupElem,"source").setAttribute("ssrc",ssrc);
				}
			}
			else if (attr.first == "ssrc")
			{
				// <ssrc> <name>[:<value>]
				int space = attr.second.indexOf(' ');
				QString ssrc = space>0 ? attr.second.left(space) : attr.second;
				if (!sources.contains(ssrc))
				{
					QDomElement sourceElem = appendElement(descElem,"source",NS_JINGLE_APPS_RTP_SSMA);
					sourceElem.setAttribute("ssrc",ssrc);
					sources.insert(ssrc,sourceElem);
				}
				if (space > 0)
				{
					SdpAttribute param = parseAttribute(attr.second.mid(space+1));
					if (!param.first.isEmpty() && !param.second.isEmpty())
					{
						QDomElement paramElem = appendElement(sources[ssrc],"parameter");
						paramElem.setAttribute("name",param.first);
						paramElem.setAttribute("value",param.second);
					}
				}
			}
		}

		if (hasAttribute(media.attributes,"rtcp-mux"))
			appendElement(descElem,"rtcp-mux");

		QDomElement transportElem = appendElement(contentElem,"transport",NS_JINGLE_TRANSPORTS_ICE_UDP);
		QString ufrag = mediaAttribute(session,media,"ice-ufrag");
		QString pwd = mediaAttribute(session,media,"ice-pwd");
		if (ufrag.isEmpty() || pwd.isEmpty())
			return setError(AError,"Session description has no ICE credentials");
		transportElem.setAttribute("ufrag",ufrag);
		transportElem.setAttribute("pwd",pwd);

		// <hash> <fingerprint>
		QString fingerprint = mediaAttribute(session,media,"fingerprint");
		int space = fingerprint.indexOf(' ');
		if (space <= 0)
			return setError(AError,"Session description has no DTLS fingerprint");
		QDomElement fingerElem = appendElement(transportElem,"fingerprint",NS_JINGLE_APPS_DTLS);
		fingerElem.setAttribute("hash",fingerprint.left(space));
		fingerElem.setAttribute("setup",mediaAttribute(session,media,"setup"));
		fingerElem.appendChild(fingerElem.ownerDocument().createTextNode(fingerprint.mid(space+1).trimmed()));

		foreach(const SdpAttribute &attr, media.attributes)
			if (attr.first == "candidate")
				candidateToJingle(attr.second,transportElem);
	}

	if (AJingle.firstChildElement("content").isNull())
		return setError(AError,"Session description has no active media");
	return true;
}

QString JingleSdp::jingleToSdp(const QDomElement &AJingle, bool ARemoteIsInitiator, QStringList &AContentNames, QString *AError)
{
	QStringList lines;
	lines.append("v=0");
	lines.append(QString("o=- %1 2 IN IP4 127.0.0.1").arg(QRandomGenerator::global()->generate64() >> 2));
	lines.append("s=-");
	lines.append("t=0 0");

	AContentNames.clear();
	QList<QDomElement> contents = childElements(AJingle,"content",NS_JINGLE);
	if (contents.isEmpty())
	{
		setError(AError,"Jingle element has no contents");
		return QString();
	}

	foreach(const QDomElement &groupElem, childElements(AJingle,"group",NS_JINGLE_APPS_GROUPING))
	{
		QStringList names;
		for (QDomElement nameElem = groupElem.firstChildElement("content"); !nameElem.isNull(); nameElem = nameElem.nextSiblingElement("content"))
			names.append(nameElem.attribute("name"));

		QString semantics = groupElem.attribute("semantics");
		if (!isSdpToken(semantics))
			continue;

		bool valid = !names.isEmpty();
		foreach(const QString &name, names)
			valid = valid && isSdpToken(name);
		if (valid)
			lines.append(QString("a=group:%1 %2").arg(semantics,names.join(" ")));
	}
	lines.append("a=msid-semantic: WMS *");

	foreach(const QDomElement &contentElem, contents)
	{
		QString name = contentElem.attribute("name");
		QDomElement descElem = contentElem.firstChildElement("description");
		QDomElement transportElem = contentElem.firstChildElement("transport");
		if (!isSdpToken(name) || descElem.namespaceURI()!=NS_JINGLE_APPS_RTP || transportElem.namespaceURI()!=NS_JINGLE_TRANSPORTS_ICE_UDP)
		{
			setError(AError,"Jingle content is not an RTP session over ICE-UDP");
			return QString();
		}

		QString media = descElem.attribute("media");
		if (media!="audio" && media!="video")
		{
			setError(AError,QString("Unsupported media type '%1'").arg(media));
			return QString();
		}

		QStringList formats;
		QStringList mediaLines;
		for (QDomElement payloadElem = descElem.firstChildElement("payload-type"); !payloadElem.isNull(); payloadElem = payloadElem.nextSiblingElement("payload-type"))
		{
			QString id = payloadElem.attribute("id");
			QString codec = payloadElem.attribute("name");
			QString clockrate = payloadElem.attribute("clockrate");
			QString channels = payloadElem.attribute("channels");
			if (!isNumber(id) || id.toInt()>127)
			{
				setError(AError,"Jingle payload type has an invalid id");
				return QString();
			}
			formats.append(id);

			if (isSdpToken(codec) && !codec.contains('/') && isNumber(clockrate))
			{
				if (isNumber(channels) && channels!="1")
					mediaLines.append(QString("a=rtpmap:%1 %2/%3/%4").arg(id,codec,clockrate,channels));
				else
					mediaLines.append(QString("a=rtpmap:%1 %2/%3").arg(id,codec,clockrate));
			}

			foreach(const QDomElement &fbElem, childElements(payloadElem,"rtcp-fb",NS_JINGLE_APPS_RTP_RTCP_FB))
			{
				QString type = fbElem.attribute("type");
				QString subtype = fbElem.attribute("subtype");
				if (isSdpToken(type) && isSdpSafe(subtype))
					mediaLines.append(QString("a=rtcp-fb:%1 %2").arg(id,subtype.isEmpty() ? type : type+" "+subtype));
			}

			QStringList params;
			for (QDomElement paramElem = payloadElem.firstChildElement("parameter"); !paramElem.isNull(); paramElem = paramElem.nextSiblingElement("parameter"))
			{
				QString paramName = paramElem.attribute("name");
				QString paramValue = paramElem.attribute("value");
				if (!isSdpSafe(paramName) || !isSdpSafe(paramValue) || paramName.contains(';') || paramValue.contains(';'))
					continue;
				if (!paramName.isEmpty())
					params.append(paramName+"="+paramValue);
				else if (!paramValue.isEmpty())
					params.append(paramValue);
			}
			if (!params.isEmpty())
				mediaLines.append(QString("a=fmtp:%1 %2").arg(id,params.join(";")));
		}
		if (formats.isEmpty())
		{
			setError(AError,"Jingle content has no payload types");
			return QString();
		}

		QString ufrag = transportElem.attribute("ufrag");
		QString pwd = transportElem.attribute("pwd");
		QDomElement fingerElem = childElements(transportElem,"fingerprint",NS_JINGLE_APPS_DTLS).value(0);
		QString hash = fingerElem.attribute("hash");
		QString setup = fingerElem.attribute("setup");
		QString fingerprint = fingerElem.text().trimmed();
		if (!isSdpToken(ufrag) || !isSdpToken(pwd))
		{
			setError(AError,"Jingle transport has no ICE credentials");
			return QString();
		}
		if (!isSdpToken(hash) || !isSdpToken(fingerprint))
		{
			setError(AError,"Jingle transport has no DTLS fingerprint");
			return QString();
		}
		if (setup!="actpass" && setup!="active" && setup!="passive")
		{
			setError(AError,"Jingle transport has no valid DTLS setup role");
			return QString();
		}

		AContentNames.append(name);
		lines.append(QString("m=%1 9 UDP/TLS/RTP/SAVPF %2").arg(media,formats.join(" ")));
		lines.append("c=IN IP4 0.0.0.0");
		lines.append("a=rtcp:9 IN IP4 0.0.0.0");
		lines.append("a=ice-ufrag:"+ufrag);
		lines.append("a=ice-pwd:"+pwd);
		lines.append("a=ice-options:trickle");
		lines.append(QString("a=fingerprint:%1 %2").arg(hash,fingerprint));
		lines.append("a=setup:"+setup);
		lines.append("a=mid:"+name);

		foreach(const QDomElement &extElem, childElements(descElem,"rtp-hdrext",NS_JINGLE_APPS_RTP_HDREXT))
		{
			QString id = extElem.attribute("id");
			QString uri = extElem.attribute("uri");
			if (isNumber(id) && isSdpToken(uri))
				lines.append(QString("a=extmap:%1 %2").arg(id,uri));
		}
		if (!childElements(descElem,"extmap-allow-mixed",NS_JINGLE_APPS_RTP_HDREXT).isEmpty())
			lines.append("a=extmap-allow-mixed");

		lines.append("a="+directionFromSenders(contentElem.attribute("senders"),ARemoteIsInitiator));
		if (!descElem.firstChildElement("rtcp-mux").isNull())
			lines.append("a=rtcp-mux");

		lines += mediaLines;

		foreach(const QDomElement &fbElem, childElements(descElem,"rtcp-fb",NS_JINGLE_APPS_RTP_RTCP_FB))
		{
			QString type = fbElem.attribute("type");
			QString subtype = fbElem.attribute("subtype");
			if (isSdpToken(type) && isSdpSafe(subtype))
				lines.append(QString("a=rtcp-fb:* %1").arg(subtype.isEmpty() ? type : type+" "+subtype));
		}

		foreach(const QDomElement &groupElem, childElements(descElem,"ssrc-group",NS_JINGLE_APPS_RTP_SSMA))
		{
			QStringList ssrcs;
			for (QDomElement sourceElem = groupElem.firstChildElement("source"); !sourceElem.isNull(); sourceElem = sourceElem.nextSiblingElement("source"))
				if (isNumber(sourceElem.attribute("ssrc")))
					ssrcs.append(sourceElem.attribute("ssrc"));
			QString semantics = groupElem.attribute("semantics");
			if (isSdpToken(semantics) && !ssrcs.isEmpty())
				lines.append(QString("a=ssrc-group:%1 %2").arg(semantics,ssrcs.join(" ")));
		}

		foreach(const QDomElement &sourceElem, childElements(descElem,"source",NS_JINGLE_APPS_RTP_SSMA))
		{
			QString ssrc = sourceElem.attribute("ssrc");
			if (!isNumber(ssrc))
				continue;
			for (QDomElement paramElem = sourceElem.firstChildElement("parameter"); !paramElem.isNull(); paramElem = paramElem.nextSiblingElement("parameter"))
			{
				QString paramName = paramElem.attribute("name");
				QString paramValue = paramElem.attribute("value");
				if (isSdpToken(paramName) && !paramValue.isEmpty() && isSdpSafe(paramValue))
					lines.append(QString("a=ssrc:%1 %2:%3").arg(ssrc,paramName,paramValue));
			}
		}

		for (QDomElement candElem = transportElem.firstChildElement("candidate"); !candElem.isNull(); candElem = candElem.nextSiblingElement("candidate"))
		{
			QString candidate = candidateToSdp(candElem);
			if (!candidate.isEmpty())
				lines.append("a="+candidate);
		}
	}

	return lines.join(SDP_EOL) + SDP_EOL;
}

bool JingleSdp::candidateToJingle(const QString &ACandidate, QDomElement &ATransport)
{
	// candidate:<foundation> <component> <protocol> <priority> <ip> <port> typ <type> [raddr <ip>] [rport <port>] [tcptype <type>] [generation <n>]
	QString candidate = ACandidate.trimmed();
	if (candidate.startsWith("a="))
		candidate.remove(0,2);
	if (candidate.startsWith("candidate:"))
		candidate.remove(0,10);

	QStringList parts = candidate.split(' ',Qt::SkipEmptyParts);
	if (parts.count() < 8)
		return false;

	QMap<QString, QString> extra;
	for (int i=6; i+1<parts.count(); i+=2)
		extra.insert(parts.at(i),parts.at(i+1));
	if (!extra.contains("typ"))
		return false;

	QDomElement candElem = appendElement(ATransport,"candidate");
	candElem.setAttribute("component",parts.at(1));
	candElem.setAttribute("foundation",parts.at(0));
	candElem.setAttribute("generation",extra.value("generation","0"));
	candElem.setAttribute("id",QUuid::createUuid().toString(QUuid::WithoutBraces));
	candElem.setAttribute("ip",parts.at(4));
	candElem.setAttribute("network","0");
	candElem.setAttribute("port",parts.at(5));
	candElem.setAttribute("priority",parts.at(3));
	candElem.setAttribute("protocol",parts.at(2).toLower());
	candElem.setAttribute("type",extra.value("typ"));
	if (extra.contains("raddr"))
		candElem.setAttribute("rel-addr",extra.value("raddr"));
	if (extra.contains("rport"))
		candElem.setAttribute("rel-port",extra.value("rport"));
	if (extra.contains("tcptype"))
		candElem.setAttribute("tcptype",extra.value("tcptype"));
	return true;
}

QString JingleSdp::candidateToSdp(const QDomElement &ACandidate)
{
	QString foundation = ACandidate.attribute("foundation");
	QString component = ACandidate.attribute("component");
	QString protocol = ACandidate.attribute("protocol");
	QString priority = ACandidate.attribute("priority");
	QString ip = ACandidate.attribute("ip");
	QString port = ACandidate.attribute("port");
	QString type = ACandidate.attribute("type");
	if (!isSdpToken(foundation) || !isNumber(component) || !isSdpToken(protocol) || !isNumber(priority) || !isSdpToken(ip) || !isNumber(port) || !isSdpToken(type))
		return QString();

	QString candidate = QString("candidate:%1 %2 %3 %4 %5 %6 typ %7").arg(foundation,component,protocol.toUpper(),priority,ip,port,type);

	QString relAddr = ACandidate.attribute("rel-addr");
	QString relPort = ACandidate.attribute("rel-port");
	if (isSdpToken(relAddr) && isNumber(relPort))
		candidate += QString(" raddr %1 rport %2").arg(relAddr,relPort);

	QString tcpType = ACandidate.attribute("tcptype");
	if (isSdpToken(tcpType))
		candidate += " tcptype " + tcpType;

	QString generation = ACandidate.attribute("generation");
	if (isNumber(generation))
		candidate += " generation " + generation;

	return candidate;
}

int JingleSdp::findPayloadType(const QDomElement &AContent, const QString &ACodec, int AClockRate)
{
	QDomElement descElem = AContent.firstChildElement("description");
	for (QDomElement payloadElem = descElem.firstChildElement("payload-type"); !payloadElem.isNull(); payloadElem = payloadElem.nextSiblingElement("payload-type"))
	{
		if (payloadElem.attribute("name").compare(ACodec,Qt::CaseInsensitive)==0 && payloadElem.attribute("clockrate").toInt()==AClockRate)
		{
			bool ok = false;
			int id = payloadElem.attribute("id").toInt(&ok);
			if (ok && id>=0 && id<=127)
				return id;
		}
	}
	return -1;
}

QString JingleSdp::contentMedia(const QDomElement &AContent)
{
	QDomElement descElem = AContent.firstChildElement("description");
	return descElem.namespaceURI()==NS_JINGLE_APPS_RTP ? descElem.attribute("media") : QString();
}
