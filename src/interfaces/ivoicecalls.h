#ifndef IVOICECALLS_H
#define IVOICECALLS_H
#include <QObject>

#include <QString>
#include <utils/jid.h>

#include <QUuid>
#define VOICECALLS_UUID QUuid::fromString(QLatin1String("{3F1A6C52-8E0B-4D7A-B9C4-61D2E5A0F7B8}"))

class IVoiceCalls
{
public:
	virtual QObject *instance() =0;
	// False when the media components needed for calls are not installed
	virtual bool isAvailable() const =0;
	virtual bool isSupported(const Jid &AStreamJid, const Jid &AContactJid) const =0;
	virtual bool hasCall() const =0;
	virtual bool startCall(const Jid &AStreamJid, const Jid &AContactJid) =0;
protected:
	virtual void callStarted(const Jid &AStreamJid, const Jid &AContactJid, const QString &ACallId) =0;
	virtual void callFinished(const Jid &AStreamJid, const Jid &AContactJid, const QString &ACallId) =0;
};

Q_DECLARE_INTERFACE(IVoiceCalls,"Vacuum.Plugin.IVoiceCalls/1.0")

#endif // IVOICECALLS_H
