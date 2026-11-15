#ifndef IFILETRANSFER_H
#define IFILETRANSFER_H
#include <QObject>

#include <QString>
#include <interfaces/ifilestreamsmanager.h>
#include <utils/xmpperror.h>
#include <utils/stanza.h>
#include <utils/jid.h>

#include <QUuid>
#define FILETRANSFER_UUID QUuid::fromString(QLatin1String("{6e1cc70e-5604-4857-b742-ba185323bb4b}"))

class IFileTransfer
{
public:
	virtual QObject *instance() =0;
	// Send Files
	virtual bool isSupported(const Jid &AStreamJid, const Jid &AContactJid) const =0;
	virtual IFileStream *sendFile(const Jid &AStreamJid, const Jid &AContactJid, const QString &AFileName=QString(), const QString &AFileDesc=QString()) =0;
};

Q_DECLARE_INTERFACE(IFileTransfer,"Vacuum.Plugin.IFileTransfer/1.2")

#endif // IFILETRANSFER_H
