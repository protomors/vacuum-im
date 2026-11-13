#ifndef MAMARCHIVE_H
#define MAMARCHIVE_H

#include <QDate>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ifilemessagearchive.h>
#include <interfaces/imessagearchiver.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/ibookmarks.h>
#include <interfaces/imultiuserchat.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/iaccountmanager.h>

#include <QUuid>
#define MAMARCHIVE_UUID QUuid::fromString(QLatin1String("{A7C31E04-5B18-4F0D-9E6A-2D84B0F73C19}"))

struct MamJob
{
	MamJob() {
		userArchive = false;
	}
	Jid streamJid;
	bool userArchive;
	Jid archiveJid;
};

struct MamQuery
{
	MamQuery() {
		userArchive = false;
		complete = false;
		finReceived = false;
	}
	bool userArchive;
	Jid archiveJid;
	QString queryId;
	QString afterId;
	bool complete;
	bool finReceived;
	QString lastId;
	QSet<QString> resultIds;
	QList<Message> messages;
};

struct MamStoredCollection
{
	MamStoredCollection() {
		open = false;
	}
	IArchiveHeader header;
	QDateTime last;
	bool open;
};

struct MamContactIndex
{
	MamContactIndex() {
		loaded = false;
		openRefreshed = false;
	}
	bool loaded;
	bool openRefreshed;
	QSet<QString> keys;
	QList<MamStoredCollection> collections;
};

struct MamRuntime
{
	enum Phase {
		Offline,
		Checking,
		Unsupported,
		Disabled,
		Syncing,
		Current,
		Failed
	};
	MamRuntime() {
		phase = Offline;
		written = 0;
	}
	Phase phase;
	int written;
	QDateTime finished;
	QString error;
};

struct MamPrefsState
{
	enum Availability {
		Unknown,
		Unavailable,
		Ready,
		Failed
	};
	MamPrefsState() {
		availability = Unknown;
	}
	Availability availability;
	QString policy;
	QStringList always;
	QStringList never;
	QString saveError;
};

class MamArchive :
	public QObject,
	public IPlugin,
	public IStanzaHandler,
	public IStanzaRequestOwner,
	public IOptionsDialogHolder
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IStanzaHandler IStanzaRequestOwner IOptionsDialogHolder);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.MamArchive");
public:
	MamArchive();
	~MamArchive();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return MAMARCHIVE_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin() { return true; }
	//IOptionsDialogHolder
	virtual QMultiMap<int, IOptionsDialogWidget *> optionsDialogWidgets(const QString &ANodeId, QWidget *AParent);
	QString statusText(const QUuid &AAccountId) const;
	void applyAccountOptions(IAccount *AAccount, bool ABoundsChanged);
	bool prefsAvailable(const QUuid &AAccountId) const;
	QString prefsPolicy(const QUuid &AAccountId) const;
	QStringList prefsAlways(const QUuid &AAccountId) const;
	QStringList prefsNever(const QUuid &AAccountId) const;
	QString prefsNote(const QUuid &AAccountId) const;
	QString prefsError(const QUuid &AAccountId) const;
	void setAccountPrefs(IAccount *AAccount, const QString &APolicy, const QStringList &AAlways, const QStringList &ANever);
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
protected:
	QString archiveKey(bool AUserArchive, const Jid &AArchiveJid) const;
	QString cursorFilePath(const Jid &AStreamJid) const;
	QString readCursor(const Jid &AStreamJid, const MamJob &AJob) const;
	void writeCursor(const Jid &AStreamJid, const MamJob &AJob, const QString &ACursor);
	bool isKnownRoom(const Jid &AStreamJid, const Jid &ARoomJid) const;
	void watchRoom(const Jid &AStreamJid, const Jid &ARoomJid);
	void enqueue(const MamJob &AJob);
	void pump(const Jid &AStreamJid);
	bool sendQuery(const MamJob &AJob, const QString &AAfterId);
	bool readArchivedMessage(const QDomElement &AResult, Message &AMessage) const;
	QString messageKey(const Message &AMessage, const Jid &AWith) const;
	QDateTime collectionStart(const QDateTime &ATime) const;
	QString headerKey(const IArchiveHeader &AHeader) const;
	bool headerMatches(const IArchiveHeader &AHeader, const Jid &AWith, bool AGroupChat) const;
	MamContactIndex &contactIndex(const Jid &AStreamJid, const Jid &ABare);
	void loadContactIndex(const Jid &AStreamJid, const Jid &ABare, MamContactIndex &AIndex) const;
	void refreshOpenCollections(const Jid &AStreamJid, const Jid &ABare, MamContactIndex &AIndex) const;
	bool prepareMessage(const Jid &AStreamJid, const MamQuery &AQuery, Message &AMessage, Jid &AWith, bool &AGroupChat) const;
	bool importMessages(const Jid &AStreamJid, const MamQuery &AQuery);
	void finishQuery(const Jid &AStreamJid);
	void clearStream(const Jid &AStreamJid);
	IAccount *accountByStream(const Jid &AStreamJid) const;
	bool downloadEnabled(const Jid &AStreamJid) const;
	bool conferencesEnabled(const Jid &AStreamJid) const;
	QDate startDate(const Jid &AStreamJid) const;
	void setPhase(const QUuid &AAccountId, MamRuntime::Phase APhase, const QString &AError = QString());
	void beginSync(const QUuid &AAccountId, bool AResetCount);
	void stopWork(const Jid &AStreamJid);
	void clearCursors(const Jid &AStreamJid);
	void startJobs(IAccount *AAccount, bool AResetCount);
	void scheduleRooms(const Jid &AStreamJid);
	void requestPrefs(IAccount *AAccount);
	bool sendPrefsRequest(IAccount *AAccount, Stanza &AStanza, bool ASetting, const QString &APolicy, const QStringList &AAlways, const QStringList &ANever);
	bool readPrefsElement(const QDomElement &APrefs, QString &APolicy, QStringList &AAlways, QStringList &ANever) const;
	void handlePrefsResult(const Stanza &AStanza);
	void clearPrefs(IAccount *AAccount);
protected slots:
	void onXmppStreamOpened(IXmppStream *AXmppStream);
	void onXmppStreamClosed(IXmppStream *AXmppStream);
	void onDiscoOpened(const Jid &AStreamJid);
	void onDiscoInfoReceived(const IDiscoInfo &AInfo);
	void onBookmarksOpened(const Jid &AStreamJid);
	void onBookmarksChanged(const Jid &AStreamJid);
	void onMultiUserChatCreated(IMultiUserChat *AMultiChat);
	void onDatabaseOpened(const Jid &AStreamJid);
	void onContinueSync();
	void onFinishQuery();
signals:
	void accountStatusChanged(const QUuid &AAccountId);
	void accountPrefsChanged(const QUuid &AAccountId);
private:
	IXmppStreamManager *FXmppStreamManager;
	IStanzaProcessor *FStanzaProcessor;
	IServiceDiscovery *FDiscovery;
	IMessageArchiver *FArchiver;
	IFileMessageArchive *FFileArchive;
	IBookmarks *FBookmarks;
	IMultiUserChatManager *FMultiChat;
	IOptionsManager *FOptionsManager;
	IAccountManager *FAccountManager;
private:
	QMap<Jid, int> FSHIMam;
	QMap<Jid, QList<MamJob> > FQueues;
	QMap<Jid, MamQuery> FActive;
	QMap<Jid, QSet<QString> > FDone;
	QMap<Jid, QSet<QString> > FFailed;
	QMap<Jid, QSet<QString> > FWatchedRooms;
	QSet<Jid> FFinishPending;
	QMap<Jid, QMap<QString, MamContactIndex> > FContacts;
	QMap<Jid, bool> FServerMam;
	QMap<QUuid, MamRuntime> FState;
	QMap<QUuid, MamPrefsState> FPrefs;
	QHash<QString, QUuid> FPrefsRequests;
	QHash<QString, bool> FPrefsRequestSet;
	QHash<QString, QString> FPrefsRequestPolicy;
	QHash<QString, QStringList> FPrefsRequestAlways;
	QHash<QString, QStringList> FPrefsRequestNever;
};

#endif // MAMARCHIVE_H
