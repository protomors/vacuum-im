#ifndef JINGLESDP_H
#define JINGLESDP_H

#include <QString>
#include <QStringList>
#include <QDomElement>

// Translation between Jingle RTP sessions (XEP-0166, XEP-0167, XEP-0176, XEP-0320,
// XEP-0293, XEP-0294, XEP-0338, XEP-0339) and the SDP that WebRTC stacks speak.
// Content names map to SDP mids and keep the order of the m-lines.
class JingleSdp
{
public:
	// Writes group and content elements for every active m-line of ASdp into AJingle.
	// ALocalIsInitiator tells which Jingle party produced the SDP.
	static bool sdpToJingle(const QString &ASdp, bool ALocalIsInitiator, QDomElement &AJingle, QStringList &AContentNames, QString *AError = NULL);
	// Builds the SDP described by the contents of AJingle.
	// ARemoteIsInitiator tells which Jingle party produced the element.
	static QString jingleToSdp(const QDomElement &AJingle, bool ARemoteIsInitiator, QStringList &AContentNames, QString *AError = NULL);
	// "candidate:..." attribute value <-> XEP-0176 candidate element
	static bool candidateToJingle(const QString &ACandidate, QDomElement &ATransport);
	static QString candidateToSdp(const QDomElement &ACandidate);
	// Payload type number of the named codec in a content description, -1 if absent
	static int findPayloadType(const QDomElement &AContent, const QString &ACodec, int AClockRate);
	static QString contentMedia(const QDomElement &AContent);
};

#endif // JINGLESDP_H
