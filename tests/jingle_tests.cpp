#include <QtTest>
#include <QPointer>
#include <definitions/namespaces.h>
#include <plugins/jingle/jingle.h>

#define STREAM_JID   "user@example.org/home"
#define CONTACT_JID  "friend@example.org/phone"
#define TEST_APP_NS  "urn:example:jingle:app"

// Keeps outgoing stanzas instead of sending them
class TestStanzaProcessor : public QObject, public IPlugin, public IStanzaProcessor
{
	Q_OBJECT
	Q_INTERFACES(IPlugin IStanzaProcessor)
public:
	struct Error {
		QString condition;
		QString jingleCondition;
	};
	QList<Stanza> sent;
	QMap<QString, Error> errors; // by stanza id
	QList<IStanzaHandle> handles;
	QObject *instance() override { return this; }
	QUuid pluginUuid() const override { return STANZAPROCESSOR_UUID; }
	void pluginInfo(IPluginInfo *) override {}
	bool initConnections(IPluginManager *, int &) override { return true; }
	bool initObjects() override { return true; }
	bool initSettings() override { return true; }
	bool startPlugin() override { return true; }
	bool sendStanzaIn(const Jid &, Stanza &) override { return false; }
	bool sendStanzaOut(const Jid &, Stanza &AStanza) override { sent.append(AStanza); return true; }
	bool sendStanzaRequest(IStanzaRequestOwner *, const Jid &, Stanza &AStanza, int) override { sent.append(AStanza); return true; }
	Stanza makeReplyResult(const Stanza &AStanza) const override
	{
		Stanza result(AStanza.kind());
		result.setType(STANZA_TYPE_RESULT).setTo(AStanza.from()).setId(AStanza.id());
		return result;
	}
	Stanza makeReplyError(const Stanza &AStanza, const XmppStanzaError &AError) const override
	{
		Stanza error(AStanza.kind());
		error.setType(STANZA_TYPE_ERROR).setTo(AStanza.from()).setId(AStanza.id());
		Error &info = const_cast<TestStanzaProcessor *>(this)->errors[AStanza.id()];
		info.condition = AError.condition();
		info.jingleCondition = AError.appCondition(NS_JINGLE_ERRORS);
		return error;
	}
	bool checkStanza(const Stanza &, const QString &) const override { return false; }
	QList<int> stanzaHandles() const override { return QList<int>(); }
	IStanzaHandle stanzaHandle(int AHandleId) const override { return handles.value(AHandleId-1); }
	int insertStanzaHandle(const IStanzaHandle &AHandle) override { handles.append(AHandle); return handles.count(); }
	void removeStanzaHandle(int) override {}
signals:
	void stanzaSent(const Jid &, const Stanza &) override;
	void stanzaReceived(const Jid &, const Stanza &) override;
	void stanzaHandleInserted(int, const IStanzaHandle &) override;
	void stanzaHandleRemoved(int, const IStanzaHandle &) override;
};

class TestPluginManager : public QObject, public IPluginManager
{
	Q_OBJECT
	Q_INTERFACES(IPluginManager)
public:
	QMap<QString, IPlugin *> plugins;
	QObject *instance() override { return this; }
	QString version() const override { return QString(); }
	QString revision() const override { return QString(); }
	QDateTime revisionDate() const override { return QDateTime(); }
	bool isShutingDown() const override { return false; }
	QString homePath() const override { return QString(); }
	void setHomePath(const QString &) override {}
	QString cachePath() const override { return QString(); }
	void setLocale(QLocale::Language, QLocale::Country) override {}
	IPlugin *pluginInstance(const QUuid &) const override { return NULL; }
	QList<IPlugin *> pluginInterface(const QString &AInterface) const override
	{
		return plugins.contains(AInterface) ? QList<IPlugin *>() << plugins.value(AInterface) : QList<IPlugin *>();
	}
	const IPluginInfo *pluginInfo(const QUuid &) const override { return NULL; }
	QSet<QUuid> pluginDependencesOn(const QUuid &) const override { return QSet<QUuid>(); }
	QSet<QUuid> pluginDependencesFor(const QUuid &) const override { return QSet<QUuid>(); }
	void quit() override {}
	void restart() override {}
	void delayShutdown() override {}
	void continueShutdown() override {}
signals:
	void aboutToQuit() override;
	void shutdownStarted() override;
};

class TestXmppStream : public QObject, public IXmppStream
{
	Q_OBJECT
	Q_INTERFACES(IXmppStream)
public:
	QObject *instance() override { return this; }
	bool open() override { return true; }
	void close() override {}
	void abort(const XmppError &) override {}
	bool isOpen() const override { return true; }
	bool isConnected() const override { return true; }
	QString streamId() const override { return QString(); }
	XmppError error() const override { return XmppError(); }
	Jid streamJid() const override { return Jid(STREAM_JID); }
	void setStreamJid(const Jid &) override {}
	bool requestPassword() override { return false; }
	QString password() const override { return QString(); }
	void setPassword(const QString &) override {}
	QString defaultLang() const override { return QString(); }
	void setDefaultLang(const QString &) override {}
	bool isEncryptionRequired() const override { return true; }
	void setEncryptionRequired(bool) override {}
	IConnection *connection() const override { return NULL; }
	void setConnection(IConnection *) override {}
	bool isKeepAliveTimerActive() const override { return false; }
	void setKeepAliveTimerActive(bool) override {}
	qint64 sendStanza(Stanza &) override { return 0; }
	void insertXmppDataHandler(int, IXmppDataHandler *) override {}
	void removeXmppDataHandler(int, IXmppDataHandler *) override {}
	void insertXmppStanzaHandler(int, IXmppStanzaHadler *) override {}
	void removeXmppStanzaHandler(int, IXmppStanzaHadler *) override {}
signals:
	void opened() override;
	void closed() override;
	void aboutToClose() override;
	void error(const XmppError &) override;
	void jidAboutToBeChanged(const Jid &) override;
	void jidChanged(const Jid &) override;
	void passwordRequested(bool &) override;
	void passwordProvided(const QString &) override;
	void connectionChanged(IConnection *) override;
	void dataHandlerInserted(int, IXmppDataHandler *) override;
	void dataHandlerRemoved(int, IXmppDataHandler *) override;
	void stanzaHandlerInserted(int, IXmppStanzaHadler *) override;
	void stanzaHandlerRemoved(int, IXmppStanzaHadler *) override;
	void streamDestroyed() override;
};

// Takes sessions it is offered and answers the requests of the sessions it has taken
class TestApplication : public QObject, public IJingleHandler
{
	Q_OBJECT
	Q_INTERFACES(IJingleHandler)
public:
	IJingle *jingle = NULL;
	bool takeSessions = true;
	bool answerRequests = true;
	QStringList initiated;
	QStringList requests;
	QObject *instance() override { return this; }
	bool jingleSessionInitiate(const Jid &AStreamJid, const Stanza &ARequest) override
	{
		QString sid = ARequest.firstElement("jingle",NS_JINGLE).attribute("sid");
		initiated.append(sid);
		if (!takeSessions)
			return false;
		jingle->insertSession(AStreamJid,sid,this);
		jingle->sendResult(AStreamJid,ARequest);
		return true;
	}
	bool jingleSessionRequest(const Jid &AStreamJid, const Stanza &ARequest) override
	{
		requests.append(ARequest.firstElement("jingle",NS_JINGLE).attribute("action"));
		if (!answerRequests)
			return false;
		jingle->sendResult(AStreamJid,ARequest);
		return true;
	}
};

class JingleTests : public QObject
{
	Q_OBJECT
private:
	TestStanzaProcessor *processor;
	TestPluginManager *manager;
	TestXmppStream *stream;
	Jingle *jingle;
	int handleId;
private:
	Stanza request(const QString &AAction, const QString &ASid, const QString &ADescriptionNS = QString(), const QString &AFrom = CONTACT_JID)
	{
		Stanza iq(STANZA_KIND_IQ);
		iq.setType(STANZA_TYPE_SET).setFrom(AFrom).setTo(STREAM_JID).setUniqueId();
		QDomElement jingleElem = iq.addElement("jingle",NS_JINGLE);
		if (!AAction.isEmpty())
			jingleElem.setAttribute("action",AAction);
		if (!ASid.isEmpty())
			jingleElem.setAttribute("sid",ASid);
		if (!ADescriptionNS.isEmpty())
		{
			QDomElement content = jingleElem.appendChild(iq.createElement("content")).toElement();
			content.setAttribute("creator","initiator");
			content.setAttribute("name","test");
			content.appendChild(iq.createElement("description",ADescriptionNS));
		}
		return iq;
	}
	Stanza receive(const Stanza &ARequest)
	{
		processor->sent.clear();
		Stanza stanza = ARequest;
		bool accept = false;
		bool handled = jingle->stanzaReadWrite(handleId,Jid(STREAM_JID),stanza,accept);
		if (!handled || !accept)
			return Stanza();
		return processor->sent.value(0);
	}
	void verifyError(const Stanza &AReply, const Stanza &ARequest, const QString &ACondition, const QString &AJingleCondition = QString())
	{
		QVERIFY(AReply.isError());
		QCOMPARE(AReply.id(),ARequest.id());
		QCOMPARE(processor->errors.value(ARequest.id()).condition,ACondition);
		QCOMPARE(processor->errors.value(ARequest.id()).jingleCondition,AJingleCondition);
	}
private slots:
	void init()
	{
		processor = new TestStanzaProcessor;
		manager = new TestPluginManager;
		manager->plugins.insert("IStanzaProcessor",processor);
		stream = new TestXmppStream;
		jingle = new Jingle;
		int order = 0;
		jingle->initConnections(manager,order);
		QVERIFY(QMetaObject::invokeMethod(jingle,"onXmppStreamOpened",Q_ARG(IXmppStream *,stream)));
		QCOMPARE(processor->handles.count(),1);
		QCOMPARE(processor->handles.first().conditions,QList<QString>() << "/iq[@type='set']/jingle[@xmlns='" NS_JINGLE "']");
		handleId = 1;
	}
	void cleanup()
	{
		delete jingle;
		delete stream;
		delete manager;
		delete processor;
	}
	void rejectsRequestWithoutSessionId()
	{
		Stanza req = request("session-initiate",QString(),TEST_APP_NS);
		verifyError(receive(req),req,"bad-request");
	}
	void rejectsUnknownSession()
	{
		Stanza req = request("transport-info","missing");
		verifyError(receive(req),req,"item-not-found","unknown-session");
	}
	void rejectsSessionWithoutApplications()
	{
		Stanza req = request("session-initiate","s1",TEST_APP_NS);
		verifyError(receive(req),req,"service-unavailable");
	}
	void terminatesUnsupportedApplication()
	{
		TestApplication app;
		app.jingle = jingle;
		QVERIFY(jingle->insertApplication(TEST_APP_NS,&app));

		Stanza req = request("session-initiate","s1","urn:example:other");
		Stanza reply = receive(req);
		QVERIFY(reply.isResult());
		QCOMPARE(processor->sent.count(),2);
		Stanza terminate = processor->sent.at(1);
		QDomElement jingleElem = terminate.firstElement("jingle",NS_JINGLE);
		QCOMPARE(terminate.toJid().full(),QString(CONTACT_JID));
		QCOMPARE(jingleElem.attribute("action"),QString("session-terminate"));
		QCOMPARE(jingleElem.attribute("sid"),QString("s1"));
		QVERIFY(!jingleElem.firstChildElement("reason").firstChildElement("unsupported-applications").isNull());
		QVERIFY(app.initiated.isEmpty());
	}
	void routesSessionToApplication()
	{
		TestApplication app;
		app.jingle = jingle;
		QVERIFY(jingle->insertApplication(TEST_APP_NS,&app));
		QCOMPARE(jingle->applicationHandler(TEST_APP_NS),static_cast<IJingleHandler *>(&app));

		Stanza init = request("session-initiate","s1",TEST_APP_NS);
		QVERIFY(receive(init).isResult());
		QCOMPARE(app.initiated,QStringList() << "s1");
		QCOMPARE(jingle->sessionHandler(Jid(STREAM_JID),"s1"),static_cast<IJingleHandler *>(&app));

		QVERIFY(receive(request("transport-info","s1")).isResult());
		QCOMPARE(app.requests,QStringList() << "transport-info");

		app.answerRequests = false;
		Stanza info = request("session-info","s1");
		verifyError(receive(info),info,"item-not-found","unknown-session");

		jingle->removeSession(Jid(STREAM_JID),"s1");
		Stanza after = request("session-terminate","s1");
		verifyError(receive(after),after,"item-not-found","unknown-session");
	}
	void repliesWhenApplicationDeclines()
	{
		TestApplication app;
		app.jingle = jingle;
		app.takeSessions = false;
		QVERIFY(jingle->insertApplication(TEST_APP_NS,&app));

		Stanza req = request("session-initiate","s1",TEST_APP_NS);
		verifyError(receive(req),req,"service-unavailable");
		QVERIFY(jingle->sessionHandler(Jid(STREAM_JID),"s1") == NULL);
	}
	void rejectsDuplicateRegistrations()
	{
		TestApplication first, second;
		QVERIFY(jingle->insertApplication(TEST_APP_NS,&first));
		QVERIFY(!jingle->insertApplication(TEST_APP_NS,&second));
		QVERIFY(jingle->insertSession(Jid(STREAM_JID),"s1",&first));
		QVERIFY(!jingle->insertSession(Jid(STREAM_JID),"s1",&second));
		QVERIFY(!jingle->insertSession(Jid(STREAM_JID),QString(),&first));
		QVERIFY(jingle->newSessionId() != jingle->newSessionId());
	}
	void forgetsSessionsOfClosedStream()
	{
		TestApplication app;
		QVERIFY(jingle->insertSession(Jid(STREAM_JID),"s1",&app));
		QVERIFY(QMetaObject::invokeMethod(jingle,"onXmppStreamClosed",Q_ARG(IXmppStream *,stream)));
		QVERIFY(jingle->sessionHandler(Jid(STREAM_JID),"s1") == NULL);
	}
	void forgetsDestroyedHandler()
	{
		TestApplication *app = new TestApplication;
		QVERIFY(jingle->insertApplication(TEST_APP_NS,app));
		QVERIFY(jingle->insertSession(Jid(STREAM_JID),"s1",app));
		delete app;
		QVERIFY(jingle->applications().isEmpty());
		QVERIFY(jingle->sessionHandler(Jid(STREAM_JID),"s1") == NULL);
	}
	void buildsRequests()
	{
		QDomElement jingleElem;
		Stanza req = jingle->makeRequest(Jid(CONTACT_JID),"s1","session-accept",jingleElem);
		QCOMPARE(req.kind(),QString(STANZA_KIND_IQ));
		QCOMPARE(req.type(),QString(STANZA_TYPE_SET));
		QCOMPARE(req.toJid().full(),QString(CONTACT_JID));
		QVERIFY(!req.id().isEmpty());
		QCOMPARE(jingleElem.namespaceURI(),QString(NS_JINGLE));
		QCOMPARE(jingleElem.attribute("action"),QString("session-accept"));
		QCOMPARE(jingleElem.attribute("sid"),QString("s1"));
	}
};

QTEST_GUILESS_MAIN(JingleTests)
#include "jingle_tests.moc"
