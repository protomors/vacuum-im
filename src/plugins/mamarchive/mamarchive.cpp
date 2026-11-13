#include "mamarchive.h"

#include <algorithm>

#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <definitions/menuicons.h>
#include <definitions/resources.h>
#include <definitions/namespaces.h>
#include <definitions/optionnodes.h>
#include <definitions/optionvalues.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/stanzahandlerorders.h>
#include <utils/datetime.h>
#include <utils/iconstorage.h>
#include <utils/logger.h>
#include <utils/options.h>
#include <utils/stanza.h>
#include "mamoptionswidget.h"

#define MAM_PAGE_SIZE            50
#define MAM_REQUEST_TIMEOUT      60000
#define MAM_SESSION_GAP          (60 * 60)

#define SHC_MAM_RESULT           "/message/result[@xmlns='" NS_MAM "']"

MamArchive::MamArchive()
{
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FDiscovery = NULL;
	FArchiver = NULL;
	FFileArchive = NULL;
	FBookmarks = NULL;
	FMultiChat = NULL;
	FOptionsManager = NULL;
	FAccountManager = NULL;
}

MamArchive::~MamArchive()
{

}

void MamArchive::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Message Archive Management");
	APluginInfo->description = tr("Downloads server message history into the local archive");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://vacuum-im.org";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
	APluginInfo->dependences.append(SERVICEDISCOVERY_UUID);
	APluginInfo->dependences.append(MESSAGEARCHIVER_UUID);
	APluginInfo->dependences.append(FILEMESSAGEARCHIVE_UUID);
}

bool MamArchive::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);

	IPlugin *plugin = APluginManager->pluginInterface("IXmppStreamManager").value(0,NULL);
	if (plugin)
	{
		FXmppStreamManager = qobject_cast<IXmppStreamManager *>(plugin->instance());
		if (FXmppStreamManager)
		{
			connect(FXmppStreamManager->instance(),SIGNAL(streamOpened(IXmppStream *)),SLOT(onXmppStreamOpened(IXmppStream *)));
			connect(FXmppStreamManager->instance(),SIGNAL(streamClosed(IXmppStream *)),SLOT(onXmppStreamClosed(IXmppStream *)));
		}
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
			connect(FDiscovery->instance(),SIGNAL(discoOpened(const Jid &)),SLOT(onDiscoOpened(const Jid &)));
			connect(FDiscovery->instance(),SIGNAL(discoInfoReceived(const IDiscoInfo &)),SLOT(onDiscoInfoReceived(const IDiscoInfo &)));
		}
	}

	plugin = APluginManager->pluginInterface("IMessageArchiver").value(0,NULL);
	if (plugin)
		FArchiver = qobject_cast<IMessageArchiver *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IFileMessageArchive").value(0,NULL);
	if (plugin)
	{
		FFileArchive = qobject_cast<IFileMessageArchive *>(plugin->instance());
		if (FFileArchive)
			connect(FFileArchive->instance(),SIGNAL(databaseOpened(const Jid &)),SLOT(onDatabaseOpened(const Jid &)));
	}

	plugin = APluginManager->pluginInterface("IBookmarks").value(0,NULL);
	if (plugin)
	{
		FBookmarks = qobject_cast<IBookmarks *>(plugin->instance());
		if (FBookmarks)
		{
			connect(FBookmarks->instance(),SIGNAL(bookmarksOpened(const Jid &)),SLOT(onBookmarksOpened(const Jid &)));
			connect(FBookmarks->instance(),SIGNAL(bookmarksChanged(const Jid &)),SLOT(onBookmarksChanged(const Jid &)));
		}
	}

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
	{
		FMultiChat = qobject_cast<IMultiUserChatManager *>(plugin->instance());
		if (FMultiChat)
			connect(FMultiChat->instance(),SIGNAL(multiUserChatCreated(IMultiUserChat *)),SLOT(onMultiUserChatCreated(IMultiUserChat *)));
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());

	plugin = APluginManager->pluginInterface("IAccountManager").value(0,NULL);
	if (plugin)
		FAccountManager = qobject_cast<IAccountManager *>(plugin->instance());

	return FXmppStreamManager!=NULL && FStanzaProcessor!=NULL && FDiscovery!=NULL && FArchiver!=NULL && FFileArchive!=NULL;
}

bool MamArchive::initObjects()
{
	if (FOptionsManager)
		FOptionsManager->insertOptionsDialogHolder(this);
	if (FDiscovery)
	{
		IDiscoFeature dfeature;
		dfeature.active = false;
		dfeature.icon = IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->getIcon(MNI_HISTORY);
		dfeature.var = NS_MAM;
		dfeature.name = tr("Message Archive Management");
		dfeature.description = tr("Supports loading message history from the server");
		FDiscovery->insertDiscoFeature(dfeature);
	}
	return true;
}

bool MamArchive::initSettings()
{
	Options::setDefaultValue(OPV_ACCOUNT_MAM_DOWNLOAD,true);
	Options::setDefaultValue(OPV_ACCOUNT_MAM_CONFERENCES,true);
	Options::setDefaultValue(OPV_ACCOUNT_MAM_START,QString());
	return true;
}

QMultiMap<int, IOptionsDialogWidget *> MamArchive::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *> widgets;
	QStringList nodeTree = ANodeId.split(".",Qt::SkipEmptyParts);
	if (FOptionsManager && FAccountManager && nodeTree.count()==3 && nodeTree.at(0)==OPN_ACCOUNTS && nodeTree.at(2)=="History")
	{
		IAccount *account = FAccountManager->findAccountById(QUuid::fromString(nodeTree.at(1)));
		if (account)
		{
			widgets.insert(OHO_ACCOUNTS_HISTORY_MAM,FOptionsManager->newOptionsDialogHeader(tr("Server history"),AParent));
			widgets.insert(OWO_ACCOUNTS_HISTORY_MAM,new MamOptionsWidget(this,account,AParent));
		}
	}
	return widgets;
}

QString MamArchive::statusText(const QUuid &AAccountId) const
{
	MamRuntime state = FState.value(AAccountId);
	switch (state.phase)
	{
	case MamRuntime::Checking:
		return tr("Checking whether the server stores history…");
	case MamRuntime::Unsupported:
		return tr("This server does not support message archive management.");
	case MamRuntime::Disabled:
		return tr("Server history download is turned off.");
	case MamRuntime::Syncing:
		if (state.written > 0)
			return tr("Downloading server history… %1 messages saved.").arg(state.written);
		return tr("Downloading server history…");
	case MamRuntime::Current:
		return tr("Server history is up to date (%1).").arg(QLocale().toString(state.finished,QLocale::ShortFormat));
	case MamRuntime::Failed:
		return tr("Server history download failed: %1").arg(state.error);
	case MamRuntime::Offline:
	default:
		break;
	}
	if (state.finished.isValid())
		return tr("Not connected. Last sync finished at %1.").arg(QLocale().toString(state.finished,QLocale::ShortFormat));
	return tr("Not connected. These settings apply when the account connects.");
}

void MamArchive::applyAccountOptions(IAccount *AAccount, bool ABoundsChanged)
{
	if (!AAccount)
		return;

	bool download = AAccount->optionsNode().value("mam-download").toBool();
	bool online = AAccount->xmppStream() && AAccount->xmppStream()->isOpen();
	Jid streamJid = AAccount->streamJid();

	if (!download)
	{
		if (online)
			stopWork(streamJid);
		setPhase(AAccount->accountId(),MamRuntime::Disabled);
		return;
	}

	if (ABoundsChanged)
	{
		clearCursors(streamJid);
		FState[AAccount->accountId()].finished = QDateTime();
	}

	if (!online)
	{
		setPhase(AAccount->accountId(),MamRuntime::Offline);
		return;
	}

	MamRuntime::Phase phase = FState.value(AAccount->accountId()).phase;
	if (ABoundsChanged || phase==MamRuntime::Disabled || phase==MamRuntime::Failed || phase==MamRuntime::Current)
	{
		stopWork(streamJid);
		FDone.remove(streamJid);
		FFailed.remove(streamJid);
		FWatchedRooms.remove(streamJid);
		FContacts.remove(streamJid);
		if (FServerMam.contains(streamJid) && FServerMam.value(streamJid))
			startJobs(AAccount,true);
		else if (FServerMam.contains(streamJid))
			setPhase(AAccount->accountId(),MamRuntime::Unsupported);
		else
			setPhase(AAccount->accountId(),MamRuntime::Checking);
	}
}

bool MamArchive::stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	Q_UNUSED(AHandleId);
	if (!FActive.contains(AStreamJid))
	{
		AAccept = true;
		return true;
	}

	QDomElement resultElem = AStanza.firstElement("result",NS_MAM);
	QString queryId = resultElem.attribute("queryid");
	MamQuery &query = FActive[AStreamJid];
	if (!queryId.isEmpty() && queryId!=query.queryId)
	{
		AAccept = true;
		return true;
	}

	Message message;
	QString resultId = resultElem.attribute("id");
	if (!resultId.isEmpty() && !query.resultIds.contains(resultId) && readArchivedMessage(resultElem,message))
	{
		query.resultIds.insert(resultId);
		query.messages.append(message);
	}

	AAccept = true;
	return true;
}

void MamArchive::stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza)
{
	if (FPrefsRequests.contains(AStanza.id()))
	{
		handlePrefsResult(AStanza);
		return;
	}

	if (!FActive.contains(AStreamJid) || FActive.value(AStreamJid).queryId!=AStanza.id())
		return;

	MamQuery &query = FActive[AStreamJid];
	if (AStanza.isResult())
	{
		QDomElement finElem = AStanza.firstElement("fin",NS_MAM);
		if (finElem.isNull())
		{
			LOG_STRM_WARNING(AStreamJid,QString("MAM query has no fin element, id=%1").arg(AStanza.id()));
			FFailed[AStreamJid].insert(archiveKey(query.userArchive,query.archiveJid));
			FActive.remove(AStreamJid);
			IAccount *account = accountByStream(AStreamJid);
			if (account)
				setPhase(account->accountId(),MamRuntime::Failed,tr("The server returned an incomplete archive response"));
			QTimer::singleShot(0,this,SLOT(onContinueSync()));
			return;
		}

		query.finReceived = true;
		query.complete = finElem.attribute("complete")=="true";
		QDomElement setElem = finElem.firstChildElement("set");
		while (!setElem.isNull() && setElem.namespaceURI()!=NS_RESULTSET)
			setElem = setElem.nextSiblingElement("set");
		if (!setElem.isNull())
			query.lastId = setElem.firstChildElement("last").text();

		if (!FFinishPending.contains(AStreamJid))
		{
			FFinishPending.insert(AStreamJid);
			QTimer::singleShot(0,this,SLOT(onFinishQuery()));
		}
	}
	else
	{
		XmppStanzaError err(AStanza);
		LOG_STRM_WARNING(AStreamJid,QString("MAM query failed, archive=%1, id=%2: %3").arg(query.userArchive ? AStreamJid.pBare() : query.archiveJid.pBare(),AStanza.id(),err.condition()));
		FFailed[AStreamJid].insert(archiveKey(query.userArchive,query.archiveJid));
		FActive.remove(AStreamJid);
		IAccount *account = accountByStream(AStreamJid);
		if (account)
			setPhase(account->accountId(),MamRuntime::Failed,err.errorMessage());
		QTimer::singleShot(0,this,SLOT(onContinueSync()));
	}
}

QString MamArchive::archiveKey(bool AUserArchive, const Jid &AArchiveJid) const
{
	return AUserArchive ? QString("account") : Jid::encode(AArchiveJid.pBare());
}

QString MamArchive::cursorFilePath(const Jid &AStreamJid) const
{
	QString dir = FArchiver!=NULL ? FArchiver->archiveDirPath(AStreamJid) : QString();
	return dir.isEmpty() ? QString() : dir + "/mam-sync.ini";
}

QString MamArchive::readCursor(const Jid &AStreamJid, const MamJob &AJob) const
{
	QString path = cursorFilePath(AStreamJid);
	if (path.isEmpty())
		return QString();

	QSettings settings(path,QSettings::IniFormat);
	settings.beginGroup("cursors");
	return settings.value(archiveKey(AJob.userArchive,AJob.archiveJid)).toString();
}

void MamArchive::writeCursor(const Jid &AStreamJid, const MamJob &AJob, const QString &ACursor)
{
	QString path = cursorFilePath(AStreamJid);
	if (path.isEmpty() || ACursor.isEmpty())
		return;

	QSettings settings(path,QSettings::IniFormat);
	settings.beginGroup("cursors");
	settings.setValue(archiveKey(AJob.userArchive,AJob.archiveJid),ACursor);
	settings.sync();
}

bool MamArchive::isKnownRoom(const Jid &AStreamJid, const Jid &ARoomJid) const
{
	return FWatchedRooms.value(AStreamJid).contains(ARoomJid.pBare());
}

void MamArchive::watchRoom(const Jid &AStreamJid, const Jid &ARoomJid)
{
	if (!FDiscovery || !ARoomJid.isValid() || !downloadEnabled(AStreamJid) || !conferencesEnabled(AStreamJid))
		return;

	QString bare = ARoomJid.pBare();
	if (FWatchedRooms[AStreamJid].contains(bare))
		return;
	FWatchedRooms[AStreamJid].insert(bare);

	Jid roomJid = ARoomJid.bare();
	if (FDiscovery->hasDiscoInfo(AStreamJid,roomJid))
	{
		IDiscoInfo info = FDiscovery->discoInfo(AStreamJid,roomJid);
		if (info.node.isEmpty() && info.error.isNull() && info.features.contains(NS_MAM))
		{
			MamJob job;
			job.streamJid = AStreamJid;
			job.userArchive = false;
			job.archiveJid = roomJid;
			enqueue(job);
		}
	}
	else
	{
		FDiscovery->requestDiscoInfo(AStreamJid,roomJid);
	}
}

void MamArchive::enqueue(const MamJob &AJob)
{
	if (!downloadEnabled(AJob.streamJid))
		return;
	if (!AJob.userArchive && !conferencesEnabled(AJob.streamJid))
		return;

	QString key = archiveKey(AJob.userArchive,AJob.archiveJid);
	if (FDone.value(AJob.streamJid).contains(key) || FFailed.value(AJob.streamJid).contains(key))
		return;

	if (FActive.contains(AJob.streamJid))
	{
		const MamQuery &active = FActive.value(AJob.streamJid);
		if (archiveKey(active.userArchive,active.archiveJid) == key)
			return;
	}

	QList<MamJob> &queue = FQueues[AJob.streamJid];
	for (int i=0; i<queue.count(); ++i)
	{
		if (archiveKey(queue.at(i).userArchive,queue.at(i).archiveJid) == key)
			return;
	}

	queue.append(AJob);
	IAccount *account = accountByStream(AJob.streamJid);
	if (account && FState.value(account->accountId()).phase!=MamRuntime::Syncing)
		setPhase(account->accountId(),MamRuntime::Syncing);
	pump(AJob.streamJid);
}

void MamArchive::pump(const Jid &AStreamJid)
{
	if (!FFileArchive || !FFileArchive->isDatabaseReady(AStreamJid))
		return;
	if (FActive.contains(AStreamJid))
		return;

	QList<MamJob> &queue = FQueues[AStreamJid];
	if (queue.isEmpty())
		return;

	MamJob job = queue.takeFirst();
	QString afterId = readCursor(AStreamJid,job);
	if (!sendQuery(job,afterId))
	{
		FFailed[AStreamJid].insert(archiveKey(job.userArchive,job.archiveJid));
		IAccount *account = accountByStream(AStreamJid);
		if (account && FQueues.value(AStreamJid).isEmpty())
			setPhase(account->accountId(),MamRuntime::Failed,tr("Could not send the archive request"));
		QTimer::singleShot(0,this,SLOT(onContinueSync()));
	}
}

bool MamArchive::sendQuery(const MamJob &AJob, const QString &AAfterId)
{
	if (!FStanzaProcessor)
		return false;

	Stanza stanza(STANZA_KIND_IQ);
	stanza.setType(STANZA_TYPE_SET).setUniqueId();
	if (!AJob.userArchive)
		stanza.setTo(AJob.archiveJid.bare());

	QDomElement queryElem = stanza.addElement("query",NS_MAM);
	queryElem.setAttribute("queryid",stanza.id());

	QDomElement xElem = queryElem.appendChild(stanza.createElement("x",NS_JABBER_DATA)).toElement();
	xElem.setAttribute("type","submit");
	QDomElement fieldElem = xElem.appendChild(stanza.createElement("field")).toElement();
	fieldElem.setAttribute("var","FORM_TYPE");
	fieldElem.setAttribute("type","hidden");
	fieldElem.appendChild(stanza.createElement("value")).appendChild(stanza.createTextNode(NS_MAM));

	QDate start = startDate(AJob.streamJid);
	if (start.isValid())
	{
		QDomElement startField = xElem.appendChild(stanza.createElement("field")).toElement();
		startField.setAttribute("var","start");
		QString stamp = DateTime(QDateTime(start,QTime(0,0))).toX85UTC();
		startField.appendChild(stanza.createElement("value")).appendChild(stanza.createTextNode(stamp));
	}

	QDomElement setElem = queryElem.appendChild(stanza.createElement("set",NS_RESULTSET)).toElement();
	setElem.appendChild(stanza.createElement("max")).appendChild(stanza.createTextNode(QString::number(MAM_PAGE_SIZE)));
	if (!AAfterId.isEmpty())
		setElem.appendChild(stanza.createElement("after")).appendChild(stanza.createTextNode(AAfterId));

	if (!FStanzaProcessor->sendStanzaRequest(this,AJob.streamJid,stanza,MAM_REQUEST_TIMEOUT))
	{
		LOG_STRM_WARNING(AJob.streamJid,"Failed to send MAM query");
		return false;
	}

	MamQuery query;
	query.userArchive = AJob.userArchive;
	query.archiveJid = AJob.archiveJid;
	query.queryId = stanza.id();
	query.afterId = AAfterId;
	FActive.insert(AJob.streamJid,query);

	LOG_STRM_DEBUG(AJob.streamJid,QString("MAM query sent, archive=%1, id=%2, after=%3").arg(AJob.userArchive ? AJob.streamJid.pBare() : AJob.archiveJid.pBare(),stanza.id(),AAfterId));
	return true;
}

bool MamArchive::readArchivedMessage(const QDomElement &AResult, Message &AMessage) const
{
	QDomElement forwardedElem = AResult.firstChildElement("forwarded");
	while (!forwardedElem.isNull() && forwardedElem.namespaceURI()!=NS_MESSAGE_FORWARD)
		forwardedElem = forwardedElem.nextSiblingElement("forwarded");
	if (forwardedElem.isNull())
		return false;

	QDomElement delayElem = forwardedElem.firstChildElement("delay");
	while (!delayElem.isNull() && delayElem.namespaceURI()!=NS_XMPP_DELAY)
		delayElem = delayElem.nextSiblingElement("delay");

	QDomElement messageElem = forwardedElem.firstChildElement("message");
	if (messageElem.isNull() || delayElem.isNull())
		return false;

	QDateTime stamp = DateTime(delayElem.attribute("stamp")).toLocal();
	if (!stamp.isValid())
		return false;

	AMessage = Message(Stanza(messageElem));
	if (AMessage.type()==Message::Error || AMessage.body().isEmpty())
		return false;

	AMessage.setDelayed(stamp,Jid::null);
	return true;
}

QString MamArchive::messageKey(const Message &AMessage, const Jid &AWith) const
{
	QString actor;
	QString kind = "chat";
	if (AMessage.type() == Message::GroupChat)
	{
		kind = "groupchat";
		if (AMessage.fromJid().pBare()==AWith.pBare() && AMessage.fromJid().hasResource())
			actor = AMessage.fromJid().pResource();
		else
			actor = AMessage.toJid().pResource();
	}
	else if (AMessage.fromJid().pBare() == AWith.pBare())
	{
		actor = "in";
	}
	else
	{
		actor = "out";
	}
	return kind + "\n" + actor + "\n" + QString::number(AMessage.dateTime().toUTC().toSecsSinceEpoch()) + "\n" + AMessage.body();
}

QDateTime MamArchive::collectionStart(const QDateTime &ATime) const
{
	QDateTime start = ATime;
	int msecs = start.time().msec();
	if (msecs != 0)
		start = start.addMSecs(-msecs);
	return start;
}

QString MamArchive::headerKey(const IArchiveHeader &AHeader) const
{
	return AHeader.with.pFull() + "\n" + DateTime(AHeader.start).toX85UTC();
}

bool MamArchive::headerMatches(const IArchiveHeader &AHeader, const Jid &AWith, bool AGroupChat) const
{
	if (AGroupChat)
		return AHeader.with.pBare()==AWith.pBare() && !AHeader.with.hasResource();
	if (AHeader.with.pFull() == AWith.pFull())
		return true;
	return !AWith.hasResource() && !AHeader.with.hasResource() && AHeader.with.pBare()==AWith.pBare();
}

MamContactIndex &MamArchive::contactIndex(const Jid &AStreamJid, const Jid &ABare)
{
	MamContactIndex &index = FContacts[AStreamJid][ABare.pBare()];
	if (!index.loaded)
		loadContactIndex(AStreamJid,ABare,index);
	return index;
}

void MamArchive::loadContactIndex(const Jid &AStreamJid, const Jid &ABare, MamContactIndex &AIndex) const
{
	AIndex.loaded = true;
	AIndex.openRefreshed = true;
	AIndex.keys.clear();
	AIndex.collections.clear();
	if (!FFileArchive)
		return;

	IArchiveRequest request;
	request.with = ABare;
	request.exactmatch = false;
	request.order = Qt::AscendingOrder;

	QList<IArchiveHeader> openHeaders;
	IArchiveRequest openRequest = request;
	openRequest.openOnly = true;
	openHeaders = FFileArchive->loadFileHeaders(AStreamJid,openRequest);

	QList<IArchiveHeader> headers = FFileArchive->loadFileHeaders(AStreamJid,request);
	foreach (const IArchiveHeader &header, headers)
	{
		IArchiveCollection collection = FFileArchive->loadFileCollection(AStreamJid,header);
		MamStoredCollection stored;
		stored.header = collection.header.with.isValid() ? collection.header : header;
		stored.last = stored.header.start;
		stored.open = openHeaders.contains(stored.header);
		foreach (const Message &message, collection.body.messages)
		{
			if (message.dateTime().isValid() && message.dateTime()>stored.last)
				stored.last = message.dateTime();
			if (!message.body().isEmpty())
				AIndex.keys.insert(messageKey(message,stored.header.with));
		}
		AIndex.collections.append(stored);
	}
}

void MamArchive::refreshOpenCollections(const Jid &AStreamJid, const Jid &ABare, MamContactIndex &AIndex) const
{
	if (AIndex.openRefreshed || !FFileArchive)
		return;
	AIndex.openRefreshed = true;

	IArchiveRequest request;
	request.with = ABare;
	request.exactmatch = false;
	request.openOnly = true;
	QList<IArchiveHeader> openHeaders = FFileArchive->loadFileHeaders(AStreamJid,request);

	foreach (const IArchiveHeader &header, openHeaders)
	{
		int index = -1;
		for (int i=0; i<AIndex.collections.count(); ++i)
		{
			if (AIndex.collections.at(i).header == header)
			{
				index = i;
				AIndex.collections[i].open = true;
				break;
			}
		}
		if (index < 0)
		{
			MamStoredCollection stored;
			stored.header = header;
			stored.last = header.start;
			stored.open = true;
			AIndex.collections.append(stored);
			index = AIndex.collections.count()-1;
		}

		IArchiveCollection collection = FFileArchive->loadFileCollection(AStreamJid,header);
		MamStoredCollection &stored = AIndex.collections[index];
		foreach (const Message &message, collection.body.messages)
		{
			if (message.dateTime().isValid() && message.dateTime()>stored.last)
				stored.last = message.dateTime();
			if (!message.body().isEmpty())
				AIndex.keys.insert(messageKey(message,stored.header.with));
		}
	}
}

bool MamArchive::prepareMessage(const Jid &AStreamJid, const MamQuery &AQuery, Message &AMessage, Jid &AWith, bool &AGroupChat) const
{
	if (!AMessage.dateTime().isValid() || AMessage.body().isEmpty())
		return false;

	AGroupChat = !AQuery.userArchive || AMessage.type()==Message::GroupChat;
	if (AGroupChat)
	{
		if (!AMessage.fromJid().hasResource())
			return false;
		AMessage.setType(Message::GroupChat);
		AWith = Jid(AMessage.fromJid().bare());
		return AWith.isValid();
	}

	bool outgoing = AMessage.fromJid().pBare() == AStreamJid.pBare();
	Jid contact = outgoing ? AMessage.toJid() : AMessage.fromJid();
	if (!contact.isValid() || contact.pBare()==AStreamJid.pBare())
		return false;

	AWith = contact;
	if (outgoing)
	{
		AMessage.setFrom(AStreamJid.full());
		AMessage.setTo(contact.full());
	}
	else
	{
		AMessage.setFrom(contact.full());
		AMessage.setTo(AStreamJid.full());
	}
	if (AMessage.type()!=Message::Chat && AMessage.type()!=Message::Normal)
		AMessage.setType(Message::Chat);
	return true;
}

bool MamArchive::importMessages(const Jid &AStreamJid, const MamQuery &AQuery)
{
	QList<Message> messages = AQuery.messages;
	std::sort(messages.begin(),messages.end());

	QMap<QString, MamContactIndex> &contacts = FContacts[AStreamJid];
	for (QMap<QString, MamContactIndex>::iterator it=contacts.begin(); it!=contacts.end(); ++it)
		it->openRefreshed = false;

	QHash<QString, IArchiveCollection> dirty;
	int stored = 0;

	foreach (Message message, messages)
	{
		Jid with;
		bool groupChat = false;
		if (!prepareMessage(AStreamJid,AQuery,message,with,groupChat))
			continue;

		MamContactIndex &index = contactIndex(AStreamJid,Jid(with.bare()));
		QString key = messageKey(message,with);
		if (index.keys.contains(key))
			continue;
		refreshOpenCollections(AStreamJid,Jid(with.bare()),index);
		if (index.keys.contains(key))
			continue;

		int selected = -1;
		for (int i=0; i<index.collections.count(); ++i)
		{
			const MamStoredCollection &storedCollection = index.collections.at(i);
			if (storedCollection.open || !headerMatches(storedCollection.header,with,groupChat))
				continue;
			if (storedCollection.header.start>message.dateTime())
				continue;
			if (message.dateTime() > storedCollection.last.addSecs(MAM_SESSION_GAP))
				continue;
			if (selected<0 || storedCollection.header.start>index.collections.at(selected).header.start)
				selected = i;
		}

		if (selected < 0)
		{
			IArchiveHeader header;
			header.engineId = FILEMESSAGEARCHIVE_UUID;
			header.with = with;
			header.start = collectionStart(message.dateTime());
			header.version = 0;

			for (int shift=0; shift<5 && selected<0; ++shift)
			{
				if (shift > 0)
					header.start = header.start.addSecs(1);
				for (int i=0; i<index.collections.count(); ++i)
				{
					if (index.collections.at(i).header == header)
					{
						if (!index.collections.at(i).open)
							selected = i;
						break;
					}
				}
				if (selected>=0)
					break;
				bool occupied = false;
				for (int i=0; i<index.collections.count(); ++i)
				{
					if (index.collections.at(i).header == header)
					{
						occupied = true;
						break;
					}
				}
				if (!occupied)
				{
					MamStoredCollection created;
					created.header = header;
					created.last = message.dateTime();
					created.open = false;
					index.collections.append(created);
					selected = index.collections.count()-1;
				}
			}
			if (selected < 0)
				continue;
		}

		MamStoredCollection &target = index.collections[selected];
		if (message.dateTime() > target.last)
			target.last = message.dateTime();
		index.keys.insert(key);

		IArchiveCollection &collection = dirty[headerKey(target.header)];
		if (!collection.header.with.isValid())
			collection.header = target.header;
		collection.body.messages.append(message);
		stored++;
	}

	bool ok = true;
	for (QHash<QString, IArchiveCollection>::const_iterator it=dirty.constBegin(); it!=dirty.constEnd(); ++it)
	{
		IArchiveHeader saved = FFileArchive->saveFileCollection(AStreamJid,it.value());
		if (!saved.with.isValid() || !saved.start.isValid())
			ok = false;
	}

	IAccount *account = accountByStream(AStreamJid);
	if (account)
	{
		FState[account->accountId()].written += stored;
		emit accountStatusChanged(account->accountId());
	}

	LOG_STRM_INFO(AStreamJid,QString("MAM page stored, archive=%1, received=%2, written=%3").arg(
		AQuery.userArchive ? AStreamJid.pBare() : AQuery.archiveJid.pBare()).arg(messages.count()).arg(stored));
	return ok;
}

void MamArchive::finishQuery(const Jid &AStreamJid)
{
	if (!FActive.contains(AStreamJid))
		return;

	MamQuery query = FActive.take(AStreamJid);
	MamJob job;
	job.streamJid = AStreamJid;
	job.userArchive = query.userArchive;
	job.archiveJid = query.archiveJid;
	QString key = archiveKey(job.userArchive,job.archiveJid);
	bool saved = importMessages(AStreamJid,query);
	IAccount *account = accountByStream(AStreamJid);
	if (!downloadEnabled(AStreamJid))
	{
		if (account)
			setPhase(account->accountId(),MamRuntime::Disabled);
	}
	else if (!saved)
	{
		LOG_STRM_WARNING(AStreamJid,QString("Failed to store MAM page, archive=%1").arg(job.userArchive ? AStreamJid.pBare() : job.archiveJid.pBare()));
		FFailed[AStreamJid].insert(key);
		if (account)
			setPhase(account->accountId(),MamRuntime::Failed,tr("Could not write messages into the local archive"));
	}
	else if (!query.complete && !query.lastId.isEmpty() && query.lastId!=query.afterId)
	{
		writeCursor(AStreamJid,job,query.lastId);
		FQueues[AStreamJid].prepend(job);
	}
	else
	{
		if (!query.lastId.isEmpty())
			writeCursor(AStreamJid,job,query.lastId);
		FDone[AStreamJid].insert(key);
		LOG_STRM_INFO(AStreamJid,QString("MAM sync finished, archive=%1, complete=%2").arg(job.userArchive ? AStreamJid.pBare() : job.archiveJid.pBare()).arg(query.complete ? "true" : "false"));
		if (account && FQueues.value(AStreamJid).isEmpty())
		{
			if (FFailed.value(AStreamJid).isEmpty())
				setPhase(account->accountId(),MamRuntime::Current);
			else
				setPhase(account->accountId(),MamRuntime::Failed,FState.value(account->accountId()).error.isEmpty() ? tr("Server history download failed.") : FState.value(account->accountId()).error);
		}
	}

	QTimer::singleShot(0,this,SLOT(onContinueSync()));
}

void MamArchive::clearStream(const Jid &AStreamJid)
{
	if (FStanzaProcessor && FSHIMam.contains(AStreamJid))
		FStanzaProcessor->removeStanzaHandle(FSHIMam.take(AStreamJid));
	FQueues.remove(AStreamJid);
	FActive.remove(AStreamJid);
	FDone.remove(AStreamJid);
	FFailed.remove(AStreamJid);
	FWatchedRooms.remove(AStreamJid);
	FFinishPending.remove(AStreamJid);
	FContacts.remove(AStreamJid);
	FServerMam.remove(AStreamJid);
}

void MamArchive::onXmppStreamOpened(IXmppStream *AXmppStream)
{
	if (FStanzaProcessor)
	{
		IStanzaHandle shandle;
		shandle.handler = this;
		shandle.order = SHO_MI_MAM;
		shandle.direction = IStanzaHandle::DirectionIn;
		shandle.streamJid = AXmppStream->streamJid();
		shandle.conditions.append(SHC_MAM_RESULT);
		FSHIMam.insert(shandle.streamJid,FStanzaProcessor->insertStanzaHandle(shandle));
	}

	IAccount *account = accountByStream(AXmppStream->streamJid());
	if (account)
	{
		MamRuntime::Phase phase = FState.value(account->accountId()).phase;
		if (!downloadEnabled(AXmppStream->streamJid()))
			setPhase(account->accountId(),MamRuntime::Disabled);
		else if (phase!=MamRuntime::Syncing && phase!=MamRuntime::Unsupported && phase!=MamRuntime::Failed)
			setPhase(account->accountId(),MamRuntime::Checking);
	}

	scheduleRooms(AXmppStream->streamJid());
	if (account)
		requestPrefs(account);
}

void MamArchive::onDiscoOpened(const Jid &AStreamJid)
{
	if (!FDiscovery)
		return;

	Jid accountJid = AStreamJid.bare();
	if (FDiscovery->hasDiscoInfo(AStreamJid,accountJid))
		onDiscoInfoReceived(FDiscovery->discoInfo(AStreamJid,accountJid));
	else
		FDiscovery->requestDiscoInfo(AStreamJid,accountJid);
}

void MamArchive::onXmppStreamClosed(IXmppStream *AXmppStream)
{
	IAccount *account = accountByStream(AXmppStream->streamJid());
	clearStream(AXmppStream->streamJid());
	if (account)
	{
		clearPrefs(account);
		setPhase(account->accountId(),MamRuntime::Offline);
	}
}

void MamArchive::onDiscoInfoReceived(const IDiscoInfo &AInfo)
{
	if (!AInfo.node.isEmpty())
		return;

	bool accountJid = !AInfo.contactJid.hasResource() && AInfo.contactJid.pBare()==AInfo.streamJid.pBare();
	bool domainJid = AInfo.contactJid.pFull()==AInfo.streamJid.pDomain();
	if (accountJid || domainJid)
	{
		IAccount *account = accountByStream(AInfo.streamJid);
		if (!AInfo.error.isNull())
		{
			if (accountJid && account && !FServerMam.value(AInfo.streamJid))
				setPhase(account->accountId(),MamRuntime::Failed,AInfo.error.errorMessage());
			return;
		}

		bool mam = AInfo.features.contains(NS_MAM);
		if (mam)
		{
			bool known = FServerMam.contains(AInfo.streamJid) && FServerMam.value(AInfo.streamJid);
			FServerMam.insert(AInfo.streamJid,true);
			LOG_STRM_INFO(AInfo.streamJid,QString("Server history is available, from=%1").arg(AInfo.contactJid.pBare()));
			if (!downloadEnabled(AInfo.streamJid))
			{
				if (account)
					setPhase(account->accountId(),MamRuntime::Disabled);
				return;
			}
			if (account && !known)
				startJobs(account,FState.value(account->accountId()).phase!=MamRuntime::Syncing);
			return;
		}

		if (accountJid && !FServerMam.value(AInfo.streamJid))
		{
			FServerMam.insert(AInfo.streamJid,false);
			LOG_STRM_INFO(AInfo.streamJid,QString("Server history is not advertised, from=%1").arg(AInfo.contactJid.pBare()));
			if (!downloadEnabled(AInfo.streamJid))
			{
				if (account)
					setPhase(account->accountId(),MamRuntime::Disabled);
				return;
			}
			if (account && !FActive.contains(AInfo.streamJid) && FQueues.value(AInfo.streamJid).isEmpty())
				setPhase(account->accountId(),MamRuntime::Unsupported);
		}
		return;
	}

	if (!AInfo.error.isNull() || !AInfo.features.contains(NS_MAM))
		return;

	if (isKnownRoom(AInfo.streamJid,AInfo.contactJid))
	{
		MamJob job;
		job.streamJid = AInfo.streamJid;
		job.userArchive = false;
		job.archiveJid = Jid(AInfo.contactJid.bare());
		enqueue(job);
	}
}

void MamArchive::onBookmarksOpened(const Jid &AStreamJid)
{
	if (!FBookmarks)
		return;

	foreach (const IBookmark &bookmark, FBookmarks->bookmarks(AStreamJid))
	{
		if (bookmark.type == IBookmark::TypeRoom)
			watchRoom(AStreamJid,bookmark.room.roomJid);
	}
}

void MamArchive::onBookmarksChanged(const Jid &AStreamJid)
{
	onBookmarksOpened(AStreamJid);
}

void MamArchive::onMultiUserChatCreated(IMultiUserChat *AMultiChat)
{
	if (AMultiChat)
		watchRoom(AMultiChat->streamJid(),AMultiChat->roomJid());
}

void MamArchive::onDatabaseOpened(const Jid &AStreamJid)
{
	pump(AStreamJid);
}

void MamArchive::onContinueSync()
{
	QList<Jid> streams = FQueues.keys();
	foreach (const Jid &streamJid, streams)
		pump(streamJid);
}

void MamArchive::onFinishQuery()
{
	QList<Jid> streams;
	for (QSet<Jid>::const_iterator it=FFinishPending.constBegin(); it!=FFinishPending.constEnd(); ++it)
		streams.append(*it);
	FFinishPending.clear();
	foreach (const Jid &streamJid, streams)
		finishQuery(streamJid);
}

bool MamArchive::prefsAvailable(const QUuid &AAccountId) const
{
	return FPrefs.value(AAccountId).availability == MamPrefsState::Ready;
}

QString MamArchive::prefsPolicy(const QUuid &AAccountId) const
{
	return FPrefs.value(AAccountId).policy;
}

QStringList MamArchive::prefsAlways(const QUuid &AAccountId) const
{
	return FPrefs.value(AAccountId).always;
}

QStringList MamArchive::prefsNever(const QUuid &AAccountId) const
{
	return FPrefs.value(AAccountId).never;
}

QString MamArchive::prefsNote(const QUuid &AAccountId) const
{
	const MamPrefsState state = FPrefs.value(AAccountId);
	if (state.availability == MamPrefsState::Failed)
		return tr("Server retention settings could not be loaded: %1").arg(state.saveError);
	return QString();
}

QString MamArchive::prefsError(const QUuid &AAccountId) const
{
	const MamPrefsState state = FPrefs.value(AAccountId);
	if (state.availability == MamPrefsState::Ready)
		return state.saveError;
	return QString();
}

void MamArchive::setAccountPrefs(IAccount *AAccount, const QString &APolicy, const QStringList &AAlways, const QStringList &ANever)
{
	if (!AAccount || !FStanzaProcessor || !AAccount->xmppStream() || !AAccount->xmppStream()->isOpen())
		return;
	if (APolicy!="always" && APolicy!="roster" && APolicy!="never")
		return;

	MamPrefsState &state = FPrefs[AAccount->accountId()];
	state.saveError.clear();

	Stanza stanza(STANZA_KIND_IQ);
	stanza.setType(STANZA_TYPE_SET).setUniqueId();
	stanza.setTo(AAccount->streamJid().bare());
	QDomElement prefsElem = stanza.addElement("prefs",NS_MAM);
	prefsElem.setAttribute("default",APolicy);

	QDomElement alwaysElem = prefsElem.appendChild(stanza.createElement("always")).toElement();
	foreach (const QString &jid, AAlways)
		alwaysElem.appendChild(stanza.createElement("jid")).appendChild(stanza.createTextNode(jid));
	QDomElement neverElem = prefsElem.appendChild(stanza.createElement("never")).toElement();
	foreach (const QString &jid, ANever)
		neverElem.appendChild(stanza.createElement("jid")).appendChild(stanza.createTextNode(jid));

	if (sendPrefsRequest(AAccount,stanza,true,APolicy,AAlways,ANever))
	{
		state.policy = APolicy;
		state.always = AAlways;
		state.never = ANever;
	}
	else
	{
		state.saveError = tr("Could not send retention settings");
	}
	emit accountPrefsChanged(AAccount->accountId());
}

void MamArchive::requestPrefs(IAccount *AAccount)
{
	if (!AAccount || !FStanzaProcessor || !AAccount->xmppStream() || !AAccount->xmppStream()->isOpen())
		return;

	foreach (const QString &id, FPrefsRequests.keys())
	{
		if (FPrefsRequests.value(id)==AAccount->accountId() && !FPrefsRequestSet.value(id))
			return;
	}

	Stanza stanza(STANZA_KIND_IQ);
	stanza.setType(STANZA_TYPE_GET).setUniqueId();
	stanza.setTo(AAccount->streamJid().bare());
	stanza.addElement("prefs",NS_MAM);
	sendPrefsRequest(AAccount,stanza,false,QString(),QStringList(),QStringList());
}

bool MamArchive::sendPrefsRequest(IAccount *AAccount, Stanza &AStanza, bool ASetting, const QString &APolicy, const QStringList &AAlways, const QStringList &ANever)
{
	QStringList previous;
	foreach (const QString &id, FPrefsRequests.keys())
	{
		if (FPrefsRequests.value(id)==AAccount->accountId() && FPrefsRequestSet.value(id)==ASetting)
			previous.append(id);
	}
	foreach (const QString &id, previous)
	{
		FPrefsRequests.remove(id);
		FPrefsRequestSet.remove(id);
		FPrefsRequestPolicy.remove(id);
		FPrefsRequestAlways.remove(id);
		FPrefsRequestNever.remove(id);
	}

	if (!FStanzaProcessor->sendStanzaRequest(this,AAccount->streamJid(),AStanza,MAM_REQUEST_TIMEOUT))
	{
		LOG_STRM_WARNING(AAccount->streamJid(),QString("Failed to send server retention request, id=%1").arg(AStanza.id()));
		return false;
	}

	FPrefsRequests.insert(AStanza.id(),AAccount->accountId());
	FPrefsRequestSet.insert(AStanza.id(),ASetting);
	FPrefsRequestPolicy.insert(AStanza.id(),APolicy);
	FPrefsRequestAlways.insert(AStanza.id(),AAlways);
	FPrefsRequestNever.insert(AStanza.id(),ANever);
	return true;
}

bool MamArchive::readPrefsElement(const QDomElement &APrefs, QString &APolicy, QStringList &AAlways, QStringList &ANever) const
{
	if (APrefs.isNull() || APrefs.namespaceURI()!=NS_MAM)
		return false;

	APolicy = APrefs.attribute("default");
	if (APolicy!="always" && APolicy!="roster" && APolicy!="never")
		return false;

	AAlways.clear();
	ANever.clear();
	QDomElement alwaysElem = APrefs.firstChildElement("always");
	QDomElement neverElem = APrefs.firstChildElement("never");
	QDomElement jidElem = alwaysElem.firstChildElement("jid");
	while (!jidElem.isNull())
	{
		Jid jid(jidElem.text().trimmed());
		if (jid.isValid())
			AAlways.append(jid.hasResource() ? jid.pFull() : jid.pBare());
		jidElem = jidElem.nextSiblingElement("jid");
	}
	jidElem = neverElem.firstChildElement("jid");
	while (!jidElem.isNull())
	{
		Jid jid(jidElem.text().trimmed());
		if (jid.isValid())
			ANever.append(jid.hasResource() ? jid.pFull() : jid.pBare());
		jidElem = jidElem.nextSiblingElement("jid");
	}
	AAlways.removeDuplicates();
	ANever.removeDuplicates();
	AAlways.sort();
	ANever.sort();
	return true;
}

void MamArchive::handlePrefsResult(const Stanza &AStanza)
{
	QUuid accountId = FPrefsRequests.take(AStanza.id());
	bool setting = FPrefsRequestSet.take(AStanza.id());
	QString sentPolicy = FPrefsRequestPolicy.take(AStanza.id());
	QStringList sentAlways = FPrefsRequestAlways.take(AStanza.id());
	QStringList sentNever = FPrefsRequestNever.take(AStanza.id());

	IAccount *account = FAccountManager!=NULL ? FAccountManager->findAccountById(accountId) : NULL;
	MamPrefsState &state = FPrefs[accountId];
	if (AStanza.isResult())
	{
		QString policy;
		QStringList always;
		QStringList never;
		if (readPrefsElement(AStanza.firstElement("prefs",NS_MAM),policy,always,never))
		{
			state.availability = MamPrefsState::Ready;
			state.policy = policy;
			state.always = always;
			state.never = never;
			if (setting)
				state.saveError.clear();
			if (account)
				LOG_STRM_INFO(account->streamJid(),QString("Server retention preferences loaded, default=%1").arg(policy));
		}
		else if (setting)
		{
			state.availability = MamPrefsState::Ready;
			state.policy = sentPolicy;
			state.always = sentAlways;
			state.never = sentNever;
			state.saveError.clear();
			if (account)
				requestPrefs(account);
		}
		else
		{
			state.availability = MamPrefsState::Failed;
			state.saveError = tr("The server returned an incomplete retention response");
		}
	}
	else
	{
		XmppStanzaError err(AStanza);
		XmppStanzaError::ErrorCondition code = err.conditionCode();
		bool unsupported = code==XmppStanzaError::EC_FEATURE_NOT_IMPLEMENTED || code==XmppStanzaError::EC_FORBIDDEN || code==XmppStanzaError::EC_NOT_ALLOWED || code==XmppStanzaError::EC_SERVICE_UNAVAILABLE;
		if (account)
			LOG_STRM_INFO(account->streamJid(),QString("Server retention request failed, id=%1: %2").arg(AStanza.id(),err.condition()));
		if (!setting && unsupported)
		{
			state.availability = MamPrefsState::Unavailable;
			state.saveError.clear();
		}
		else
		{
			state.saveError = err.errorMessage();
			if (!setting)
				state.availability = MamPrefsState::Failed;
		}
	}

	emit accountPrefsChanged(accountId);
}

void MamArchive::clearPrefs(IAccount *AAccount)
{
	if (!AAccount)
		return;

	QStringList ids;
	foreach (const QString &id, FPrefsRequests.keys())
	{
		if (FPrefsRequests.value(id) == AAccount->accountId())
			ids.append(id);
	}
	foreach (const QString &id, ids)
	{
		FPrefsRequests.remove(id);
		FPrefsRequestSet.remove(id);
		FPrefsRequestPolicy.remove(id);
		FPrefsRequestAlways.remove(id);
		FPrefsRequestNever.remove(id);
	}

	FPrefs.remove(AAccount->accountId());
	emit accountPrefsChanged(AAccount->accountId());
}

IAccount *MamArchive::accountByStream(const Jid &AStreamJid) const
{
	return FAccountManager!=NULL ? FAccountManager->findAccountByStream(AStreamJid) : NULL;
}

bool MamArchive::downloadEnabled(const Jid &AStreamJid) const
{
	IAccount *account = accountByStream(AStreamJid);
	return account!=NULL ? account->optionsNode().value("mam-download").toBool() : true;
}

bool MamArchive::conferencesEnabled(const Jid &AStreamJid) const
{
	IAccount *account = accountByStream(AStreamJid);
	return account!=NULL ? account->optionsNode().value("mam-conferences").toBool() : true;
}

QDate MamArchive::startDate(const Jid &AStreamJid) const
{
	IAccount *account = accountByStream(AStreamJid);
	if (!account)
		return QDate();
	return QDate::fromString(account->optionsNode().value("mam-start").toString(),Qt::ISODate);
}

void MamArchive::setPhase(const QUuid &AAccountId, MamRuntime::Phase APhase, const QString &AError)
{
	MamRuntime &state = FState[AAccountId];
	state.phase = APhase;
	if (APhase == MamRuntime::Failed)
		state.error = AError;
	else if (APhase != MamRuntime::Syncing)
		state.error.clear();
	if (APhase == MamRuntime::Current)
		state.finished = QDateTime::currentDateTime();
	emit accountStatusChanged(AAccountId);
}

void MamArchive::beginSync(const QUuid &AAccountId, bool AResetCount)
{
	MamRuntime &state = FState[AAccountId];
	if (AResetCount || state.phase!=MamRuntime::Syncing)
		state.written = 0;
	state.phase = MamRuntime::Syncing;
	state.error.clear();
	emit accountStatusChanged(AAccountId);
}

void MamArchive::stopWork(const Jid &AStreamJid)
{
	FQueues.remove(AStreamJid);
	FActive.remove(AStreamJid);
	FFinishPending.remove(AStreamJid);
}

void MamArchive::clearCursors(const Jid &AStreamJid)
{
	QString path = cursorFilePath(AStreamJid);
	if (!path.isEmpty())
		QFile::remove(path);
}

void MamArchive::startJobs(IAccount *AAccount, bool AResetCount)
{
	if (!AAccount || !downloadEnabled(AAccount->streamJid()))
		return;
	beginSync(AAccount->accountId(),AResetCount);
	MamJob job;
	job.streamJid = AAccount->streamJid();
	job.userArchive = true;
	enqueue(job);
	scheduleRooms(AAccount->streamJid());
}

void MamArchive::scheduleRooms(const Jid &AStreamJid)
{
	if (!downloadEnabled(AStreamJid) || !conferencesEnabled(AStreamJid))
		return;

	if (FMultiChat)
	{
		foreach (IMultiUserChat *chat, FMultiChat->multiUserChats())
		{
			if (chat->streamJid() == AStreamJid)
				watchRoom(AStreamJid,chat->roomJid());
		}
	}

	if (FBookmarks && FBookmarks->isReady(AStreamJid))
		onBookmarksOpened(AStreamJid);
}
