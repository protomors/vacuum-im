#ifndef IMESSAGECORRECTIONS_H
#define IMESSAGECORRECTIONS_H

#include <QObject>
#include <interfaces/imessagewidgets.h>

#include <QUuid>
#define MESSAGECORRECTIONS_UUID QUuid::fromString(QLatin1String("{6d0f44a5-36b8-490b-b733-b4fd9ca10119}"))

// XEP-0308 Last Message Correction
class IMessageCorrections
{
public:
	virtual QObject *instance() =0;
	virtual bool isCorrecting(IMessageWindow *AWindow) const =0;
	// Puts the text of the last message sent in the window into its editor
	virtual bool startCorrection(IMessageWindow *AWindow) =0;
	virtual void cancelCorrection(IMessageWindow *AWindow) =0;
protected:
	virtual void correctionStarted(IMessageWindow *AWindow) =0;
	virtual void correctionFinished(IMessageWindow *AWindow) =0;
};

Q_DECLARE_INTERFACE(IMessageCorrections,"Vacuum.Plugin.IMessageCorrections/1.0")

#endif // IMESSAGECORRECTIONS_H
