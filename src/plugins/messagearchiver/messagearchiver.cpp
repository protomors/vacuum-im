#include "messagearchiver.h"

#include <utils/xmlutils.h>
#include <functional>
#include <utils/qtutils.h>

#include <QDir>
#include <QFile>
#include <QActionGroup>
#include <definitions/resources.h>
#include <definitions/menuicons.h>
#include <definitions/shortcuts.h>
#include <definitions/namespaces.h>
#include <definitions/actiongroups.h>
#include <definitions/toolbargroups.h>
#include <definitions/internalerrors.h>
#include <definitions/optionvalues.h>
#include <definitions/optionnodes.h>
#include <definitions/optionnodeorders.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/messagedataroles.h>
#include <definitions/rosterindexkinds.h>
#include <definitions/rosterindexroles.h>
#include <definitions/stanzahandlerorders.h>
#include <definitions/shortcutgrouporders.h>
#include <definitions/statisticsparams.h>
#include <utils/widgetmanager.h>
#include <utils/xmpperror.h>
#include <utils/shortcuts.h>
#include <utils/options.h>
#include <utils/logger.h>

#define ARCHIVE_DIR_NAME           "archive"
#define PENDING_FILE_NAME          "pending.xml"
#define ARCHIVE_REQUEST_TIMEOUT    30000

#define SHC_MESSAGE_BODY           "/message/body"

#define ADR_STREAM_JID             Action::DR_StreamJid
#define ADR_CONTACT_JID            Action::DR_Parametr1
#define ADR_ITEM_SAVE              Action::DR_Parametr2
#define ADR_FILTER_START           Action::DR_Parametr2
#define ADR_FILTER_END             Action::DR_Parametr3
#define ADR_GROUP_KIND             Action::DR_Parametr4

#define PST_ARCHIVE_PREFS          "pref"
#define PSN_ARCHIVE_PREFS          NS_ARCHIVE

MessageArchiver::MessageArchiver()
{
	FPluginManager = NULL;
	FXmppStreamManager = NULL;
	FStanzaProcessor = NULL;
	FOptionsManager = NULL;
	FPrivateStorage = NULL;
	FAccountManager = NULL;
	FRostersViewPlugin = NULL;
	FDataForms = NULL;
	FMessageWidgets = NULL;
	FRosterManager = NULL;
	FMultiChatManager = NULL;
}

MessageArchiver::~MessageArchiver()
{

}

void MessageArchiver::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("History");
	APluginInfo->description = tr("Allows to save the history of communications");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(XMPPSTREAMS_UUID);
	APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool MessageArchiver::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	FPluginManager = APluginManager;

	IPlugin *plugin = APluginManager->pluginInterface("IXmppStreamManager").value(0,NULL);
	if (plugin)
	{
		FXmppStreamManager = qobject_cast<IXmppStreamManager *>(plugin->instance());
		if (FXmppStreamManager)
		{
			connect(FXmppStreamManager->instance(),SIGNAL(streamOpened(IXmppStream *)),SLOT(onXmppStreamOpened(IXmppStream *)));
			connect(FXmppStreamManager->instance(),SIGNAL(streamClosed(IXmppStream *)),SLOT(onXmppStreamClosed(IXmppStream *)));
			connect(FXmppStreamManager->instance(),SIGNAL(streamAboutToClose(IXmppStream *)),SLOT(onXmppStreamAboutToClose(IXmppStream *)));
		}
	}

	plugin = APluginManager->pluginInterface("IStanzaProcessor").value(0,NULL);
	if (plugin)
	{
		FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IPrivateStorage").value(0,NULL);
	if (plugin)
	{
		FPrivateStorage = qobject_cast<IPrivateStorage *>(plugin->instance());
		if (FPrivateStorage)
		{
			connect(FPrivateStorage->instance(),SIGNAL(dataSaved(const QString &, const Jid &, const QDomElement &)),
				SLOT(onPrivateDataLoadedSaved(const QString &, const Jid &, const QDomElement &)));
			connect(FPrivateStorage->instance(),SIGNAL(dataLoaded(const QString &, const Jid &, const QDomElement &)),
				SLOT(onPrivateDataLoadedSaved(const QString &, const Jid &, const QDomElement &)));
			connect(FPrivateStorage->instance(),SIGNAL(dataChanged(const Jid &, const QString &, const QString &)),
				SLOT(onPrivateDataChanged(const Jid &, const QString &, const QString &)));
			connect(FPrivateStorage->instance(),SIGNAL(storageOpened(const Jid &)),SLOT(onPrivateStorageOpened(const Jid &)));
		}
	}

	plugin = APluginManager->pluginInterface("IAccountManager").value(0,NULL);
	if (plugin)
	{
		FAccountManager = qobject_cast<IAccountManager *>(plugin->instance());
		if (FAccountManager)
		{
			connect(FAccountManager->instance(),SIGNAL(accountInserted(IAccount *)),SLOT(onAccountInserted(IAccount *)));
			connect(FAccountManager->instance(),SIGNAL(accountRemoved(IAccount *)),SLOT(onAccountRemoved(IAccount *)));
		}
	}

	plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
	if (plugin)
	{
		FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
		if (FRostersViewPlugin)
		{
			connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(indexMultiSelection(const QList<IRosterIndex *> &, bool &)),
				SLOT(onRostersViewIndexMultiSelection(const QList<IRosterIndex *> &, bool &)));
			connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(indexContextMenu(const QList<IRosterIndex *> &, quint32, Menu *)),
				SLOT(onRostersViewIndexContextMenu(const QList<IRosterIndex *> &, quint32, Menu *)));
		}
	}

	plugin = APluginManager->pluginInterface("IDataForms").value(0,NULL);
	if (plugin)
	{
		FDataForms = qobject_cast<IDataForms *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(toolBarWidgetCreated(IMessageToolBarWidget *)),SLOT(onToolBarWidgetCreated(IMessageToolBarWidget *)));
		}
	}

	plugin = APluginManager->pluginInterface("IRosterManager").value(0,NULL);
	if (plugin)
	{
		FRosterManager = qobject_cast<IRosterManager *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMultiUserChatManager").value(0,NULL);
	if (plugin)
	{
		FMultiChatManager = qobject_cast<IMultiUserChatManager *>(plugin->instance());
		if (FMultiChatManager)
		{
			connect(FMultiChatManager->instance(),SIGNAL(multiUserContextMenu(IMultiUserChatWindow *, IMultiUser *, Menu *)),
				SLOT(onMultiUserContextMenu(IMultiUserChatWindow *, IMultiUser *, Menu *)));
		}
	}

	connect(this,SIGNAL(requestFailed(const QString &, const XmppError &)),
		SLOT(onSelfRequestFailed(const QString &, const XmppError &)));
	connect(this,SIGNAL(headersLoaded(const QString &, const QList<IArchiveHeader> &)),
		SLOT(onSelfHeadersLoaded(const QString &, const QList<IArchiveHeader> &)));
	connect(this,SIGNAL(collectionLoaded(const QString &, const IArchiveCollection &)),
		SLOT(onSelfCollectionLoaded(const QString &, const IArchiveCollection &)));

	connect(Options::instance(),SIGNAL(optionsChanged(const OptionsNode &)),SLOT(onOptionsChanged(const OptionsNode &)));
	connect(Shortcuts::instance(),SIGNAL(shortcutActivated(const QString &, QWidget *)),SLOT(onShortcutActivated(const QString &, QWidget *)));

	return FXmppStreamManager!=NULL && FStanzaProcessor!=NULL;
}

bool MessageArchiver::initObjects()
{
	Shortcuts::declareShortcut(SCT_MESSAGEWINDOWS_SHOWHISTORY, tr("Show history"), tr("Ctrl+H","Show history"));
	Shortcuts::declareShortcut(SCT_ROSTERVIEW_SHOWHISTORY,tr("Show history"),tr("Ctrl+H","Show history"),Shortcuts::WidgetShortcut);

	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HISTORY_HEADERS_LOAD_ERROR,tr("Failed to load conversation headers"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HISTORY_CONVERSATION_SAVE_ERROR,tr("Failed to save conversation"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HISTORY_CONVERSATION_LOAD_ERROR,tr("Failed to load conversation"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HISTORY_CONVERSATION_REMOVE_ERROR,tr("Failed to remove conversation"));
	XmppError::registerError(NS_INTERNAL_ERROR,IERR_HISTORY_MODIFICATIONS_LOAD_ERROR,tr("Failed to load archive modifications"));

	if (FRostersViewPlugin)
	{
		Shortcuts::insertWidgetShortcut(SCT_ROSTERVIEW_SHOWHISTORY,FRostersViewPlugin->rostersView()->instance());
	}
	if (FOptionsManager)
	{
		IOptionsDialogNode historyNode = { ONO_HISTORY, OPN_HISTORY, MNI_HISTORY, tr("History") };
		FOptionsManager->insertOptionsDialogNode(historyNode);
		FOptionsManager->insertOptionsDialogHolder(this);
	}
	return true;
}

bool MessageArchiver::initSettings()
{
	Options::setDefaultValue(OPV_HISTORY_ENGINE_ENABLED,true);
	Options::setDefaultValue(OPV_HISTORY_ENGINE_REPLICATEAPPEND,true);
	Options::setDefaultValue(OPV_HISTORY_ENGINE_REPLICATEREMOVE,true);
	Options::setDefaultValue(OPV_HISTORY_ARCHIVEVIEW_FONTPOINTSIZE,10);

	Options::setDefaultValue(OPV_ACCOUNT_HISTORYREPLICATE,false);

	return true;
}

bool MessageArchiver::stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept)
{
	Q_UNUSED(AAccept);
	if (FSHIMessageIn.value(AStreamJid) == AHandlerId)
	{
		Message message(AStanza);
		processMessage(AStreamJid,message,true);
	}
	else if (FSHIMessageOut.value(AStreamJid) == AHandlerId)
	{
		Message message(AStanza);
		processMessage(AStreamJid,message,false);
	}
	return false;
}

QMultiMap<int, IOptionsDialogWidget *> MessageArchiver::optionsDialogWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsDialogWidget *>  widgets;
	QStringList nodeTree = ANodeId.split(".",Qt::SkipEmptyParts);
	if (nodeTree.count()==3 && nodeTree.at(0)==OPN_ACCOUNTS && nodeTree.at(2)=="History")
	{
		IAccount *account = FAccountManager!=NULL ? FAccountManager->findAccountById(QUuid::fromString(nodeTree.at(1))) : NULL;
		if (account!=NULL && isReady(account->streamJid()))
		{
			OptionsNode options = account->optionsNode();

			widgets.insert(OHO_ACCOUNTS_HISTORY_SERVERSETTINGS, FOptionsManager->newOptionsDialogHeader(tr("Archive preferences"),AParent));
			widgets.insert(OWO_ACCOUNTS_HISTORY_SERVERSETTINGS, new ArchiveAccountOptionsWidget(this,account->streamJid(),AParent));

			int replCount = 0;
			int manualCount = 0;
			foreach(IArchiveEngine *engine, archiveEngines())
			{
				if (engine->isCapable(account->streamJid(),IArchiveEngine::ArchiveReplication))
					replCount++;
				else if (engine->isCapable(account->streamJid(),IArchiveEngine::ManualArchiving))
					manualCount++;
			}

			if (replCount>0 && replCount+manualCount>1)
			{
				widgets.insert(OHO_ACCOUNTS_HISTORY_REPLICATION,FOptionsManager->newOptionsDialogHeader(tr("Archive synchronization"),AParent));
				widgets.insert(OWO_ACCOUNTS_HISTORY_REPLICATION,FOptionsManager->newOptionsDialogWidget(options.node("history-replicate"),tr("Synchronize history between archives"),AParent));
			}
		}
	}
	else if (ANodeId == OPN_HISTORY)
	{
		int index = 0;
		widgets.insert(OHO_HISORY_ENGINES, FOptionsManager->newOptionsDialogHeader(tr("Used history archives"),AParent));
		foreach(IArchiveEngine *engine, archiveEngines())
		{
			OptionsNode node = Options::node(OPV_HISTORY_ENGINE_ITEM,engine->engineId().toString()).node("enabled");
			widgets.insert(OWO_HISORY_ENGINE,FOptionsManager->newOptionsDialogWidget(node,engine->engineName(),AParent));

			IOptionsDialogWidget *engineSettings = engine->engineSettingsWidget(AParent);
			if (engineSettings)
			{
				widgets.insert(OHO_HISTORY_ENGINNAME + index,FOptionsManager->newOptionsDialogHeader(engine->engineName(),AParent));
				widgets.insert(OWO_HISTORY_ENGINESETTINGS + index,engineSettings);
				index += 10;
			}
		}
	}
	return widgets;
}

bool MessageArchiver::isReady(const Jid &AStreamJid) const
{
	return FArchivePrefs.contains(AStreamJid);
}

QString MessageArchiver::archiveDirPath(const Jid &AStreamJid) const
{
	if (FArchiveDirPath.isEmpty())
	{
		QDir dir(FPluginManager->homePath());
		dir.mkdir(ARCHIVE_DIR_NAME);
		FArchiveDirPath = dir.cd(ARCHIVE_DIR_NAME) ? dir.absolutePath() : QString();
	}
	if (AStreamJid.isValid() && !FArchiveDirPath.isEmpty())
	{
		QString streamDir = Jid::encode(AStreamJid.pBare());

		QDir dir(FArchiveDirPath);
		dir.mkdir(streamDir);
		return dir.cd(streamDir) ? dir.absolutePath() : QString();
	}
	return FArchiveDirPath;
}

QWidget *MessageArchiver::showArchiveWindow(const QMultiMap<Jid,Jid> &AAddresses)
{
	ArchiveViewWindow *window = new ArchiveViewWindow(this,AAddresses);
	WidgetManager::showActivateRaiseWindow(window);
	return window;
}

bool MessageArchiver::isArchivePrefsEnabled(const Jid &AStreamJid) const
{
	return isReady(AStreamJid);
}

bool MessageArchiver::isArchiveReplicationEnabled(const Jid &AStreamJid) const
{
	IAccount *account = FAccountManager!=NULL ? FAccountManager->findAccountByStream(AStreamJid) : NULL;
	return account!=NULL ? account->optionsNode().value("history-replicate").toBool() : false;
}

bool MessageArchiver::isArchivingAllowed(const Jid &AStreamJid, const Jid &AItemJid, const QString &AThreadId) const
{
	if (isReady(AStreamJid) && AItemJid.isValid())
	{
		IArchiveItemPrefs itemPrefs = archiveItemPrefs(AStreamJid, AItemJid, AThreadId);
		return itemPrefs.save != ARCHIVE_SAVE_FALSE;
	}
	return false;
}

IArchiveStreamPrefs MessageArchiver::archivePrefs(const Jid &AStreamJid) const
{
	return FArchivePrefs.value(AStreamJid);
}

IArchiveItemPrefs MessageArchiver::archiveItemPrefs(const Jid &AStreamJid, const Jid &AItemJid, const QString &AThreadId) const
{
	IArchiveStreamPrefs streamPrefs = archivePrefs(AStreamJid);

	IArchiveItemPrefs sessionPrefs, domainPrefs, barePrefs, fullPrefs;
	if (!AThreadId.isEmpty() && streamPrefs.sessionPrefs.contains(AThreadId))
	{
		IArchiveSessionPrefs sprefs = streamPrefs.sessionPrefs.value(AThreadId);
		sessionPrefs = archiveItemPrefs(AStreamJid,AItemJid);
		sessionPrefs.save = sprefs.save;
	}
	else for (QMap<Jid, IArchiveItemPrefs>::const_iterator it = streamPrefs.itemPrefs.constBegin(); it != streamPrefs.itemPrefs.constEnd(); ++it)
	{
		QString node = it.key().pNode();
		QString domain = it.key().pDomain();
		QString resource = it.key().pResource();
		if (it->exactmatch)
		{
			if (it.key() == AItemJid)
			{
				fullPrefs = it.value();
				break;
			}
		}
		else if (domain == AItemJid.pDomain())
		{
			if (node == AItemJid.pNode())
			{
				if (resource == AItemJid.pResource())
				{
					fullPrefs = it.value();
					break;
				}
				else if (resource.isEmpty())
				{
					barePrefs = it.value();
				}
			}
			else if (resource.isEmpty() && node.isEmpty())
			{
				domainPrefs = it.value();
			}
		}
	}

	IArchiveItemPrefs itemPrefs;
	if (!sessionPrefs.save.isEmpty())
		itemPrefs = sessionPrefs;
	else if (!fullPrefs.save.isEmpty())
		itemPrefs = fullPrefs;
	else if (!barePrefs.save.isEmpty())
		itemPrefs = barePrefs;
	else if (!domainPrefs.save.isEmpty())
		itemPrefs = domainPrefs;
	else
		itemPrefs = streamPrefs.defaultPrefs;

	return itemPrefs;
}

QString MessageArchiver::setArchivePrefs(const Jid &AStreamJid, const IArchiveStreamPrefs &APrefs)
{
	if (isArchivePrefsEnabled(AStreamJid) && FPrivateStorage!=NULL)
	{
		IArchiveStreamPrefs oldPrefs = archivePrefs(AStreamJid);
		IArchiveStreamPrefs newPrefs = oldPrefs;

		if (!APrefs.defaultPrefs.save.isEmpty())
		{
			newPrefs.defaultPrefs.save = APrefs.defaultPrefs.save;
			newPrefs.defaultPrefs.expire = APrefs.defaultPrefs.expire;
		}

		foreach(const Jid &itemJid, APrefs.itemPrefs.keys())
		{
			IArchiveItemPrefs newItemPrefs = APrefs.itemPrefs.value(itemJid);
			if (!newItemPrefs.save.isEmpty())
				newPrefs.itemPrefs.insert(itemJid,newItemPrefs);
			else
				newPrefs.itemPrefs.remove(itemJid);
		}

		foreach(const QString &threadId, APrefs.sessionPrefs.keys())
		{
			IArchiveSessionPrefs newSessionPrefs = APrefs.sessionPrefs.value(threadId);
			if (!newSessionPrefs.save.isEmpty())
				newPrefs.sessionPrefs.insert(threadId,newSessionPrefs);
			else
				newPrefs.sessionPrefs.remove(threadId);
		}

		bool changed = oldPrefs.defaultPrefs!=newPrefs.defaultPrefs;
		changed = changed || oldPrefs.itemPrefs.keys()!=newPrefs.itemPrefs.keys() || oldPrefs.sessionPrefs.keys()!=newPrefs.sessionPrefs.keys();
		for (QMap<Jid,IArchiveItemPrefs>::const_iterator it=newPrefs.itemPrefs.constBegin(); !changed && it!=newPrefs.itemPrefs.constEnd(); ++it)
			changed = oldPrefs.itemPrefs.value(it.key())!=it.value();
		for (QMap<QString,IArchiveSessionPrefs>::const_iterator it=newPrefs.sessionPrefs.constBegin(); !changed && it!=newPrefs.sessionPrefs.constEnd(); ++it)
			changed = oldPrefs.sessionPrefs.value(it.key())!=it.value();

		if (changed)
		{
			QDomDocument doc;
			QDomElement prefElem = doc.appendChild(doc.createElementNS(PSN_ARCHIVE_PREFS,PST_ARCHIVE_PREFS)).toElement();

			QDomElement defElem = prefElem.appendChild(doc.createElement("default")).toElement();
			if (newPrefs.defaultPrefs.expire > 0)
				defElem.setAttribute("expire",newPrefs.defaultPrefs.expire);
			defElem.setAttribute("save",newPrefs.defaultPrefs.save);

			for (QMap<Jid,IArchiveItemPrefs>::const_iterator it=newPrefs.itemPrefs.constBegin(); it!=newPrefs.itemPrefs.constEnd(); ++it)
			{
				QDomElement itemElem = prefElem.appendChild(doc.createElement("item")).toElement();
				if (it->expire > 0)
					itemElem.setAttribute("expire",it->expire);
				if (it->exactmatch)
					itemElem.setAttribute("exactmatch",QVariant(it->exactmatch).toString());
				itemElem.setAttribute("jid",it.key().full());
				itemElem.setAttribute("save",it->save);
			}

			for (QMap<QString,IArchiveSessionPrefs>::const_iterator it=newPrefs.sessionPrefs.constBegin(); it!=newPrefs.sessionPrefs.constEnd(); ++it)
			{
				QDomElement sessionElem = prefElem.appendChild(doc.createElement("session")).toElement();
				sessionElem.setAttribute("thread",it.key());
				sessionElem.setAttribute("save",it->save);
				if (it->timeout >= 0)
					sessionElem.setAttribute("timeout",it->timeout);
			}

			QString requestId = FPrivateStorage->saveData(AStreamJid,prefElem);
			if (!requestId.isEmpty())
			{
				LOG_STRM_INFO(AStreamJid,QString("Update archive prefs request sent, id=%1").arg(requestId));
				FPrefsSaveRequests.insert(requestId,AStreamJid);
				return requestId;
			}
			else
			{
				LOG_STRM_WARNING(AStreamJid,QString("Failed to send update archive prefs request"));
			}
		}
	}
	return QString();
}

QString MessageArchiver::removeArchiveItemPrefs(const Jid &AStreamJid, const Jid &AItemJid)
{
	if (isArchivePrefsEnabled(AStreamJid) && archivePrefs(AStreamJid).itemPrefs.contains(AItemJid))
	{
		IArchiveStreamPrefs prefs;
		prefs.itemPrefs[AItemJid].save = QString();
		return setArchivePrefs(AStreamJid,prefs);
	}
	return QString();
}

QString MessageArchiver::removeArchiveSessionPrefs(const Jid &AStreamJid, const QString &AThreadId)
{
	if (isArchivePrefsEnabled(AStreamJid) && archivePrefs(AStreamJid).sessionPrefs.contains(AThreadId))
	{
		IArchiveStreamPrefs prefs;
		prefs.sessionPrefs[AThreadId].save = QString();
		return setArchivePrefs(AStreamJid,prefs);
	}
	return QString();
}

bool MessageArchiver::saveMessage(const Jid &AStreamJid, const Jid &AItemJid, const Message &AMessage)
{
	if (isArchivingAllowed(AStreamJid,AItemJid,AMessage.threadId()))
	{
		IArchiveEngine *engine = findEngineByCapability(AStreamJid,IArchiveEngine::DirectArchiving);
		if (engine)
		{
			Message message = AMessage;
			bool directionIn = AItemJid==message.from() || AStreamJid==message.to();
			if (prepareMessage(AStreamJid,message,directionIn))
				return engine->saveMessage(AStreamJid,message,directionIn);
		}
	}
	return false;
}

bool MessageArchiver::saveNote(const Jid &AStreamJid, const Jid &AItemJid, const QString &ANote, const QString &AThreadId)
{
	if (isArchivingAllowed(AStreamJid,AItemJid,AThreadId))
	{
		IArchiveEngine *engine = findEngineByCapability(AStreamJid,IArchiveEngine::DirectArchiving);
		if (engine)
		{
			Message message;
			message.setTo(AStreamJid.full()).setFrom(AItemJid.full()).setBody(ANote).setThreadId(AThreadId);
			return engine->saveNote(AStreamJid,message,true);
		}
	}
	return false;
}

QString MessageArchiver::loadMessages(const Jid &AStreamJid, const IArchiveRequest &ARequest)
{
	QString id = loadHeaders(AStreamJid,ARequest);
	if (!id.isEmpty())
	{
		MessagesRequest request;
		request.request = ARequest;
		request.streamJid = AStreamJid;
		QString localId = QUuid::createUuid().toString();
		FRequestId2LocalId.insert(id,localId);
		FMesssagesRequests.insert(localId,request);
		LOG_STRM_DEBUG(AStreamJid,QString("Load messages request sent, id=%1").arg(localId));
		Logger::startTiming(STMP_HISTORY_MESSAGES_LOAD,localId);
		return localId;
	}
	else
	{
		LOG_STRM_WARNING(AStreamJid,"Failed to send load messages request: Headers not requested");
	}
	return QString();
}

QString MessageArchiver::loadHeaders(const Jid &AStreamJid, const IArchiveRequest &ARequest)
{
	HeadersRequest request;
	QString localId = QUuid::createUuid().toString();
	foreach(IArchiveEngine *engine, engineOrderByCapability(AStreamJid,IArchiveEngine::ArchiveManagement))
	{
		if (ARequest.text.isEmpty() || engine->isCapable(AStreamJid,IArchiveEngine::FullTextSearch))
		{
			QString id = engine->loadHeaders(AStreamJid,ARequest);
			if (!id.isEmpty())
			{
				request.engines.append(engine);
				FRequestId2LocalId.insert(id,localId);
			}
			else
			{
				LOG_STRM_WARNING(AStreamJid,QString("Failed to send load headers request to engine=%1").arg(engine->engineName()));
			}
		}
	}

	if (!request.engines.isEmpty())
	{
		request.request = ARequest;
		FHeadersRequests.insert(localId,request);
		LOG_STRM_DEBUG(AStreamJid,QString("Load headers request sent to %1 engines, id=%2").arg(request.engines.count()).arg(localId));
		Logger::startTiming(STMP_HISTORY_HEADERS_LOAD,localId);
		return localId;
	}
	else
	{
		LOG_STRM_WARNING(AStreamJid,"Failed to send load headers request to any engine");
	}

	return QString();
}

QString MessageArchiver::loadCollection(const Jid &AStreamJid, const IArchiveHeader &AHeader)
{
	IArchiveEngine *engine = findArchiveEngine(AHeader.engineId);
	if (engine)
	{
		QString id = engine->loadCollection(AStreamJid,AHeader);
		if (!id.isEmpty())
		{
			CollectionRequest request;
			QString localId = QUuid::createUuid().toString();
			FRequestId2LocalId.insert(id,localId);
			FCollectionRequests.insert(localId,request);
			LOG_STRM_DEBUG(AStreamJid,QString("Load collection request sent to engine=%1, id=%2").arg(engine->engineName(),localId));
			return localId;
		}
		else
		{
			LOG_STRM_WARNING(AStreamJid,QString("Failed to send load collection request to engine=%1").arg(engine->engineName()));
		}
	}
	else
	{
		REPORT_ERROR("Failed to send load collection request: Engine not found");
	}
	return QString();
}

QString MessageArchiver::removeCollections(const Jid &AStreamJid, const IArchiveRequest &ARequest)
{
	RemoveRequest request;
	QString localId = QUuid::createUuid().toString();
	foreach(IArchiveEngine *engine, engineOrderByCapability(AStreamJid,IArchiveEngine::ArchiveManagement))
	{
		QString id = engine->removeCollections(AStreamJid,ARequest);
		if (!id.isEmpty())
		{
			FRequestId2LocalId.insert(id,localId);
			request.engines.append(engine);
		}
		else
		{
			LOG_STRM_WARNING(AStreamJid,QString("Failed to send remove collections request to engine=%1").arg(engine->engineName()));
		}
	}

	if (!request.engines.isEmpty())
	{
		request.request = ARequest;
		FRemoveRequests.insert(localId,request);
		LOG_STRM_DEBUG(AStreamJid,QString("Remove collections request sent to %1 engines, id=%2").arg(request.engines.count()).arg(localId));
		return localId;
	}
	else
	{
		LOG_STRM_WARNING(AStreamJid,"Failed to send remove collections request to any engine");
	}

	return QString();
}

void MessageArchiver::elementToCollection(const Jid &AStreamJid, const QDomElement &AChatElem, IArchiveCollection &ACollection) const
{
	ACollection.header.with = AChatElem.attribute("with");
	ACollection.header.start = DateTime(AChatElem.attribute("start")).toLocal();
	ACollection.header.subject = AChatElem.attribute("subject");
	ACollection.header.threadId = AChatElem.attribute("thread");
	ACollection.header.version = AChatElem.attribute("version").toUInt();

	QDomElement nodeElem = AChatElem.firstChildElement();

	bool isSecsFromStart;
	if (!AChatElem.hasAttribute("secsFromLast"))
	{
		int secsLast = 0;
		isSecsFromStart = true;
		while (!nodeElem.isNull() && isSecsFromStart)
		{
			if (nodeElem.hasAttribute("secs"))
			{
				int secs = nodeElem.attribute("secs").toInt();
				if (secs < secsLast)
					isSecsFromStart = false;
				secsLast = secs;
			}
			nodeElem = nodeElem.nextSiblingElement();
		}
	}
	else
	{
		isSecsFromStart = AChatElem.attribute("secsFromLast")!="true";
	}

	int secsLast = 0;
	QDateTime lastMessageDT;
	nodeElem = AChatElem.firstChildElement();
	while (!nodeElem.isNull())
	{
		if (nodeElem.tagName()=="to" || nodeElem.tagName()=="from")
		{
			Message message;

			Jid with = ACollection.header.with;
			QString nick = nodeElem.attribute("name");
			Jid contactJid(with.node(),with.domain(),nick.isEmpty() ? with.resource() : nick);

			if (nodeElem.tagName()=="to")
			{
				message.setTo(contactJid.full());
				message.setFrom(AStreamJid.full());
				message.setData(MDR_MESSAGE_DIRECTION,IMessageProcessor::DirectionOut);
			}
			else
			{
				message.setTo(AStreamJid.full());
				message.setFrom(contactJid.full());
				message.setData(MDR_MESSAGE_DIRECTION,IMessageProcessor::DirectionIn);
			}

			message.setType(nick.isEmpty() ? Message::Chat : Message::GroupChat);

			QString utc = nodeElem.attribute("utc");
			if (utc.isEmpty())
			{
				int secs = nodeElem.attribute("secs").toInt();
				QDateTime messageDT = ACollection.header.start.addSecs(isSecsFromStart ? secs : secsLast+secs);

				if (lastMessageDT.isValid() && lastMessageDT>=messageDT)
					messageDT = lastMessageDT.addMSecs(1);

				message.setDateTime(messageDT);
				lastMessageDT = messageDT;
			}
			else
			{
				QDateTime messageDT = DateTime(utc).toLocal();
				message.setDateTime(messageDT.isValid() ? messageDT : ACollection.header.start.addSecs(secsLast));
			}
			secsLast = ACollection.header.start.secsTo(message.dateTime());

			QDomElement childElem = nodeElem.firstChildElement();
			while (!childElem.isNull())
			{
				message.stanza().element().appendChild(childElem.cloneNode(true));
				childElem = childElem.nextSiblingElement();
			}
			message.setThreadId(ACollection.header.threadId);

			ACollection.body.messages.append(message);
		}
		else if (nodeElem.tagName() == "note")
		{
			QString utc = nodeElem.attribute("utc");
			ACollection.body.notes.insert(DateTime(utc).toLocal(),nodeElem.text());
		}
		else if(nodeElem.tagName() == "next")
		{
			ACollection.next.with = nodeElem.attribute("with");
			ACollection.next.start = DateTime(nodeElem.attribute("start")).toLocal();
		}
		else if(nodeElem.tagName() == "previous")
		{
			ACollection.previous.with = nodeElem.attribute("with");
			ACollection.previous.start = DateTime(nodeElem.attribute("start")).toLocal();
		}
		else if (FDataForms && nodeElem.tagName()=="x" && nodeElem.namespaceURI()==NS_JABBER_DATA)
		{
			ACollection.attributes = FDataForms->dataForm(nodeElem);
		}
		nodeElem = nodeElem.nextSiblingElement();
	}
}

void MessageArchiver::collectionToElement(const IArchiveCollection &ACollection, QDomElement &AChatElem, const QString &ASaveMode) const
{
	QDomDocument ownerDoc = AChatElem.ownerDocument();
	AChatElem.setAttribute("with",ACollection.header.with.full());
	AChatElem.setAttribute("start",DateTime(ACollection.header.start).toX85UTC());
	AChatElem.setAttribute("version",ACollection.header.version);
	if (!ACollection.header.subject.isEmpty())
		AChatElem.setAttribute("subject",ACollection.header.subject);
	if (!ACollection.header.threadId.isEmpty())
		AChatElem.setAttribute("thread",ACollection.header.threadId);
	AChatElem.setAttribute("secsFromLast","false");

	bool groupChat = false;
	QList<Message>::const_iterator messageIt = ACollection.body.messages.constBegin();
	QMultiMap<QDateTime,QString>::const_iterator noteIt = ACollection.body.notes.constBegin();
	while(messageIt!=ACollection.body.messages.constEnd() || noteIt!=ACollection.body.notes.constEnd())
	{
		bool writeNote = false;
		bool writeMessage = false;
		if (noteIt == ACollection.body.notes.constEnd())
			writeMessage = true;
		else if (messageIt == ACollection.body.messages.constEnd())
			writeNote = true;
		else if (messageIt->dateTime() <= noteIt.key())
			writeMessage = true;
		else
			writeNote = true;

		if (writeMessage)
		{
			groupChat |= messageIt->type()==Message::GroupChat;
			if (!groupChat || messageIt->fromJid().hasResource())
			{
				bool directionIn = ACollection.header.with.pBare() == messageIt->fromJid().pBare();
				QDomElement messageElem = AChatElem.appendChild(ownerDoc.createElement(directionIn ? "from" : "to")).toElement();

				int secs = ACollection.header.start.secsTo(messageIt->dateTime());
				if (secs >= 0)
					messageElem.setAttribute("secs",secs);
				else
					messageElem.setAttribute("utc",DateTime(messageIt->dateTime()).toX85UTC());

				if (groupChat)
					messageElem.setAttribute("name",messageIt->fromJid().resource());

				if (ASaveMode==ARCHIVE_SAVE_MESSAGE || ASaveMode==ARCHIVE_SAVE_STREAM)
				{
					QDomElement childElem = messageIt->stanza().element().firstChildElement();
					while (!childElem.isNull())
					{
						if (childElem.tagName() != "thread")
							messageElem.appendChild(childElem.cloneNode(true));
						childElem = childElem.nextSiblingElement();
					}
				}
				else if (ASaveMode == ARCHIVE_SAVE_BODY)
				{
					messageElem.appendChild(ownerDoc.createElement("body")).appendChild(ownerDoc.createTextNode(messageIt->body()));
				}
			}
			++messageIt;
		}

		if (writeNote)
		{
			QDomElement noteElem = AChatElem.appendChild(ownerDoc.createElement("note")).toElement();
			noteElem.setAttribute("utc",DateTime(noteIt.key()).toX85UTC());
			noteElem.appendChild(ownerDoc.createTextNode(noteIt.value()));
			++noteIt;
		}
	}

	if (ACollection.previous.with.isValid() && ACollection.previous.start.isValid())
	{
		QDomElement prevElem = AChatElem.appendChild(ownerDoc.createElement("previous")).toElement();
		prevElem.setAttribute("with",ACollection.previous.with.full());
		prevElem.setAttribute("start",DateTime(ACollection.previous.start).toX85UTC());
	}

	if (ACollection.next.with.isValid() && ACollection.next.start.isValid())
	{
		QDomElement nextElem = AChatElem.appendChild(ownerDoc.createElement("next")).toElement();
		nextElem.setAttribute("with",ACollection.next.with.full());
		nextElem.setAttribute("start",DateTime(ACollection.next.start).toX85UTC());
	}

	if (FDataForms && FDataForms->isFormValid(ACollection.attributes))
	{
		FDataForms->xmlForm(ACollection.attributes,AChatElem);
	}
}

void MessageArchiver::insertArchiveHandler(int AOrder, IArchiveHandler *AHandler)
{
	if (AHandler)
		FArchiveHandlers.insert(AOrder,AHandler);
}

void MessageArchiver::removeArchiveHandler(int AOrder, IArchiveHandler *AHandler)
{
	FArchiveHandlers.remove(AOrder,AHandler);
}

quint32 MessageArchiver::totalCapabilities(const Jid &AStreamJid) const
{
	quint32 caps = 0;
	foreach(IArchiveEngine *engine, FArchiveEngines)
	{
		if (isArchiveEngineEnabled(engine->engineId()))
			caps |= engine->capabilities(AStreamJid);
	}
	return caps;
}

QList<IArchiveEngine *> MessageArchiver::archiveEngines() const
{
	return FArchiveEngines.values();
}

IArchiveEngine *MessageArchiver::findArchiveEngine(const QUuid &AId) const
{
	return FArchiveEngines.value(AId);
}

bool MessageArchiver::isArchiveEngineEnabled(const QUuid &AId) const
{
	return Options::node(OPV_HISTORY_ENGINE_ITEM,AId.toString()).value("enabled").toBool();
}

void MessageArchiver::setArchiveEngineEnabled(const QUuid &AId, bool AEnabled)
{
	if (isArchiveEngineEnabled(AId) != AEnabled)
		Options::node(OPV_HISTORY_ENGINE_ITEM,AId.toString()).setValue(AEnabled,"enabled");
}

void MessageArchiver::registerArchiveEngine(IArchiveEngine *AEngine)
{
	if (AEngine!=NULL && !FArchiveEngines.contains(AEngine->engineId()))
	{
		LOG_DEBUG(QString("Archive engine registered, id=%1, name=%2").arg(AEngine->engineId().toString(),AEngine->engineName()));
		connect(AEngine->instance(),SIGNAL(capabilitiesChanged(const Jid &)),
			SLOT(onEngineCapabilitiesChanged(const Jid &)));
		connect(AEngine->instance(),SIGNAL(requestFailed(const QString &, const XmppError &)),
			SLOT(onEngineRequestFailed(const QString &, const XmppError &)));
		connect(AEngine->instance(),SIGNAL(collectionsRemoved(const QString &, const IArchiveRequest &)),
			SLOT(onEngineCollectionsRemoved(const QString &, const IArchiveRequest &)));
		connect(AEngine->instance(),SIGNAL(headersLoaded(const QString &, const QList<IArchiveHeader> &)),
			SLOT(onEngineHeadersLoaded(const QString &, const QList<IArchiveHeader> &)));
		connect(AEngine->instance(),SIGNAL(collectionLoaded(const QString &, const IArchiveCollection &)),
			SLOT(onEngineCollectionLoaded(const QString &, const IArchiveCollection &)));
		FArchiveEngines.insert(AEngine->engineId(),AEngine);
		emit archiveEngineRegistered(AEngine);
		emit totalCapabilitiesChanged(Jid::null);
	}
}

QString MessageArchiver::archiveFilePath(const Jid &AStreamJid, const QString &AFileName) const
{
	if (AStreamJid.isValid() && !AFileName.isEmpty())
	{
		QString dirPath = archiveDirPath(AStreamJid);
		if (!dirPath.isEmpty())
			return dirPath+"/"+AFileName;
	}
	return QString();
}

QString MessageArchiver::loadStoragePrefs(const Jid &AStreamJid)
{
	QString requestId = FPrivateStorage!=NULL ? FPrivateStorage->loadData(AStreamJid,PST_ARCHIVE_PREFS,PSN_ARCHIVE_PREFS) : QString();
	if (!requestId.isEmpty())
	{
		LOG_STRM_INFO(AStreamJid,QString("Load storage archive prefs request sent, id=%1").arg(requestId));
		FPrefsLoadRequests.insert(requestId,AStreamJid);
	}
	else
	{
		LOG_STRM_WARNING(AStreamJid,"Failed to send load storage archive prefs request");
		applyArchivePrefs(AStreamJid,QDomElement());
	}
	return requestId;
}

void MessageArchiver::applyArchivePrefs(const Jid &AStreamJid, const QDomElement &AElem)
{
	bool initPrefs = !isReady(AStreamJid);
	LOG_STRM_INFO(AStreamJid,QString("Applying new archive prefs, init=%1").arg(initPrefs));

	IArchiveStreamPrefs &prefs = FArchivePrefs[AStreamJid];

	QDomElement defElem = AElem.firstChildElement("default");
	if (!defElem.isNull())
	{
		prefs.defaultPrefs.save = defElem.attribute("save",ARCHIVE_SAVE_MESSAGE);
		prefs.defaultPrefs.expire = defElem.attribute("expire","0").toUInt();
	}
	else
	{
		prefs.defaultPrefs.save = ARCHIVE_SAVE_MESSAGE;
		prefs.defaultPrefs.expire = 0;
	}

	prefs.itemPrefs.clear();
	QDomElement itemElem = AElem.firstChildElement("item");
	while (!itemElem.isNull())
	{
		IArchiveItemPrefs itemPrefs;
		itemPrefs.save = itemElem.attribute("save",prefs.defaultPrefs.save);
		itemPrefs.expire = itemElem.attribute("expire","0").toUInt();
		itemPrefs.exactmatch = QVariant(itemElem.attribute("exactmatch","false")).toBool();
		if (!itemPrefs.save.isEmpty())
			prefs.itemPrefs.insert(itemElem.attribute("jid"),itemPrefs);
		itemElem = itemElem.nextSiblingElement("item");
	}

	prefs.sessionPrefs.clear();
	QDomElement sessionElem = AElem.firstChildElement("session");
	while (!sessionElem.isNull())
	{
		IArchiveSessionPrefs sessionPrefs;
		sessionPrefs.save = sessionElem.attribute("save");
		sessionPrefs.timeout = sessionElem.attribute("timeout","-1").toInt();
		if (!sessionPrefs.save.isEmpty())
			prefs.sessionPrefs.insert(sessionElem.attribute("thread"),sessionPrefs);
		sessionElem = sessionElem.nextSiblingElement("session");
	}

	if (initPrefs)
	{
		emit archivePrefsOpened(AStreamJid);
		processPendingMessages(AStreamJid);
	}
	emit archivePrefsChanged(AStreamJid);
}

void MessageArchiver::loadPendingMessages(const Jid &AStreamJid)
{
	QFile file(archiveFilePath(AStreamJid,PENDING_FILE_NAME));
	if (file.open(QFile::ReadOnly))
	{
		QString xmlError;
		QDomDocument doc;
		if (setDomContent(doc, &file, &xmlError))
		{
			if (AStreamJid.pBare() == doc.documentElement().attribute("jid"))
			{
				QList< QPair<Message,bool> > &messages = FPendingMessages[AStreamJid];
				QDomElement messageElem = doc.documentElement().firstChildElement("message");
				while (!messageElem.isNull())
				{
					bool directionIn = QVariant(messageElem.attribute("x-archive-direction-in")).toBool();
					messageElem.removeAttribute("x-archive-direction-in");

					Stanza stanza(messageElem);
					Message message(stanza);

					if (directionIn)
						message.setTo(AStreamJid.full());
					else
						message.setFrom(AStreamJid.full());
					messages.append(qMakePair(message,directionIn));

					messageElem = messageElem.nextSiblingElement("message");
				}
				LOG_STRM_INFO(AStreamJid,QString("Pending messages loaded, count=%1").arg(messages.count()));
			}
			else
			{
				REPORT_ERROR("Failed to load pending messages from file content: Invalid stream JID");
				file.remove();
			}
		}
		else
		{
			REPORT_ERROR(QString("Failed to load pending messages from file content: %1").arg(xmlError));
			file.remove();
		}
	}
	else if (file.exists())
	{
		REPORT_ERROR(QString("Failed to load pending messages from file: %1").arg(file.errorString()));
	}
}

void MessageArchiver::savePendingMessages(const Jid &AStreamJid)
{
	QList< QPair<Message,bool> > messages = FPendingMessages.take(AStreamJid);
	if (!messages.isEmpty())
	{
		QDomDocument doc;
		doc.appendChild(doc.createElement("pending-messages"));
		doc.documentElement().setAttribute("version","1.0");
		doc.documentElement().setAttribute("jid",AStreamJid.pBare());

		for (int i=0; i<messages.count(); i++)
		{
			QPair<Message,bool> &message = messages[i];
			message.first.setDelayed(message.first.dateTime(),message.first.from());
			if (prepareMessage(AStreamJid,message.first,message.second))
			{
				QDomElement messageElem = doc.documentElement().appendChild(doc.importNode(message.first.stanza().element(),true)).toElement();
				messageElem.setAttribute("x-archive-direction-in",QVariant(message.second).toString());
			}
		}

		QFile file(archiveFilePath(AStreamJid,PENDING_FILE_NAME));
		if (file.open(QFile::WriteOnly|QFile::Truncate))
		{
			LOG_STRM_INFO(AStreamJid,QString("Pending messages saved, count=%1").arg(messages.count()));
			file.write(doc.toByteArray());
			file.close();
		}
		else
		{
			REPORT_ERROR(QString("Failed to save pending messages to file: %1").arg(file.errorString()));
		}
	}
}

void MessageArchiver::processPendingMessages(const Jid &AStreamJid)
{
	QList< QPair<Message,bool> > messages = FPendingMessages.take(AStreamJid);
	if (!messages.isEmpty())
	{
		LOG_STRM_INFO(AStreamJid,QString("Processing pending messages, count=%1").arg(messages.count()));
		for (int i = 0; i<messages.count(); i++)
		{
			QPair<Message, bool> message = messages.at(i);
			processMessage(AStreamJid, message.first, message.second);
		}
	}
	QFile::remove(archiveFilePath(AStreamJid,PENDING_FILE_NAME));
}

bool MessageArchiver::prepareMessage(const Jid &AStreamJid, Message &AMessage, bool ADirectionIn)
{
	if (AMessage.body().isEmpty())
		return false;
	if (AMessage.type()==Message::Error)
		return false;
	if (AMessage.type()==Message::GroupChat && !ADirectionIn)
		return false;
	if (AMessage.type()==Message::GroupChat && AMessage.isDelayed())
		return false;

	if (ADirectionIn && AMessage.from().isEmpty())
		AMessage.setFrom(AStreamJid.domain());
	else if (!ADirectionIn && AMessage.to().isEmpty())
		AMessage.setTo(AStreamJid.domain());

	for (QMultiMap<int,IArchiveHandler *>::const_iterator it = FArchiveHandlers.constBegin(); it!=FArchiveHandlers.constEnd(); ++it)
	{
		IArchiveHandler *handler = it.value();
		if (handler->archiveMessageEdit(it.key(),AStreamJid,AMessage,ADirectionIn))
			return false;
	}

	if (AMessage.type()==Message::Chat || AMessage.type()==Message::GroupChat)
	{
		if (!AMessage.threadId().isEmpty())
			AMessage.setThreadId(QString());
	}

	return true;
}

bool MessageArchiver::processMessage(const Jid &AStreamJid, const Message &AMessage, bool ADirectionIn)
{
	Jid itemJid = ADirectionIn ? (!AMessage.from().isEmpty() ? AMessage.from() : AStreamJid.domain()) : AMessage.to();
	if (!isReady(AStreamJid))
	{
		FPendingMessages[AStreamJid].append(qMakePair(AMessage,ADirectionIn));
		return true;
	}
	return saveMessage(AStreamJid,itemJid,AMessage);
}

IArchiveEngine *MessageArchiver::findEngineByCapability(const Jid &AStreamJid, quint32 ACapability) const
{
	QMultiMap<int, IArchiveEngine *> order = engineOrderByCapability(AStreamJid,ACapability);
	return !order.isEmpty() ? order.constBegin().value() : NULL;
}

QMultiMap<int, IArchiveEngine *> MessageArchiver::engineOrderByCapability(const Jid &AStreamJid, quint32 ACapability) const
{
	QMultiMap<int, IArchiveEngine *> order;
	for (QMap<QUuid,IArchiveEngine *>::const_iterator it=FArchiveEngines.constBegin(); it!=FArchiveEngines.constEnd(); ++it)
	{
		if (isArchiveEngineEnabled(it.key()))
		{
			int engineOrder = (*it)->capabilityOrder(ACapability,AStreamJid);
			if (engineOrder > 0)
				order.insert(engineOrder,it.value());
		}
	}
	return order;
}

void MessageArchiver::processMessagesRequest(const QString &ALocalId, MessagesRequest &ARequest)
{
	if (!ARequest.lastError.isNull())
	{
		Logger::finishTiming(STMP_HISTORY_MESSAGES_LOAD,ALocalId);
		LOG_WARNING(QString("Failed to load messages, id=%1: %2").arg(ALocalId,ARequest.lastError.condition()));

		emit requestFailed(ALocalId,ARequest.lastError);
		FMesssagesRequests.remove(ALocalId);
	}
	else if (ARequest.headers.isEmpty() || (quint32)ARequest.body.messages.count()>ARequest.request.maxItems)
	{
		if (ARequest.request.order == Qt::AscendingOrder)
			std::sort(ARequest.body.messages.begin(),ARequest.body.messages.end(),std::less<Message>());
		else
			std::sort(ARequest.body.messages.begin(),ARequest.body.messages.end(),[](const Message &left, const Message &right) { return right < left; });

		REPORT_TIMING(STMP_HISTORY_MESSAGES_LOAD,Logger::finishTiming(STMP_HISTORY_MESSAGES_LOAD,ALocalId));
		LOG_DEBUG(QString("Messages successfully loaded, id=%1").arg(ALocalId));

		emit messagesLoaded(ALocalId,ARequest.body);
		FMesssagesRequests.remove(ALocalId);
	}
	else
	{
		QString id = loadCollection(ARequest.streamJid,ARequest.headers.takeFirst());
		if (!id.isEmpty())
		{
			FRequestId2LocalId.insert(id,ALocalId);
		}
		else
		{
			ARequest.lastError = XmppError(IERR_HISTORY_CONVERSATION_LOAD_ERROR);
			processMessagesRequest(ALocalId,ARequest);
		}
	}
}

void MessageArchiver::processHeadersRequest(const QString &ALocalId, HeadersRequest &ARequest)
{
	if (ARequest.engines.count() == ARequest.headers.count())
	{
		if (!ARequest.engines.isEmpty() || ARequest.lastError.isNull())
		{
			QList<IArchiveHeader> headers;
			foreach(IArchiveEngine *engine, ARequest.engines)
			{
				foreach(const IArchiveHeader &header, ARequest.headers.value(engine))
				{
					if (!headers.contains(header))
						headers.append(header);
				}
			}

			if (ARequest.request.order == Qt::AscendingOrder)
				std::sort(headers.begin(),headers.end(),std::less<IArchiveHeader>());
			else
				std::sort(headers.begin(),headers.end(),[](const IArchiveHeader &left, const IArchiveHeader &right) { return right < left; });

			if ((quint32)headers.count() > ARequest.request.maxItems)
				headers = headers.mid(0,ARequest.request.maxItems);

			REPORT_TIMING(STMP_HISTORY_HEADERS_LOAD,Logger::finishTiming(STMP_HISTORY_HEADERS_LOAD,ALocalId));
			LOG_DEBUG(QString("Headers successfully loaded, id=%1").arg(ALocalId));

			emit headersLoaded(ALocalId,headers);
		}
		else
		{
			Logger::finishTiming(STMP_HISTORY_HEADERS_LOAD,ALocalId);
			LOG_WARNING(QString("Failed to load headers, id=%1: %2").arg(ALocalId,ARequest.lastError.condition()));

			emit requestFailed(ALocalId,ARequest.lastError);
		}
		FHeadersRequests.remove(ALocalId);
	}
}

void MessageArchiver::processCollectionRequest(const QString &ALocalId, CollectionRequest &ARequest)
{
	if (ARequest.lastError.isNull())
	{
		LOG_DEBUG(QString("Collection successfully loaded, id=%1").arg(ALocalId));
		emit collectionLoaded(ALocalId,ARequest.collection);
	}
	else
	{
		LOG_WARNING(QString("Failed to load collection, id=%1").arg(ALocalId));
		emit requestFailed(ALocalId,ARequest.lastError);
	}
	FCollectionRequests.remove(ALocalId);
}

void MessageArchiver::processRemoveRequest(const QString &ALocalId, RemoveRequest &ARequest)
{
	if (ARequest.engines.isEmpty())
	{
		if (ARequest.lastError.isNull())
		{
			LOG_DEBUG(QString("Collections successfully removed, id=%1").arg(ALocalId));
			emit collectionsRemoved(ALocalId,ARequest.request);
		}
		else
		{
			LOG_WARNING(QString("Failed to remove collections, id=%1: %2").arg(ALocalId,ARequest.lastError.condition()));
			emit requestFailed(ALocalId,ARequest.lastError);
		}
		FRemoveRequests.remove(ALocalId);
	}
}

void MessageArchiver::openHistoryOptionsNode(const QUuid &AAccountId)
{
	if (FOptionsManager)
	{
		QString historyNodeId = QString(OPN_ACCOUNTS_HISTORY).replace("[id]",AAccountId.toString());
		IOptionsDialogNode historyNode = { ONO_ACCOUNTS_HISTORY, historyNodeId, MNI_HISTORY, tr("History") };
		FOptionsManager->insertOptionsDialogNode(historyNode);
	}
}

void MessageArchiver::closeHistoryOptionsNode(const QUuid &AAccountId)
{
	if (FOptionsManager)
	{
		QString historyNodeId = QString(OPN_ACCOUNTS_HISTORY).replace("[id]",AAccountId.toString());
		FOptionsManager->removeOptionsDialogNode(historyNodeId);
	}
}

bool MessageArchiver::isSelectionAccepted(const QList<IRosterIndex *> &ASelected) const
{
	int singleKind = -1;
	foreach(IRosterIndex *index, ASelected)
	{
		Jid contactJid = index->data(RDR_FULL_JID).toString();
		if (!contactJid.isValid())
			return false;
		else if (singleKind!=-1 && singleKind!=index->kind())
			return false;
		singleKind = index->kind();
	}
	return !ASelected.isEmpty();
}

Menu *MessageArchiver::createContextMenu(const QStringList &AStreams, const QStringList &AContacts, QWidget *AParent) const
{
	bool isMultiSelection = AStreams.count()>1;
	bool isStreamMenu = AContacts.first().isEmpty();

	bool isAnyMangement = false;
	bool isAllPrefsEnabled = true;
	bool isAllDefaultPrefs = !isStreamMenu;
	QString allPrefsSave = QString();
	for (int i=0; i<AStreams.count(); i++)
	{
		isAllPrefsEnabled = isAllPrefsEnabled && isArchivePrefsEnabled(AStreams.at(i));

		if (isAllPrefsEnabled)
		{
			IArchiveItemPrefs itemPrefs = isStreamMenu ? archivePrefs(AStreams.at(i)).defaultPrefs : archiveItemPrefs(AStreams.at(i),AContacts.at(i));

			if (allPrefsSave.isNull())
				allPrefsSave = itemPrefs.save;
			else if (allPrefsSave != itemPrefs.save)
				allPrefsSave = "-=different=-";

			isAllDefaultPrefs = isAllDefaultPrefs && !archivePrefs(AStreams.at(i)).itemPrefs.contains(AContacts.at(i));
		}

		isAnyMangement = isAnyMangement || !engineOrderByCapability(AStreams.at(i),IArchiveEngine::ArchiveManagement).isEmpty();
	}

	Menu *menu = new Menu(AParent);
	menu->setTitle(tr("History"));
	menu->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY);

	if (isAnyMangement)
	{
		Action *viewAction = new Action(menu);
		viewAction->setText(tr("View History"));
		viewAction->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY);
		viewAction->setData(ADR_STREAM_JID,AStreams);
		viewAction->setData(ADR_CONTACT_JID,AContacts);
		viewAction->setShortcutId(SCT_ROSTERVIEW_SHOWHISTORY);
		connect(viewAction,SIGNAL(triggered(bool)),SLOT(onShowArchiveWindowByAction(bool)));
		menu->addAction(viewAction,AG_DEFAULT,false);
	}

	if (isAllPrefsEnabled)
	{
		Action *fullSaveAction = new Action(menu);
		fullSaveAction->setCheckable(true);
		fullSaveAction->setText(tr("Save Messages with Formatting"));
		fullSaveAction->setData(ADR_STREAM_JID,AStreams);
		fullSaveAction->setData(ADR_CONTACT_JID,AContacts);
		fullSaveAction->setData(ADR_ITEM_SAVE,ARCHIVE_SAVE_MESSAGE);
		fullSaveAction->setChecked(allPrefsSave==ARCHIVE_SAVE_MESSAGE || allPrefsSave==ARCHIVE_SAVE_STREAM);
		connect(fullSaveAction,SIGNAL(triggered(bool)),SLOT(onSetItemPrefsByAction(bool)));
		menu->addAction(fullSaveAction,AG_DEFAULT+200);

		Action *bodySaveAction = new Action(menu);
		bodySaveAction->setCheckable(true);
		bodySaveAction->setText(tr("Save Only Messages Text"));
		bodySaveAction->setData(ADR_STREAM_JID,AStreams);
		bodySaveAction->setData(ADR_CONTACT_JID,AContacts);
		bodySaveAction->setData(ADR_ITEM_SAVE,ARCHIVE_SAVE_BODY);
		bodySaveAction->setChecked(allPrefsSave == ARCHIVE_SAVE_BODY);
		connect(bodySaveAction,SIGNAL(triggered(bool)),SLOT(onSetItemPrefsByAction(bool)));
		menu->addAction(bodySaveAction,AG_DEFAULT+200);

		Action *disableSaveAction = new Action(menu);
		disableSaveAction->setCheckable(true);
		disableSaveAction->setText(tr("Do not Save Messages"));
		disableSaveAction->setData(ADR_STREAM_JID,AStreams);
		disableSaveAction->setData(ADR_CONTACT_JID,AContacts);
		disableSaveAction->setData(ADR_ITEM_SAVE,ARCHIVE_SAVE_FALSE);
		disableSaveAction->setChecked(allPrefsSave == ARCHIVE_SAVE_FALSE);
		connect(disableSaveAction,SIGNAL(triggered(bool)),SLOT(onSetItemPrefsByAction(bool)));
		menu->addAction(disableSaveAction,AG_DEFAULT+200);

		QActionGroup *saveGroup = new QActionGroup(menu);
		saveGroup->addAction(fullSaveAction);
		saveGroup->addAction(bodySaveAction);
		saveGroup->addAction(disableSaveAction);

		if (!isStreamMenu)
		{
			Action *defAction = new Action(menu);
			defAction->setCheckable(true);
			defAction->setText(tr("Use Default Options"));
			defAction->setData(ADR_STREAM_JID,AStreams);
			defAction->setData(ADR_CONTACT_JID,AContacts);
			defAction->setChecked(isAllDefaultPrefs);
			connect(defAction,SIGNAL(triggered(bool)),SLOT(onRemoveItemPrefsByAction(bool)));
			menu->addAction(defAction,AG_DEFAULT+500,false);
		}
	}

	if (!isMultiSelection && isStreamMenu && isReady(AStreams.first()))
	{
		Action *optionsAction = new Action(menu);
		optionsAction->setText(tr("Options..."));
		optionsAction->setData(ADR_STREAM_JID,AStreams.first());
		connect(optionsAction,SIGNAL(triggered(bool)),SLOT(onShowHistoryOptionsDialogByAction(bool)));
		menu->addAction(optionsAction,AG_DEFAULT+500,false);
	}

	return menu;
}

void MessageArchiver::onEngineCapabilitiesChanged(const Jid &AStreamJid)
{
	emit totalCapabilitiesChanged(AStreamJid);
}

void MessageArchiver::onEngineRequestFailed(const QString &AId, const XmppError &AError)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		IArchiveEngine *engine = qobject_cast<IArchiveEngine *>(sender());
		if (FHeadersRequests.contains(localId))
		{
			HeadersRequest &request = FHeadersRequests[localId];
			request.lastError = AError;
			request.engines.removeAll(engine);
			processHeadersRequest(localId,request);
		}
		else if (FCollectionRequests.contains(localId))
		{
			CollectionRequest &request = FCollectionRequests[localId];
			request.lastError = AError;
			processCollectionRequest(localId,request);
		}
		else if (FRemoveRequests.contains(localId))
		{
			RemoveRequest &request = FRemoveRequests[localId];
			request.lastError = AError;
			request.engines.removeAll(engine);
			processRemoveRequest(localId,request);
		}
	}
}

void MessageArchiver::onEngineHeadersLoaded(const QString &AId, const QList<IArchiveHeader> &AHeaders)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FHeadersRequests.contains(localId))
		{
			IArchiveEngine *engine = qobject_cast<IArchiveEngine *>(sender());
			HeadersRequest &request = FHeadersRequests[localId];
			request.headers.insert(engine,AHeaders);
			processHeadersRequest(localId,request);
		}
	}
}

void MessageArchiver::onEngineCollectionLoaded(const QString &AId, const IArchiveCollection &ACollection)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FCollectionRequests.contains(localId))
		{
			CollectionRequest &request = FCollectionRequests[localId];
			request.collection = ACollection;
			processCollectionRequest(localId,request);
		}
	}
}

void MessageArchiver::onEngineCollectionsRemoved(const QString &AId, const IArchiveRequest &ARequest)
{
	Q_UNUSED(ARequest);
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FRemoveRequests.contains(localId))
		{
			IArchiveEngine *engine = qobject_cast<IArchiveEngine *>(sender());
			RemoveRequest &request = FRemoveRequests[localId];
			request.engines.removeAll(engine);
			processRemoveRequest(localId,request);
		}
	}
}

void MessageArchiver::onSelfRequestFailed(const QString &AId, const XmppError &AError)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FMesssagesRequests.contains(localId))
		{
			MessagesRequest &request = FMesssagesRequests[localId];
			request.lastError = AError;
			processMessagesRequest(localId,request);
		}
	}
}

void MessageArchiver::onSelfHeadersLoaded(const QString &AId, const QList<IArchiveHeader> &AHeaders)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FMesssagesRequests.contains(localId))
		{
			MessagesRequest &request = FMesssagesRequests[localId];
			request.headers = AHeaders;
			processMessagesRequest(localId,request);
		}
	}
}

void MessageArchiver::onSelfCollectionLoaded(const QString &AId, const IArchiveCollection &ACollection)
{
	if (FRequestId2LocalId.contains(AId))
	{
		QString localId = FRequestId2LocalId.take(AId);
		if (FMesssagesRequests.contains(localId))
		{
			MessagesRequest &request = FMesssagesRequests[localId];
			request.body.messages += ACollection.body.messages;
			request.body.notes += ACollection.body.notes;
			processMessagesRequest(localId,request);
		}
	}
}

void MessageArchiver::onAccountInserted(IAccount *AAccount)
{
	openHistoryOptionsNode(AAccount->accountId());
}

void MessageArchiver::onAccountRemoved(IAccount *AAccount)
{
	closeHistoryOptionsNode(AAccount->accountId());
}

void MessageArchiver::onXmppStreamOpened(IXmppStream *AXmppStream)
{
	if (FStanzaProcessor)
	{
		IStanzaHandle shandle;
		shandle.handler = this;
		shandle.streamJid = AXmppStream->streamJid();

		shandle.order = SHO_DEFAULT;
		shandle.direction = IStanzaHandle::DirectionIn;
		shandle.conditions.append(SHC_MESSAGE_BODY);
		FSHIMessageIn.insert(shandle.streamJid,FStanzaProcessor->insertStanzaHandle(shandle));

		shandle.direction = IStanzaHandle::DirectionOut;
		FSHIMessageOut.insert(shandle.streamJid,FStanzaProcessor->insertStanzaHandle(shandle));
	}

	loadPendingMessages(AXmppStream->streamJid());

	if (FPrivateStorage == NULL)
		applyArchivePrefs(AXmppStream->streamJid(),QDomElement());

	ArchiveReplicator *replicator = new ArchiveReplicator(this,AXmppStream->streamJid(),this);
	FReplicators.insert(AXmppStream->streamJid(),replicator);
}

void MessageArchiver::onXmppStreamClosed(IXmppStream *AXmppStream)
{
	if (FStanzaProcessor)
	{
		FStanzaProcessor->removeStanzaHandle(FSHIMessageIn.take(AXmppStream->streamJid()));
		FStanzaProcessor->removeStanzaHandle(FSHIMessageOut.take(AXmppStream->streamJid()));
	}

	savePendingMessages(AXmppStream->streamJid());

	FArchivePrefs.remove(AXmppStream->streamJid());

	emit archivePrefsChanged(AXmppStream->streamJid());
	emit archivePrefsClosed(AXmppStream->streamJid());
}

void MessageArchiver::onXmppStreamAboutToClose(IXmppStream *AXmppStream)
{
	ArchiveReplicator *replicator = FReplicators.take(AXmppStream->streamJid());
	if (replicator)
		replicator->quitAndDestroy();
}

void MessageArchiver::onPrivateStorageOpened(const Jid &AStreamJid)
{
	loadStoragePrefs(AStreamJid);
}

void MessageArchiver::onPrivateDataLoadedSaved(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement)
{
	if (FPrefsLoadRequests.contains(AId))
	{
		LOG_STRM_INFO(AStreamJid,QString("Storage archive prefs loaded, id=%1").arg(AId));
		FPrefsLoadRequests.remove(AId);
		applyArchivePrefs(AStreamJid,AElement);
		emit requestCompleted(AId);
	}
	else if (FPrefsSaveRequests.contains(AId))
	{
		LOG_STRM_INFO(AStreamJid,QString("Storage archive prefs saved, id=%1").arg(AId));
		FPrefsSaveRequests.remove(AId);
		applyArchivePrefs(AStreamJid,AElement);
		emit requestCompleted(AId);
	}
}

void MessageArchiver::onPrivateDataChanged(const Jid &AStreamJid, const QString &ATagName, const QString &ANamespace)
{
	if (isReady(AStreamJid) && ATagName==PST_ARCHIVE_PREFS && ANamespace==PSN_ARCHIVE_PREFS)
	{
		loadStoragePrefs(AStreamJid);
	}
}

void MessageArchiver::onShortcutActivated(const QString &AId, QWidget *AWidget)
{
	if (FRostersViewPlugin && AWidget==FRostersViewPlugin->rostersView()->instance())
	{
		QList<IRosterIndex *> indexes = FRostersViewPlugin->rostersView()->selectedRosterIndexes();
		if (AId==SCT_ROSTERVIEW_SHOWHISTORY && isSelectionAccepted(indexes))
		{
			QMultiMap<Jid,Jid> addresses;
			foreach(IRosterIndex *index, indexes)
			{
				if (index->kind() == RIK_STREAM_ROOT)
				{
					addresses.insert(index->data(RDR_STREAM_JID).toString(),Jid::null);
				}
				else if (index->kind() == RIK_METACONTACT)
				{
					for (int row=0; row<index->childCount(); row++)
					{
						IRosterIndex *metaItemIndex = index->childIndex(row);
						addresses.insert(metaItemIndex->data(RDR_STREAM_JID).toString(),metaItemIndex->data(RDR_PREP_BARE_JID).toString());
					}
				}
				else
				{
					addresses.insert(index->data(RDR_STREAM_JID).toString(),index->data(RDR_PREP_BARE_JID).toString());
				}
			}
			showArchiveWindow(addresses);
		}
	}
}

void MessageArchiver::onRostersViewIndexMultiSelection(const QList<IRosterIndex *> &ASelected, bool &AAccepted)
{
	AAccepted = AAccepted || isSelectionAccepted(ASelected);
}

void MessageArchiver::onRostersViewIndexContextMenu(const QList<IRosterIndex *> &AIndexes, quint32 ALabelId, Menu *AMenu)
{
	if (ALabelId==AdvancedDelegateItem::DisplayId && isSelectionAccepted(AIndexes))
	{
		int indexKind = AIndexes.first()->kind();
		QMap<int, QStringList> rolesMap = FRostersViewPlugin->rostersView()->indexesRolesMap(AIndexes,
			QList<int>()<<RDR_STREAM_JID<<RDR_PREP_BARE_JID<<RDR_ANY_ROLE,RDR_PREP_BARE_JID,RDR_STREAM_JID);

		Menu *menu;
		if (indexKind == RIK_STREAM_ROOT)
			menu = createContextMenu(rolesMap.value(RDR_STREAM_JID),rolesMap.value(RDR_ANY_ROLE),AMenu);
		else
			menu = createContextMenu(rolesMap.value(RDR_STREAM_JID),rolesMap.value(RDR_PREP_BARE_JID),AMenu);

		if (!menu->isEmpty())
			AMenu->addAction(menu->menuAction(),AG_RVCM_ARCHIVER_OPTIONS,true);
		else
			delete menu;
	}
}

void MessageArchiver::onMultiUserContextMenu(IMultiUserChatWindow *AWindow, IMultiUser *AUser, Menu *AMenu)
{
	Menu *menu = createContextMenu(QStringList()<<AWindow->streamJid().pFull(),QStringList()<<AUser->userJid().pFull(),AMenu);
	if (!menu->isEmpty())
		AMenu->addAction(menu->menuAction(),AG_MUCM_ARCHIVER_OPTIONS,true);
	else
		delete menu;
}

void MessageArchiver::onSetItemPrefsByAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		QMap<Jid, IArchiveStreamPrefs> streamPrefs;
		QStringList streams = action->data(ADR_STREAM_JID).toStringList();
		QStringList contacts = action->data(ADR_CONTACT_JID).toStringList();
		for (int i=0; i<streams.count(); i++)
		{
			Jid streamJid = streams.at(i);
			Jid contactJid = contacts.at(i);

			if (!streamPrefs.contains(streamJid))
				streamPrefs[streamJid] = archivePrefs(streamJid);
			IArchiveStreamPrefs &prefs = streamPrefs[streamJid];

			QString itemSave = action->data(ADR_ITEM_SAVE).toString();
			if (!itemSave.isEmpty())
			{
				if (streamJid.pBare() != contactJid.pBare())
				{
					prefs.itemPrefs[contactJid] = archiveItemPrefs(streamJid,contactJid);
					prefs.itemPrefs[contactJid].save = itemSave;
				}
				else
				{
					prefs.defaultPrefs.save = itemSave;
				}
			}
		}

		for (QMap<Jid, IArchiveStreamPrefs>::const_iterator it=streamPrefs.constBegin(); it!=streamPrefs.constEnd(); ++it)
			setArchivePrefs(it.key(),it.value());
	}
}

void MessageArchiver::onRemoveItemPrefsByAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		QMap<Jid, IArchiveStreamPrefs> streamPrefs;
		QStringList streams = action->data(ADR_STREAM_JID).toStringList();
		QStringList contacts = action->data(ADR_CONTACT_JID).toStringList();
		for (int i=0; i<streams.count(); i++)
		{
			if (!streamPrefs.contains(streams.at(i)))
				streamPrefs[streams.at(i)] = archivePrefs(streams.at(i));
			streamPrefs[streams.at(i)].itemPrefs[contacts.at(i)].save = QString();
		}

		for (QMap<Jid, IArchiveStreamPrefs>::const_iterator it=streamPrefs.constBegin(); it!=streamPrefs.constEnd(); ++it)
			setArchivePrefs(it.key(),it.value());
	}
}

void MessageArchiver::onShowArchiveWindowByAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		QMultiMap<Jid,Jid> addresses;
		QStringList streams = action->data(ADR_STREAM_JID).toStringList();
		QStringList contacts = action->data(ADR_CONTACT_JID).toStringList();
		for(int i=0; i<streams.count() && i<contacts.count(); i++)
			addresses.insert(streams.at(i),contacts.at(i));
		showArchiveWindow(addresses);
	}
}

void MessageArchiver::onShowArchiveWindowByToolBarAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action)
	{
		IMessageToolBarWidget *toolBarWidget = qobject_cast<IMessageToolBarWidget *>(action->parent());
		if (toolBarWidget)
			showArchiveWindow(toolBarWidget->messageWindow()->address()->availAddresses(true));
	}
}

void MessageArchiver::onShowHistoryOptionsDialogByAction(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (FOptionsManager && FAccountManager && action)
	{
		Jid streamJid = action->data(ADR_STREAM_JID).toString();
		IAccount *account = FAccountManager->findAccountByStream(streamJid);
		if (account)
		{
			QString rootId = OPN_ACCOUNTS"."+account->accountId().toString();
			QString nodeId = QString(OPN_ACCOUNTS_HISTORY).replace("[id]",account->accountId().toString());
			FOptionsManager->showOptionsDialog(nodeId, rootId);
		}
	}
}

void MessageArchiver::onToolBarWidgetCreated(IMessageToolBarWidget *AWidget)
{
	Action *action = new Action(AWidget->toolBarChanger()->toolBar());
	action->setText(tr("View History"));
	action->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY);
	action->setShortcutId(SCT_MESSAGEWINDOWS_SHOWHISTORY);
	connect(action,SIGNAL(triggered(bool)),SLOT(onShowArchiveWindowByToolBarAction(bool)));
	QToolButton *historyButton = AWidget->toolBarChanger()->insertAction(action,TBG_MWTBW_ARCHIVE_VIEW);

	ChatWindowMenu *historyMenu = new ChatWindowMenu(this,AWidget,AWidget->toolBarChanger()->toolBar());
	historyButton->setMenu(historyMenu);
	historyButton->setPopupMode(QToolButton::MenuButtonPopup);
}

void MessageArchiver::onOptionsChanged(const OptionsNode &ANode)
{
	if (ANode.cleanPath() == OPV_HISTORY_ENGINE_ENABLED)
	{
		QUuid id = QUuid::fromString(ANode.parent().nspace());
		emit archiveEngineEnableChanged(id,ANode.value().toBool());
		emit totalCapabilitiesChanged(Jid::null);
	}
}
