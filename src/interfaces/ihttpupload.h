#ifndef IHTTPUPLOAD_H
#define IHTTPUPLOAD_H
#include <QObject>

#include <QUrl>
#include <QString>
#include <QStringList>
#include <utils/jid.h>
#include <utils/xmpperror.h>

#include <QUuid>
#define HTTPUPLOAD_UUID QUuid::fromString(QLatin1String("{5CCD8A5B-FD73-42F1-846D-0130ACF7C36E}"))

class IHttpUpload
{
public:
	virtual QObject *instance() =0;
	// True when the server of the stream offers an upload service
	virtual bool isSupported(const Jid &AStreamJid) const =0;
	virtual Jid uploadService(const Jid &AStreamJid) const =0;
	// Zero when the service does not announce a limit
	virtual qint64 maxFileSize(const Jid &AStreamJid) const =0;
	// Uploads a file and returns the upload id, the result always comes in uploadFinished or uploadFailed
	virtual QString uploadFile(const Jid &AStreamJid, const QString &AFileName) =0;
	virtual void abortUpload(const QString &AUploadId) =0;
	// Uploads files and sends their links to a contact or an open conference
	virtual QStringList shareFiles(const Jid &AStreamJid, const Jid &AContactJid, const QStringList &AFileNames) =0;
protected:
	virtual void uploadProgress(const QString &AUploadId, qint64 ASent, qint64 ATotal) =0;
	virtual void uploadFinished(const QString &AUploadId, const QUrl &AGetUrl) =0;
	virtual void uploadFailed(const QString &AUploadId, const XmppError &AError) =0;
};

Q_DECLARE_INTERFACE(IHttpUpload,"Vacuum.Plugin.IHttpUpload/1.0")

#endif // IHTTPUPLOAD_H
