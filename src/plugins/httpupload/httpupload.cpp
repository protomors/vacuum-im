#include "httpupload.h"

#include <QUuid>
#include <QLocale>
#include <QMimeData>
#include <QFileInfo>
#include <QFileDialog>
#include <QMimeDatabase>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QDragEnterEvent>
#include <definitions/namespaces.h>
#include <definitions/actiongroups.h>
#include <definitions/menuicons.h>
#include <definitions/resources.h>
#include <definitions/toolbargroups.h>
#include <definitions/internalerrors.h>
#include <interfaces/idefaultconnection.h>
#include <utils/iconstorage.h>
#include <utils/message.h>
#include <utils/logger.h>

#define SLOT_REQUEST_TIMEOUT    30000

#define ADR_STREAM_JID          Action::DR_StreamJid
#define ADR_CONTACT_JID         Action::DR_Parametr1
#define ADR_FILE_NAMES          Action::DR_Parametr2

#define PROP_UPLOAD_ID          "httpUploadId"

// Headers a client may pass from the slot to the PUT request (XEP-0363)
static const QStringList AllowedPutHeaders = QStringList() << "authorization" << "cookie" << "expires";

HttpUpload::HttpUpload()
{
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FDiscovery = NULL;
	FMessageWidgets = NULL;
	FMessageProcessor = NULL;
	FMultiChatManager = NULL;
}

HttpUpload::~HttpUpload()
{
	foreach(const QString &uploadId, FJobs.keys())
		finishJob(uploadId,XmppError(IERR_HTTPUPLOAD_ABORTED));
}

void HttpUpload::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("HTTP File Upload");
	APluginInfo->description = tr("Allows to share files by uploading them to the server");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Vacuum-IM";
	APluginInfo->homePage = "https://github.com/Vacuum-IM";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
	APluginInfo->dependences.append(SERVICEDISCOVERY_UUID);
}

bool HttpUpload::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);

	IPlugin *plugin = APluginManager->pluginInterface("IXmppStreamManager").value(0,NULL);
	if (plugin)
	{
		FXmppStreamManager = qobject_cast<IXmppStreamManager *>(plugin->instance());
		if (FXmppStreamManager)
			connect(FXmppStreamManager->instance(),SIGNAL(streamClosed(IXmppStream *)),SLOT(onXmppStreamClosed(IXmppStream *)));
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IServiceDiscovery").value(0,NULL);
	if (plugin)
	{
		FDiscovery = qobject_cast<IServiceDiscovery *>(plugin->instance());
		if (FDiscovery)
		{
			connect(FDiscovery->instance(),SIGNAL(discoInfoReceived(const IDiscoInfo &)),SLOT(onDiscoInfoReceived(const IDiscoInfo &)));
			connect(FDiscovery->instance(),SIGNAL(discoItemsReceived(const IDiscoItems &)),SLOT(onDiscoItemsReceived(const IDiscoItems &)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
			connect(FMessageWidgets->instance(),SIGNAL(toolBarWidgetCreated(IMessageToolBarWidget *)),SLOT(onToolBarWidgetCreated(IMessageToolBarWidget *)));
	}

	plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
		FMultiChatManager = qobject_cast<IMultiUserChatManager *>(plugin->instance());

	return FXmppStreamManager!=NULL && FStanzaProcessor!=NULL && FDiscovery!=NULL;
}

bool HttpUpload::initObjects()
{
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_NOT_SUPPORTED,tr("The server does not offer file upload"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_FILE_NOT_READABLE,tr("The file can not be read"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_FILE_TOO_LARGE,tr("The file is larger than the server allows"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_INVALID_SLOT,tr("The server returned an invalid upload slot"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_UPLOAD_FAILED,tr("Failed to upload the file"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_ABORTED,tr("Upload canceled"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HTTPUPLOAD_SEND_FAILED,tr("Failed to send the file link"));

	if (FMessageWidgets)
		FMessageWidgets->insertViewDropHandler(this);
	return true;
}

void HttpUpload::stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza)
{
	QString uploadId = FSlotRequests.take(AStanza.id());
	if (FJobs.contains(uploadId))
	{
		UploadJob &job = FJobs[uploadId];
		job.slotRequestId.clear();
		if (AStanza.isResult())
		{
			QDomElement slotElem = AStanza.firstElement("slot",NS_HTTP_UPLOAD);
			LOG_STRM_INFO(AStreamJid,QString("Upload slot received, id=%1, file=%2").arg(uploadId,job.displayName));
			startHttpPut(job,slotElem);
		}
		else
		{
			XmppStanzaError err(AStanza);
			LOG_STRM_WARNING(AStreamJid,QString("Failed to get upload slot, id=%1, file=%2: %3").arg(uploadId,job.displayName,err.condition()));
			finishJob(uploadId,err);
		}
	}
}

bool HttpUpload::messageViewDragEnter(IMessageViewWidget *AWidget, const QDragEnterEvent *AEvent)
{
	Jid streamJid, contactJid;
	if (AEvent->mimeData()->hasUrls() && windowAddress(AWidget->messageWindow(),streamJid,contactJid) && canShareTo(streamJid,contactJid))
	{
		foreach(const QUrl &url, AEvent->mimeData()->urls())
			if (!QFileInfo(url.toLocalFile()).isFile())
				return false;
		return true;
	}
	return false;
}

bool HttpUpload::messageViewDragMove(IMessageViewWidget *AWidget, const QDragMoveEvent *AEvent)
{
	Q_UNUSED(AWidget); Q_UNUSED(AEvent);
	return true;
}

void HttpUpload::messageViewDragLeave(IMessageViewWidget *AWidget, const QDragLeaveEvent *AEvent)
{
	Q_UNUSED(AWidget); Q_UNUSED(AEvent);
}

bool HttpUpload::messageViewDropAction(IMessageViewWidget *AWidget, const QDropEvent *AEvent, Menu *AMenu)
{
	Jid streamJid, contactJid;
	if (AEvent->dropAction()!=Qt::IgnoreAction && windowAddress(AWidget->messageWindow(),streamJid,contactJid) && canShareTo(streamJid,contactJid))
	{
		QStringList files;
		foreach(const QUrl &url, AEvent->mimeData()->urls())
			files.append(url.toLocalFile());

		Action *action = new Action(AMenu);
		action->setText(files.count()>1 ? tr("Upload and Share %n Files","",files.count()) : tr("Upload and Share File"));
		action->setIcon(RSR_STORAGE_MENUICONS,MNI_FILETRANSFER_SEND);
		action->setData(ADR_STREAM_JID,streamJid.full());
		action->setData(ADR_CONTACT_JID,contactJid.full());
		action->setData(ADR_FILE_NAMES,files);
		connect(action,SIGNAL(triggered(bool)),SLOT(onShareFilesByAction(bool)));
		AMenu->addAction(action,AG_DEFAULT,true);
		if (AMenu->defaultAction() == NULL)
			AMenu->setDefaultAction(action);
		return true;
	}
	return false;
}

bool HttpUpload::isSupported(const Jid &AStreamJid) const
{
	return FServices.contains(AStreamJid);
}

Jid HttpUpload::uploadService(const Jid &AStreamJid) const
{
	return FServices.value(AStreamJid).serviceJid;
}

qint64 HttpUpload::maxFileSize(const Jid &AStreamJid) const
{
	return FServices.value(AStreamJid).maxFileSize;
}

QString HttpUpload::uploadFile(const Jid &AStreamJid, const QString &AFileName)
{
	UploadJob job;
	job.streamJid = AStreamJid;
	job.fileName = AFileName;
	return startJob(job);
}

void HttpUpload::abortUpload(const QString &AUploadId)
{
	if (FJobs.contains(AUploadId))
	{
		LOG_STRM_INFO(FJobs.value(AUploadId).streamJid,QString("Upload aborted, id=%1").arg(AUploadId));
		finishJob(AUploadId,XmppError(IERR_HTTPUPLOAD_ABORTED));
	}
}

QStringList HttpUpload::shareFiles(const Jid &AStreamJid, const Jid &AContactJid, const QStringList &AFileNames)
{
	QStringList uploads;
	foreach(const QString &fileName, AFileNames)
	{
		UploadJob job;
		job.streamJid = AStreamJid;
		job.fileName = fileName;
		job.share = true;
		job.contactJid = AContactJid;
		uploads.append(startJob(job));
	}
	return uploads;
}

QString HttpUpload::startJob(UploadJob &AJob)
{
	QFileInfo info(AJob.fileName);
	AJob.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	AJob.displayName = info.fileName();
	AJob.size = info.size();

	XmppError err;
	if (!isSupported(AJob.streamJid))
		err = XmppError(IERR_HTTPUPLOAD_NOT_SUPPORTED);
	else if (!info.isFile() || !info.isReadable())
		err = XmppError(IERR_HTTPUPLOAD_FILE_NOT_READABLE);
	else if (maxFileSize(AJob.streamJid)>0 && AJob.size>maxFileSize(AJob.streamJid))
		err = XmppError(IERR_HTTPUPLOAD_FILE_TOO_LARGE,tr("Maximum size is %1").arg(QLocale().formattedDataSize(maxFileSize(AJob.streamJid))));

	if (err.isNull())
	{
		AJob.contentType = QMimeDatabase().mimeTypeForFile(info).name();

		Stanza request(STANZA_KIND_IQ);
		request.setType(STANZA_TYPE_GET).setTo(uploadService(AJob.streamJid).full()).setUniqueId();
		QDomElement requestElem = request.addElement("request",NS_HTTP_UPLOAD);
		requestElem.setAttribute("filename",AJob.displayName);
		requestElem.setAttribute("size",QString::number(AJob.size));
		requestElem.setAttribute("content-type",AJob.contentType);
		if (FStanzaProcessor->sendStanzaRequest(this,AJob.streamJid,request,SLOT_REQUEST_TIMEOUT))
		{
			LOG_STRM_INFO(AJob.streamJid,QString("Upload slot requested, id=%1, file=%2, size=%3").arg(AJob.id,AJob.displayName).arg(AJob.size));
			AJob.slotRequestId = request.id();
			FSlotRequests.insert(request.id(),AJob.id);

			if (AJob.share)
			{
				IMessageViewWidget *view = findViewWidget(AJob.streamJid,AJob.contactJid);
				QProgressDialog *progress = new QProgressDialog(view!=NULL ? view->messageWindow()->instance()->window() : NULL);
				progress->setAttribute(Qt::WA_DeleteOnClose,true);
				progress->setWindowModality(Qt::NonModal);
				progress->setWindowTitle(tr("Uploading File"));
				progress->setLabelText(tr("Uploading '%1' (%2)...").arg(AJob.displayName,QLocale().formattedDataSize(AJob.size)));
				progress->setRange(0,100);
				progress->setMinimumDuration(1000);
				progress->setAutoClose(false);
				progress->setAutoReset(false);
				progress->setValue(0);
				progress->setProperty(PROP_UPLOAD_ID,AJob.id);
				connect(progress,SIGNAL(canceled()),SLOT(onProgressCanceled()));
				AJob.progress = progress;
			}

			FJobs.insert(AJob.id,AJob);
			return AJob.id;
		}
		err = XmppError(IERR_HTTPUPLOAD_UPLOAD_FAILED);
	}

	LOG_STRM_WARNING(AJob.streamJid,QString("Failed to start upload, file=%1: %2").arg(AJob.fileName,err.condition()));
	if (AJob.share)
		showStatusEvent(AJob.streamJid,AJob.contactJid,tr("Failed to share file '%1': %2").arg(AJob.displayName,err.errorMessage()));

	// Report the failure after the caller got the upload id
	QString uploadId = AJob.id;
	QMetaObject::invokeMethod(this,[this,uploadId,err]() { emit uploadFailed(uploadId,err); },Qt::QueuedConnection);
	return uploadId;
}

void HttpUpload::startHttpPut(UploadJob &AJob, const QDomElement &ASlotElem)
{
	QDomElement putElem = ASlotElem.firstChildElement("put");
	QUrl putUrl(putElem.attribute("url"));
	QUrl getUrl(ASlotElem.firstChildElement("get").attribute("url"));
	static const QStringList schemes = QStringList() << "https" << "http";
	if (!putUrl.isValid() || !getUrl.isValid() || !schemes.contains(putUrl.scheme()) || !schemes.contains(getUrl.scheme()))
	{
		LOG_STRM_WARNING(AJob.streamJid,QString("Invalid upload slot, id=%1, put=%2, get=%3").arg(AJob.id,putUrl.toString(),getUrl.toString()));
		finishJob(AJob.id,XmppError(IERR_HTTPUPLOAD_INVALID_SLOT));
		return;
	}

	AJob.file = new QFile(AJob.fileName,this);
	if (!AJob.file->open(QFile::ReadOnly))
	{
		finishJob(AJob.id,XmppError(IERR_HTTPUPLOAD_FILE_NOT_READABLE,AJob.file->errorString()));
		return;
	}

	QNetworkRequest request(putUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader,AJob.contentType);
	request.setHeader(QNetworkRequest::ContentLengthHeader,AJob.size);
	for (QDomElement headerElem=putElem.firstChildElement("header"); !headerElem.isNull(); headerElem=headerElem.nextSiblingElement("header"))
	{
		QString name = headerElem.attribute("name");
		if (AllowedPutHeaders.contains(name.toLower()))
		{
			QString value = headerElem.text();
			value.remove('\r').remove('\n');
			request.setRawHeader(name.toLatin1(),value.toUtf8());
		}
	}

	// Upload through the same proxy as the XMPP connection
	AJob.network = new QNetworkAccessManager(this);
	IXmppStream *stream = FXmppStreamManager->findXmppStream(AJob.streamJid);
	IDefaultConnection *connection = stream!=NULL && stream->connection()!=NULL ? qobject_cast<IDefaultConnection *>(stream->connection()->instance()) : NULL;
	if (connection != NULL)
		AJob.network->setProxy(connection->proxy());

	AJob.getUrl = getUrl;
	AJob.reply = AJob.network->put(request,AJob.file);
	AJob.reply->setProperty(PROP_UPLOAD_ID,AJob.id);
	connect(AJob.reply,SIGNAL(uploadProgress(qint64,qint64)),SLOT(onReplyUploadProgress(qint64,qint64)));
	connect(AJob.reply,SIGNAL(finished()),SLOT(onReplyFinished()));
	LOG_STRM_INFO(AJob.streamJid,QString("Uploading file, id=%1, url=%2").arg(AJob.id,putUrl.toString()));
}

void HttpUpload::finishJob(const QString &AUploadId, const XmppError &AError)
{
	if (FJobs.contains(AUploadId))
	{
		UploadJob job = FJobs.take(AUploadId);
		FSlotRequests.remove(job.slotRequestId);

		if (job.reply != NULL)
		{
			job.reply->disconnect(this);
			if (job.reply->isRunning())
				job.reply->abort();
			job.reply->deleteLater();
		}
		if (job.network != NULL)
			job.network->deleteLater();
		if (job.file != NULL)
			job.file->deleteLater();
		if (!job.progress.isNull())
		{
			job.progress->disconnect(this);
			job.progress->close();
		}

		if (AError.isNull())
		{
			LOG_STRM_INFO(job.streamJid,QString("File uploaded, id=%1, url=%2").arg(AUploadId,job.getUrl.toString()));
			emit uploadFinished(AUploadId,job.getUrl);
			if (job.share && !sendShareMessage(job))
				showStatusEvent(job.streamJid,job.contactJid,tr("File '%1' was uploaded, but its link could not be sent").arg(job.displayName));
		}
		else
		{
			emit uploadFailed(AUploadId,AError);
			if (job.share && AError.condition()!=IERR_HTTPUPLOAD_ABORTED)
				showStatusEvent(job.streamJid,job.contactJid,tr("Failed to share file '%1': %2").arg(job.displayName,AError.errorMessage()));
		}
	}
}

bool HttpUpload::sendShareMessage(const UploadJob &AJob)
{
	Message message;
	message.setBody(AJob.getUrl.toString(QUrl::FullyEncoded));
	QDomElement oobElem = message.stanza().addElement("x",NS_JABBER_X_OOB);
	oobElem.appendChild(message.stanza().createElement("url")).appendChild(message.stanza().createTextNode(AJob.getUrl.toString(QUrl::FullyEncoded)));

	if (isConference(AJob.streamJid,AJob.contactJid))
	{
		IMultiUserChat *multiChat = FMultiChatManager->findMultiUserChat(AJob.streamJid,AJob.contactJid);
		if (multiChat!=NULL && multiChat->sendMessage(message))
			return true;
	}
	else if (FMessageProcessor)
	{
		message.setType(Message::Chat).setTo(AJob.contactJid.full());
		if (FMessageProcessor->sendMessage(AJob.streamJid,message,IMessageProcessor::DirectionOut))
			return true;
	}
	LOG_STRM_WARNING(AJob.streamJid,QString("Failed to send uploaded file link to=%1").arg(AJob.contactJid.full()));
	return false;
}

void HttpUpload::checkServiceInfo(const IDiscoInfo &AInfo)
{
	if (AInfo.node.isEmpty() && AInfo.error.isNull() && AInfo.features.contains(NS_HTTP_UPLOAD) && !FServices.contains(AInfo.streamJid))
	{
		UploadService service;
		service.serviceJid = AInfo.contactJid;
		foreach(const IDataForm &form, AInfo.extensions)
		{
			bool uploadForm = false;
			QString maxSize;
			foreach(const IDataField &field, form.fields)
			{
				if (field.var=="FORM_TYPE" && field.value.toString()==NS_HTTP_UPLOAD)
					uploadForm = true;
				else if (field.var == "max-file-size")
					maxSize = field.value.toString();
			}
			if (uploadForm && !maxSize.isEmpty())
				service.maxFileSize = maxSize.toLongLong();
		}

		LOG_STRM_INFO(AInfo.streamJid,QString("Upload service found, jid=%1, max-file-size=%2").arg(service.serviceJid.full()).arg(service.maxFileSize));
		FServices.insert(AInfo.streamJid,service);
		updateToolBarActions();
	}
}

bool HttpUpload::isConference(const Jid &AStreamJid, const Jid &AContactJid) const
{
	return !AContactJid.hasResource() && FMultiChatManager!=NULL && FMultiChatManager->findMultiUserChat(AStreamJid,AContactJid)!=NULL;
}

bool HttpUpload::canShareTo(const Jid &AStreamJid, const Jid &AContactJid) const
{
	if (!isSupported(AStreamJid) || !AContactJid.isValid())
		return false;
	if (isConference(AStreamJid,AContactJid))
		return FMultiChatManager->findMultiUserChat(AStreamJid,AContactJid)->isOpen();
	return true;
}

IMessageViewWidget *HttpUpload::findViewWidget(const Jid &AStreamJid, const Jid &AContactJid) const
{
	if (isConference(AStreamJid,AContactJid))
	{
		IMultiUserChatWindow *window = FMultiChatManager->findMultiChatWindow(AStreamJid,AContactJid);
		return window!=NULL ? window->viewWidget() : NULL;
	}
	IMessageChatWindow *window = FMessageWidgets!=NULL ? FMessageWidgets->findChatWindow(AStreamJid,AContactJid) : NULL;
	return window!=NULL ? window->viewWidget() : NULL;
}

void HttpUpload::showStatusEvent(const Jid &AStreamJid, const Jid &AContactJid, const QString &AText) const
{
	IMessageViewWidget *view = findViewWidget(AStreamJid,AContactJid);
	if (view != NULL)
	{
		IMessageStyleContentOptions options;
		options.kind = IMessageStyleContentOptions::KindStatus;
		options.type |= IMessageStyleContentOptions::TypeEvent;
		options.direction = IMessageStyleContentOptions::DirectionIn;
		options.time = QDateTime::currentDateTime();
		view->appendText(AText,options);
	}
}

bool HttpUpload::windowAddress(IMessageWindow *AWindow, Jid &AStreamJid, Jid &AContactJid) const
{
	if (AWindow != NULL)
	{
		IMultiUserChatWindow *mucWindow = qobject_cast<IMultiUserChatWindow *>(AWindow->instance());
		if (mucWindow != NULL)
		{
			AStreamJid = mucWindow->streamJid();
			AContactJid = mucWindow->multiUserChat()->roomJid();
			return true;
		}
		if (qobject_cast<IMessageChatWindow *>(AWindow->instance()) != NULL)
		{
			AStreamJid = AWindow->streamJid();
			AContactJid = AWindow->contactJid();
			return true;
		}
	}
	return false;
}

void HttpUpload::updateToolBarAction(IMessageToolBarWidget *AWidget)
{
	Jid streamJid, contactJid;
	if (windowAddress(AWidget->messageWindow(),streamJid,contactJid))
	{
		Action *action = FToolBarActions.value(AWidget);
		if (action == NULL)
		{
			action = new Action(AWidget->toolBarChanger()->toolBar());
			action->setText(tr("Upload and Share File"));
			action->setIcon(RSR_STORAGE_MENUICONS,MNI_FILETRANSFER_SEND);
			connect(action,SIGNAL(triggered(bool)),SLOT(onShareFilesByAction(bool)));
			AWidget->toolBarChanger()->insertAction(action,TBG_MWTBW_HTTPUPLOAD);
			FToolBarActions.insert(AWidget,action);
		}
		action->setData(ADR_STREAM_JID,streamJid.full());
		action->setData(ADR_CONTACT_JID,contactJid.full());
		action->setVisible(isSupported(streamJid));
		action->setEnabled(canShareTo(streamJid,contactJid));
	}
}

void HttpUpload::updateToolBarActions()
{
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		updateToolBarAction(widget);
}

void HttpUpload::onXmppStreamClosed(IXmppStream *AXmppStream)
{
	FServices.remove(AXmppStream->streamJid());
	FServiceCandidates.remove(AXmppStream->streamJid());
	foreach(const UploadJob &job, FJobs.values())
		if (job.streamJid==AXmppStream->streamJid() && job.reply==NULL)
			finishJob(job.id,XmppError(IERR_HTTPUPLOAD_UPLOAD_FAILED,tr("Connection closed")));
	updateToolBarActions();
}

void HttpUpload::onDiscoInfoReceived(const IDiscoInfo &AInfo)
{
	if (AInfo.contactJid==AInfo.streamJid.domain() || FServiceCandidates.value(AInfo.streamJid).contains(AInfo.contactJid))
		checkServiceInfo(AInfo);
}

void HttpUpload::onDiscoItemsReceived(const IDiscoItems &AItems)
{
	if (AItems.node.isEmpty() && AItems.contactJid==AItems.streamJid.domain())
	{
		QList<Jid> &candidates = FServiceCandidates[AItems.streamJid];
		foreach(const IDiscoItem &item, AItems.items)
		{
			if (item.node.isEmpty() && !candidates.contains(item.itemJid))
			{
				candidates.append(item.itemJid);
				if (FDiscovery->hasDiscoInfo(AItems.streamJid,item.itemJid))
					checkServiceInfo(FDiscovery->discoInfo(AItems.streamJid,item.itemJid));
				else
					FDiscovery->requestDiscoInfo(AItems.streamJid,item.itemJid);
			}
		}
	}
}

void HttpUpload::onToolBarWidgetCreated(IMessageToolBarWidget *AWidget)
{
	IMultiUserChatWindow *mucWindow = qobject_cast<IMultiUserChatWindow *>(AWidget->messageWindow()->instance());
	if (mucWindow != NULL)
		connect(mucWindow->multiUserChat()->instance(),SIGNAL(stateChanged(int)),SLOT(onMultiUserChatStateChanged(int)));
	else if (qobject_cast<IMessageChatWindow *>(AWidget->messageWindow()->instance()) != NULL)
		connect(AWidget->messageWindow()->address()->instance(),SIGNAL(addressChanged(const Jid &,const Jid &)),SLOT(onToolBarWidgetAddressChanged(const Jid &,const Jid &)));
	else
		return;

	connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onToolBarWidgetDestroyed(QObject *)));
	updateToolBarAction(AWidget);
}

void HttpUpload::onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore)
{
	Q_UNUSED(AStreamBefore); Q_UNUSED(AContactBefore);
	IMessageAddress *address = qobject_cast<IMessageAddress *>(sender());
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		if (widget->messageWindow()->address() == address)
			updateToolBarAction(widget);
}

void HttpUpload::onToolBarWidgetDestroyed(QObject *AObject)
{
	foreach(IMessageToolBarWidget *widget, FToolBarActions.keys())
		if (qobject_cast<QObject *>(widget->instance()) == AObject)
			FToolBarActions.remove(widget);
}

void HttpUpload::onMultiUserChatStateChanged(int AState)
{
	Q_UNUSED(AState);
	updateToolBarActions();
}

void HttpUpload::onShareFilesByAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		Jid streamJid = action->data(ADR_STREAM_JID).toString();
		Jid contactJid = action->data(ADR_CONTACT_JID).toString();
		QStringList files = action->data(ADR_FILE_NAMES).toStringList();
		if (files.isEmpty())
		{
			IMessageViewWidget *view = findViewWidget(streamJid,contactJid);
			QWidget *parent = view!=NULL ? view->messageWindow()->instance()->window() : NULL;
			files = QFileDialog::getOpenFileNames(parent,tr("Select Files to Share"));
		}
		if (!files.isEmpty())
			shareFiles(streamJid,contactJid,files);
	}
}

void HttpUpload::onReplyUploadProgress(qint64 ASent, qint64 ATotal)
{
	QString uploadId = sender()->property(PROP_UPLOAD_ID).toString();
	if (FJobs.contains(uploadId))
	{
		const UploadJob &job = FJobs[uploadId];
		if (!job.progress.isNull() && ATotal>0)
			job.progress->setValue(int(ASent*100/ATotal));
		emit uploadProgress(uploadId,ASent,ATotal);
	}
}

void HttpUpload::onReplyFinished()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
	QString uploadId = reply!=NULL ? reply->property(PROP_UPLOAD_ID).toString() : QString();
	if (FJobs.contains(uploadId))
	{
		int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error()==QNetworkReply::NoError && (status==200 || status==201))
		{
			finishJob(uploadId,XmppError::null);
		}
		else
		{
			QString text = status>0 ? QString("HTTP %1 %2").arg(status).arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()) : reply->errorString();
			LOG_STRM_WARNING(FJobs.value(uploadId).streamJid,QString("Failed to upload file, id=%1: %2").arg(uploadId,text));
			finishJob(uploadId,XmppError(IERR_HTTPUPLOAD_UPLOAD_FAILED,text));
		}
	}
}

void HttpUpload::onProgressCanceled()
{
	abortUpload(sender()->property(PROP_UPLOAD_ID).toString());
}
