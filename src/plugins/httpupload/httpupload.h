#ifndef HTTPUPLOAD_H
#define HTTPUPLOAD_H

#include <QMap>
#include <QFile>
#include <QPointer>
#include <QProgressDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ihttpupload.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/imultiuserchat.h>
#include <utils/action.h>

struct UploadService
{
	UploadService() {
		maxFileSize = 0;
	}
	Jid serviceJid;
	qint64 maxFileSize;
};

struct UploadJob
{
	UploadJob() {
		size = 0;
		share = false;
		file = NULL;
		reply = NULL;
		network = NULL;
	}
	QString id;
	Jid streamJid;
	QString fileName;
	QString displayName;
	QString contentType;
	qint64 size;
	QString slotRequestId;
	QUrl getUrl;
	bool share;
	Jid contactJid;
	QFile *file;
	QNetworkReply *reply;
	QNetworkAccessManager *network;
	QPointer<QProgressDialog> progress;
};

class HttpUpload :
	public QObject,
	public IPlugin,
	public IHttpUpload,
	public IStanzaRequestOwner,
	public IMessageViewDropHandler
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IHttpUpload IStanzaRequestOwner IMessageViewDropHandler);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.HttpUpload");
public:
	HttpUpload();
	~HttpUpload();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return HTTPUPLOAD_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings() { return true; }
	virtual bool startPlugin() { return true; }
	//IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
	//IMessageViewDropHandler
	virtual bool messageViewDragEnter(IMessageViewWidget *AWidget, const QDragEnterEvent *AEvent);
	virtual bool messageViewDragMove(IMessageViewWidget *AWidget, const QDragMoveEvent *AEvent);
	virtual void messageViewDragLeave(IMessageViewWidget *AWidget, const QDragLeaveEvent *AEvent);
	virtual bool messageViewDropAction(IMessageViewWidget *AWidget, const QDropEvent *AEvent, Menu *AMenu);
	//IHttpUpload
	virtual bool isSupported(const Jid &AStreamJid) const;
	virtual Jid uploadService(const Jid &AStreamJid) const;
	virtual qint64 maxFileSize(const Jid &AStreamJid) const;
	virtual QString uploadFile(const Jid &AStreamJid, const QString &AFileName);
	virtual void abortUpload(const QString &AUploadId);
	virtual QStringList shareFiles(const Jid &AStreamJid, const Jid &AContactJid, const QStringList &AFileNames);
signals:
	void uploadProgress(const QString &AUploadId, qint64 ASent, qint64 ATotal);
	void uploadFinished(const QString &AUploadId, const QUrl &AGetUrl);
	void uploadFailed(const QString &AUploadId, const XmppError &AError);
protected:
	QString startJob(UploadJob &AJob);
	void startHttpPut(UploadJob &AJob, const QDomElement &ASlotElem);
	void finishJob(const QString &AUploadId, const XmppError &AError);
	bool sendShareMessage(const UploadJob &AJob);
	void checkServiceInfo(const IDiscoInfo &AInfo);
	bool isConference(const Jid &AStreamJid, const Jid &AContactJid) const;
	bool canShareTo(const Jid &AStreamJid, const Jid &AContactJid) const;
	IMessageViewWidget *findViewWidget(const Jid &AStreamJid, const Jid &AContactJid) const;
	void showStatusEvent(const Jid &AStreamJid, const Jid &AContactJid, const QString &AText) const;
	bool windowAddress(IMessageWindow *AWindow, Jid &AStreamJid, Jid &AContactJid) const;
	void updateToolBarAction(IMessageToolBarWidget *AWidget);
	void updateToolBarActions();
protected slots:
	void onXmppStreamClosed(IXmppStream *AXmppStream);
	void onDiscoInfoReceived(const IDiscoInfo &AInfo);
	void onDiscoItemsReceived(const IDiscoItems &AItems);
	void onToolBarWidgetCreated(IMessageToolBarWidget *AWidget);
	void onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore);
	void onToolBarWidgetDestroyed(QObject *AObject);
	void onMultiUserChatStateChanged(int AState);
	void onShareFilesByAction(bool);
	void onReplyUploadProgress(qint64 ASent, qint64 ATotal);
	void onReplyFinished();
	void onProgressCanceled();
private:
	IXmppStreamManager *FXmppStreamManager;
	IStanzaProcessor *FStanzaProcessor;
	IServiceDiscovery *FDiscovery;
	IMessageWidgets *FMessageWidgets;
	IMessageProcessor *FMessageProcessor;
	IMultiUserChatManager *FMultiChatManager;
private:
	QMap<Jid, UploadService> FServices;
	QMap<Jid, QList<Jid> > FServiceCandidates;
	QMap<QString, UploadJob> FJobs;
	QMap<QString, QString> FSlotRequests;
	QMap<IMessageToolBarWidget *, Action *> FToolBarActions;
};

#endif // HTTPUPLOAD_H
