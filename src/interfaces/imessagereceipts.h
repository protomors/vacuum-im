#ifndef IMESSAGERECEIPTS_H
#define IMESSAGERECEIPTS_H

#include <QObject>
#include <utils/jid.h>

#include <QUuid>
#define MESSAGERECEIPTS_UUID QUuid::fromString(QLatin1String("{bfc468bf-392a-45ce-a3f7-7e089e32c428}"))

// XEP-0184 Message Delivery Receipts and XEP-0333 Chat Markers
class IMessageReceipts
{
public:
	enum MessageState {
		StateUnknown,
		StateDelivered,
		StateDisplayed
	};
public:
	virtual QObject *instance() =0;
	// State of an outgoing message, AMessageId is the id of the displayed message,
	// which for corrected messages is the id of the original message
	virtual int messageState(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessageId) const =0;
protected:
	virtual void messageStateChanged(const Jid &AStreamJid, const Jid &AContactJid, const QString &AMessageId, int AState) =0;
};

Q_DECLARE_INTERFACE(IMessageReceipts,"Vacuum.Plugin.IMessageReceipts/1.0")

#endif // IMESSAGERECEIPTS_H
