#include <QtTest>
#include <definitions/namespaces.h>
#include <definitions/optionvalues.h>
#include <plugins/messagereceipts/messagereceipts.h>
#include <utils/options.h>

#define STREAM_JID   "user@example.org/home"
#define CONTACT_JID  "friend@example.org/phone"
#define STRANGER_JID "stranger@example.org/pc"

class TestStanzaProcessor : public QObject, public IPlugin, public IStanzaProcessor
{
	Q_OBJECT
	Q_INTERFACES(IPlugin IStanzaProcessor)
public:
	QList<Stanza> sent;
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
	Stanza makeReplyResult(const Stanza &AStanza) const override { return AStanza; }
	Stanza makeReplyError(const Stanza &AStanza, const XmppStanzaError &) const override { return AStanza; }
	bool checkStanza(const Stanza &, const QString &) const override { return false; }
	QList<int> stanzaHandles() const override { return QList<int>(); }
	IStanzaHandle stanzaHandle(int) const override { return IStanzaHandle(); }
	int insertStanzaHandle(const IStanzaHandle &) override { return 1; }
	void removeStanzaHandle(int) override {}
signals:
	void stanzaSent(const Jid &, const Stanza &) override;
	void stanzaReceived(const Jid &, const Stanza &) override;
	void stanzaHandleInserted(int, const IStanzaHandle &) override;
	void stanzaHandleRemoved(int, const IStanzaHandle &) override;
};

class TestMessageProcessor : public QObject, public IPlugin, public IMessageProcessor
{
	Q_OBJECT
	Q_INTERFACES(IPlugin IMessageProcessor)
public:
	QMultiMap<int, IMessageEditor *> editors;
	QObject *instance() override { return this; }
	QUuid pluginUuid() const override { return MESSAGEPROCESSOR_UUID; }
	void pluginInfo(IPluginInfo *) override {}
	bool initConnections(IPluginManager *, int &) override { return true; }
	bool initObjects() override { return true; }
	bool initSettings() override { return true; }
	bool startPlugin() override { return true; }
	QList<Jid> activeStreams() const override { return QList<Jid>(); }
	bool isActiveStream(const Jid &) const override { return true; }
	void appendActiveStream(const Jid &) override {}
	void removeActiveStream(const Jid &) override {}
	bool sendMessage(const Jid &, Message &, int) override { return false; }
	bool processMessage(const Jid &AStreamJid, Message &AMessage, int ADirection) override
	{
		if (ADirection == DirectionIn)
			AMessage.setTo(AStreamJid.full());
		else
			AMessage.setFrom(AStreamJid.full());
		for (QMultiMap<int, IMessageEditor *>::const_iterator it=editors.constBegin(); it!=editors.constEnd(); ++it)
			if (it.value()->messageReadWrite(it.key(),AStreamJid,AMessage,ADirection))
				return false;
		return true;
	}
	bool displayMessage(const Jid &, Message &, int) override { return false; }
	QList<int> notifiedMessages() const override { return QList<int>(); }
	Message notifiedMessage(int) const override { return Message(); }
	int notifyByMessage(int) const override { return -1; }
	int messageByNotify(int) const override { return -1; }
	void showNotifiedMessage(int) override {}
	void removeMessageNotify(int) override {}
	bool messageHasText(const Message &AMessage, const QString &) const override { return !AMessage.body().isEmpty(); }
	bool messageToText(const Message &, QTextDocument *, const QString &) const override { return false; }
	bool textToMessage(const QTextDocument *, Message &, const QString &) const override { return false; }
	IMessageWindow *getMessageWindow(const Jid &, const Jid &, Message::MessageType, int) const override { return NULL; }
	QMultiMap<int, IMessageHandler *> messageHandlers() const override { return QMultiMap<int, IMessageHandler *>(); }
	void insertMessageHandler(int, IMessageHandler *) override {}
	void removeMessageHandler(int, IMessageHandler *) override {}
	QMultiMap<int, IMessageWriter *> messageWriters() const override { return QMultiMap<int, IMessageWriter *>(); }
	void insertMessageWriter(int, IMessageWriter *) override {}
	void removeMessageWriter(int, IMessageWriter *) override {}
	QMultiMap<int, IMessageEditor *> messageEditors() const override { return editors; }
	void insertMessageEditor(int AOrder, IMessageEditor *AEditor) override { editors.insert(AOrder,AEditor); }
	void removeMessageEditor(int AOrder, IMessageEditor *AEditor) override { editors.remove(AOrder,AEditor); }
signals:
	void messageSent(const Message &AMessage) override;
	void messageReceived(const Message &AMessage) override;
	void messageNotifyInserted(int AMessageId) override;
	void messageNotifyRemoved(int AMessageid) override;
	void activeStreamAppended(const Jid &AStreamJid) override;
	void activeStreamRemoved(const Jid &AStreamJid) override;
};

class TestRoster : public QObject, public IRoster
{
	Q_OBJECT
	Q_INTERFACES(IRoster)
public:
	QMap<Jid, IRosterItem> itemsMap;
	QObject *instance() override { return this; }
	Jid streamJid() const override { return Jid(STREAM_JID); }
	IXmppStream *xmppStream() const override { return NULL; }
	bool isOpen() const override { return true; }
	QString groupDelimiter() const override { return "::"; }
	QList<Jid> itemsJid() const override { return itemsMap.keys(); }
	QList<IRosterItem> items() const override { return itemsMap.values(); }
	bool hasItem(const Jid &AItemJid) const override { return itemsMap.contains(AItemJid.bare()); }
	IRosterItem findItem(const Jid &AItemJid) const override { return itemsMap.value(AItemJid.bare()); }
	QSet<QString> groups() const override { return QSet<QString>(); }
	bool hasGroup(const QString &) const override { return false; }
	QSet<QString> itemGroups(const Jid &) const override { return QSet<QString>(); }
	QList<IRosterItem> groupItems(const QString &) const override { return QList<IRosterItem>(); }
	bool isSubgroup(const QString &, const QString &) const override { return false; }
	void setItem(const Jid &, const QString &, const QSet<QString> &) override {}
	void setItems(const QList<IRosterItem> &) override {}
	void removeItem(const Jid &) override {}
	void removeItems(const QList<IRosterItem> &) override {}
	void saveRosterItems(const QString &) const override {}
	void loadRosterItems(const QString &) override {}
	QSet<Jid> subscriptionRequests() const override { return QSet<Jid>(); }
	void sendSubscription(const Jid &, int, const QString &) override {}
	void renameItem(const Jid &, const QString &) override {}
	void copyItemToGroup(const Jid &, const QString &) override {}
	void moveItemToGroup(const Jid &, const QString &, const QString &) override {}
	void removeItemFromGroup(const Jid &, const QString &) override {}
	void renameGroup(const QString &, const QString &) override {}
	void copyGroupToGroup(const QString &, const QString &) override {}
	void moveGroupToGroup(const QString &, const QString &) override {}
	void removeGroup(const QString &) override {}
signals:
	void opened() override;
	void closed() override;
	void itemReceived(const IRosterItem &, const IRosterItem &) override;
	void subscriptionSent(const Jid &, int, const QString &) override;
	void subscriptionReceived(const Jid &, int, const QString &) override;
	void streamJidAboutToBeChanged(const Jid &) override;
	void streamJidChanged(const Jid &) override;
	void rosterDestroyed() override;
};

class TestRosterManager : public QObject, public IPlugin, public IRosterManager
{
	Q_OBJECT
	Q_INTERFACES(IPlugin IRosterManager)
public:
	TestRoster roster;
	QObject *instance() override { return this; }
	QUuid pluginUuid() const override { return QUuid(); }
	void pluginInfo(IPluginInfo *) override {}
	bool initConnections(IPluginManager *, int &) override { return true; }
	bool initObjects() override { return true; }
	bool initSettings() override { return true; }
	bool startPlugin() override { return true; }
	QList<IRoster *> rosters() const override { return QList<IRoster *>() << const_cast<TestRoster *>(&roster); }
	IRoster *findRoster(const Jid &AStreamJid) const override { return AStreamJid==Jid(STREAM_JID) ? const_cast<TestRoster *>(&roster) : NULL; }
	IRoster *createRoster(IXmppStream *) override { return NULL; }
	void destroyRoster(IRoster *) override {}
	bool isRosterActive(IRoster *) const override { return true; }
	QString rosterFileName(const Jid &) const override { return QString(); }
signals:
	void rosterCreated(IRoster *) override;
	void rosterOpened(IRoster *) override;
	void rosterClosed(IRoster *) override;
	void rosterItemReceived(IRoster *, const IRosterItem &, const IRosterItem &) override;
	void rosterSubscriptionSent(IRoster *, const Jid &, int, const QString &) override;
	void rosterSubscriptionReceived(IRoster *, const Jid &, int, const QString &) override;
	void rosterStreamJidAboutToBeChanged(IRoster *, const Jid &) override;
	void rosterStreamJidChanged(IRoster *, const Jid &) override;
	void rosterActiveChanged(IRoster *, bool) override;
	void rosterDestroyed(IRoster *) override;
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

class ReceiptsTests : public QObject
{
	Q_OBJECT
private:
	TestStanzaProcessor *stanzas;
	TestMessageProcessor *messages;
	TestRosterManager *rosters;
	TestPluginManager *manager;
	MessageReceipts *receipts;
	Message send(const QString &AId, const QString &ABody, const QString &AReplaceId = QString(), const QString &ATo = CONTACT_JID)
	{
		Message message;
		message.setType(Message::Chat).setTo(ATo).setId(AId).setBody(ABody);
		if (!AReplaceId.isEmpty())
			message.stanza().addElement("replace",NS_MESSAGE_CORRECT).setAttribute("id",AReplaceId);
		messages->processMessage(Jid(STREAM_JID),message,IMessageProcessor::DirectionOut);
		return message;
	}
	void receive(Message &AMessage, const QString &AFrom = CONTACT_JID)
	{
		AMessage.setFrom(AFrom);
		if (AMessage.stanza().type().isEmpty())
			AMessage.setType(Message::Chat);
		messages->processMessage(Jid(STREAM_JID),AMessage,IMessageProcessor::DirectionIn);
	}
	void receiveMarker(const QString &ATag, const QString &ANamespace, const QString &AId, const QString &AFrom = CONTACT_JID)
	{
		Message message;
		message.stanza().addElement(ATag,ANamespace).setAttribute("id",AId);
		receive(message,AFrom);
	}
	int state(const QString &AId) const
	{
		return receipts->messageState(Jid(STREAM_JID),Jid(CONTACT_JID),AId);
	}
	void setSubscription(const QString &ASubscription)
	{
		IRosterItem item;
		item.itemJid = Jid(CONTACT_JID).bare();
		item.subscription = ASubscription;
		rosters->roster.itemsMap.insert(item.itemJid,item);
	}
private slots:
	void initTestCase()
	{
		QDomDocument options;
		options.appendChild(options.createElement("options"));
		Options::setOptions(options,QString(),QByteArray());
	}
	void init()
	{
		stanzas = new TestStanzaProcessor;
		messages = new TestMessageProcessor;
		rosters = new TestRosterManager;
		manager = new TestPluginManager;
		manager->plugins.insert("IStanzaProcessor",stanzas);
		manager->plugins.insert("IMessageProcessor",messages);
		manager->plugins.insert("IRosterManager",rosters);
		receipts = new MessageReceipts;
		int order = 0;
		QVERIFY(receipts->initConnections(manager,order));
		QVERIFY(receipts->initObjects());
		QVERIFY(receipts->initSettings());
		setSubscription(SUBSCRIPTION_BOTH);
	}
	void cleanup()
	{
		delete receipts;
		delete manager;
		delete rosters;
		delete messages;
		delete stanzas;
	}

	void requestsReceiptsAndMarkers()
	{
		Message message = send("m1","hello");
		QVERIFY(!message.stanza().firstElement("request",NS_RECEIPTS).isNull());
		QVERIFY(!message.stanza().firstElement("markable",NS_CHAT_MARKERS).isNull());

		// Copies of messages sent by other clients are not changed
		Message carbon;
		carbon.setType(Message::Chat).setTo(CONTACT_JID).setId("c1").setBody("from phone");
		carbon.stanza().addElement("sent",NS_MESSAGE_CARBONS);
		messages->processMessage(Jid(STREAM_JID),carbon,IMessageProcessor::DirectionOut);
		QVERIFY(carbon.stanza().firstElement("request",NS_RECEIPTS).isNull());

		// But their receipts are shown
		receiveMarker("received",NS_RECEIPTS,"c1");
		QCOMPARE(state("c1"),(int)IMessageReceipts::StateDelivered);
	}

	void receiptMarksOnlyOneMessage()
	{
		send("m1","one");
		send("m2","two");
		receiveMarker("received",NS_RECEIPTS,"m2");
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateUnknown);
		QCOMPARE(state("m2"),(int)IMessageReceipts::StateDelivered);
	}

	void displayedMarkerMarksPreviousMessages()
	{
		send("m1","one");
		send("m2","two");
		send("m3","three");
		receiveMarker("displayed",NS_CHAT_MARKERS,"m2");
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateDisplayed);
		QCOMPARE(state("m2"),(int)IMessageReceipts::StateDisplayed);
		QCOMPARE(state("m3"),(int)IMessageReceipts::StateUnknown);

		// A late receipt does not downgrade the state
		receiveMarker("received",NS_RECEIPTS,"m1");
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateDisplayed);
	}

	void ignoresIdsOfOtherConversations()
	{
		send("m1","one");
		receiveMarker("displayed",NS_CHAT_MARKERS,"m1",STRANGER_JID);
		receiveMarker("received",NS_RECEIPTS,"m1",STRANGER_JID);
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateUnknown);
		receiveMarker("displayed",NS_CHAT_MARKERS,"unknown");
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateUnknown);
	}

	void correctionReceiptsApplyToOriginal()
	{
		send("m1","helo");
		send("m2","hello","m1");
		receiveMarker("received",NS_RECEIPTS,"m2");
		QCOMPARE(state("m1"),(int)IMessageReceipts::StateDelivered);
		QCOMPARE(state("m2"),(int)IMessageReceipts::StateUnknown);
	}

	void sendsReceiptsToAuthorizedContacts()
	{
		Message message;
		message.setId("in1").setBody("hi");
		message.stanza().addElement("request",NS_RECEIPTS);
		receive(message);
		QCOMPARE(stanzas->sent.count(),1);
		Stanza receipt = stanzas->sent.first();
		QCOMPARE(receipt.to(),QString(CONTACT_JID));
		QCOMPARE(receipt.firstElement("received",NS_RECEIPTS).attribute("id"),QString("in1"));
		QVERIFY(!receipt.id().isEmpty());

		// Not to contacts which may not see the presence
		stanzas->sent.clear();
		setSubscription(SUBSCRIPTION_TO);
		Message hidden;
		hidden.setId("in2").setBody("hi");
		hidden.stanza().addElement("request",NS_RECEIPTS);
		receive(hidden);
		QVERIFY(stanzas->sent.isEmpty());
	}

	void noReceiptsForCarbonsGroupchatsOrWhenDisabled()
	{
		Message carbon;
		carbon.setId("in1").setBody("hi");
		carbon.stanza().addElement("request",NS_RECEIPTS);
		carbon.stanza().addElement("received",NS_MESSAGE_CARBONS);
		receive(carbon);

		Message groupchat;
		groupchat.setType(Message::GroupChat).setId("in2").setBody("hi");
		groupchat.stanza().addElement("request",NS_RECEIPTS);
		receive(groupchat);

		Options::node(OPV_MESSAGES_SENDRECEIPTS).setValue(false);
		Message disabled;
		disabled.setId("in3").setBody("hi");
		disabled.stanza().addElement("request",NS_RECEIPTS);
		receive(disabled);
		Options::node(OPV_MESSAGES_SENDRECEIPTS).setValue(true);

		QVERIFY(stanzas->sent.isEmpty());
	}

	void archiveDropsRequests()
	{
		Message message = send("m1","hello");
		receipts->archiveMessageEdit(0,Jid(STREAM_JID),message,false);
		QVERIFY(message.stanza().firstElement("request",NS_RECEIPTS).isNull());
		QVERIFY(message.stanza().firstElement("markable",NS_CHAT_MARKERS).isNull());
		QCOMPARE(message.body(),QString("hello"));
	}
};

QTEST_MAIN(ReceiptsTests)
#include "receipts_tests.moc"
