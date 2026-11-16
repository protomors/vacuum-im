#ifndef IJINGLE_H
#define IJINGLE_H

#include <QObject>
#include <QString>
#include <QDomElement>
#include <utils/jid.h>
#include <utils/stanza.h>
#include <utils/xmpperror.h>

#include <QUuid>
#define JINGLE_UUID QUuid::fromString(QLatin1String("{290D88C1-237E-4048-9B65-5C0BF022CE35}"))

// An application that runs over Jingle sessions (XEP-0166), such as RTP calls or file transfer
class IJingleHandler
{
public:
	virtual QObject *instance() =0;
	// A session-initiate for a session not yet known, whose first content uses the description
	// namespace this handler was inserted for. Return true after replying to the request,
	// and call IJingle::insertSession() if the session is taken; false makes the core
	// reply with service-unavailable
	virtual bool jingleSessionInitiate(const Jid &AStreamJid, const Stanza &ARequest) =0;
	// Any request for a session that this handler has inserted. Return true after replying
	// to the request; false makes the core reply with unknown-session
	virtual bool jingleSessionRequest(const Jid &AStreamJid, const Stanza &ARequest) =0;
};

class IJingle
{
public:
	virtual QObject *instance() =0;
	// Applications, keyed by the namespace of the <description/> element of their contents
	virtual QList<QString> applications() const =0;
	virtual IJingleHandler *applicationHandler(const QString &ADescriptionNS) const =0;
	virtual bool insertApplication(const QString &ADescriptionNS, IJingleHandler *AHandler) =0;
	virtual void removeApplication(const QString &ADescriptionNS) =0;
	// Sessions route incoming requests with the same sid to their handler
	virtual QString newSessionId() const =0;
	virtual IJingleHandler *sessionHandler(const Jid &AStreamJid, const QString &ASessionId) const =0;
	virtual bool insertSession(const Jid &AStreamJid, const QString &ASessionId, IJingleHandler *AHandler) =0;
	virtual void removeSession(const Jid &AStreamJid, const QString &ASessionId) =0;
	// Stanza helpers
	virtual Stanza makeRequest(const Jid &AContactJid, const QString &ASessionId, const QString &AAction, QDomElement &AJingle) const =0;
	virtual bool sendResult(const Jid &AStreamJid, const Stanza &ARequest) const =0;
	virtual bool sendError(const Jid &AStreamJid, const Stanza &ARequest, XmppStanzaError::ErrorCondition ACondition, const QString &AJingleCondition = QString()) const =0;
	// Sends session-terminate with the given reason, without waiting for the reply
	virtual bool sendTerminate(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, const QString &AReason, const QString &AText = QString()) const =0;
protected:
	virtual void sessionInserted(const Jid &AStreamJid, const QString &ASessionId) =0;
	virtual void sessionRemoved(const Jid &AStreamJid, const QString &ASessionId) =0;
};

Q_DECLARE_INTERFACE(IJingleHandler,"Vacuum.Plugin.IJingleHandler/1.0")
Q_DECLARE_INTERFACE(IJingle,"Vacuum.Plugin.IJingle/1.0")

#endif // IJINGLE_H
