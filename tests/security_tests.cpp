#include <QtTest>
#include <QComboBox>
#include <definitions/optionvalues.h>
#include <plugins/defaultconnection/defaultconnection.h>
#include <plugins/defaultconnection/defaultconnectionengine.h>
#include <plugins/defaultconnection/connectionoptionswidget.h>
#include <plugins/saslauth/saslauthfeature.h>

class TestConnection : public DefaultConnection
{
public:
	TestConnection() : DefaultConnection(nullptr, nullptr) {}
	bool encrypted = true;
	bool isEncrypted() const override { return encrypted; }
};

// Capture SASL requests without opening a network connection.
class TestStream : public QObject, public IXmppStream
{
	Q_OBJECT
	Q_INTERFACES(IXmppStream)
public:
	QList<Stanza> sent;
	int passwordRequests = 0;
	IConnection *transport = nullptr;
	QObject *instance() override { return this; }
	bool open() override { return false; }
	void close() override {}
	void abort(const XmppError &) override {}
	bool isOpen() const override { return false; }
	bool isConnected() const override { return false; }
	QString streamId() const override { return QString(); }
	XmppError error() const override { return XmppError(); }
	Jid streamJid() const override { return Jid("user@example.org"); }
	void setStreamJid(const Jid &) override {}
	bool requestPassword() override { ++passwordRequests; return false; }
	QString password() const override { return "test-password"; }
	void setPassword(const QString &) override {}
	QString defaultLang() const override { return "en"; }
	void setDefaultLang(const QString &) override {}
	bool isEncryptionRequired() const override { return false; }
	void setEncryptionRequired(bool) override {}
	IConnection *connection() const override { return transport; }
	void setConnection(IConnection *) override {}
	bool isKeepAliveTimerActive() const override { return false; }
	void setKeepAliveTimerActive(bool) override {}
	qint64 sendStanza(Stanza &stanza) override { sent.append(stanza); return 1; }
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

class SecurityTests : public QObject
{
	Q_OBJECT
private slots:
	void tlsDefaults()
	{
		DefaultConnection connection(nullptr, nullptr);
		QCOMPARE(connection.protocol(), QSsl::TlsV1_2OrLater);
		DefaultConnectionEngine engine;
		QVERIFY(engine.initSettings());
		QCOMPARE(Options::defaultValue(OPV_ACCOUNT_CONNECTION_SSLPROTOCOL).toInt(), int(QSsl::TlsV1_2OrLater));
	}

	void tlsSettings_data()
	{
		QTest::addColumn<int>("saved");
		QTest::addColumn<int>("effective");
		// Include obsolete versions, unrestricted policies, old Auto, DTLS and invalid values.
		for (int value : {-1, 0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 999})
			QTest::newRow(qPrintable(QString::number(value))) << value << int(QSsl::TlsV1_2OrLater);
		for (auto value : {QSsl::TlsV1_2, QSsl::TlsV1_2OrLater, QSsl::TlsV1_3, QSsl::TlsV1_3OrLater})
			QTest::newRow(qPrintable(QString::number(value))) << int(value) << int(value);
	}

	void tlsSettings()
	{
		QFETCH(int, saved);
		QFETCH(int, effective);
		DefaultConnection connection(nullptr, nullptr);
		connection.setProtocol(static_cast<QSsl::SslProtocol>(saved));
		QCOMPARE(int(connection.protocol()), effective);

		QDomDocument doc;
		auto node = Options::createNodeForElement(doc.createElement("connection"));
		node.setValue(saved, "ssl-protocol");
		DefaultConnectionEngine engine;
		engine.loadConnectionSettings(&connection, node);
		QCOMPARE(int(connection.protocol()), effective);

		ConnectionOptionsWidget widget(nullptr, node);
		auto combo = widget.findChild<QComboBox *>("cmbSslProtocol");
		QVERIFY(combo);
		QCOMPARE(combo->count(), 3);
		const int displayed = saved == QSsl::TlsV1_2 || saved == QSsl::TlsV1_3 ? saved : QSsl::TlsV1_2OrLater;
		QCOMPARE(combo->currentData().toInt(), displayed);
		widget.apply();
		QCOMPARE(node.value("ssl-protocol").toInt(), displayed);
	}

	void saslMechanisms_data()
	{
		QTest::addColumn<QStringList>("offered");
		QTest::addColumn<QString>("selected");
		QTest::addColumn<bool>("encrypted");
		QTest::newRow("obsolete-only") << QStringList{"DIGEST-MD5"} << QString() << true;
		QTest::newRow("unknown-only") << QStringList{"UNKNOWN"} << QString() << true;
		QTest::newRow("plain-fallback") << QStringList{"DIGEST-MD5", "PLAIN"} << QString("PLAIN") << true;
		QTest::newRow("reject-plaintext-fallback") << QStringList{"DIGEST-MD5", "PLAIN"} << QString() << false;
		QTest::newRow("reject-plaintext-password") << QStringList{"PLAIN"} << QString() << false;
		for (const auto &scram : {"SCRAM-SHA-1", "SCRAM-SHA-224", "SCRAM-SHA-256", "SCRAM-SHA-384", "SCRAM-SHA-512"})
			QTest::newRow(scram) << QStringList{"DIGEST-MD5", "PLAIN", scram} << QString(scram) << true;
		QTest::newRow("prefer-strongest") << QStringList{"SCRAM-SHA-1", "PLAIN", "SCRAM-SHA-256", "SCRAM-SHA-512"} << QString("SCRAM-SHA-512") << true;
	}

	void saslMechanisms()
	{
		QFETCH(QStringList, offered);
		QFETCH(QString, selected);
		QFETCH(bool, encrypted);
		TestConnection connection;
		connection.encrypted = encrypted;
		TestStream stream;
		stream.transport = &connection;
		SASLAuthFeature feature(&stream);
		QDomDocument doc;
		auto mechanisms = doc.createElement("mechanisms");
		for (const auto &name : offered)
		{
			auto mechanism = doc.createElement("mechanism");
			mechanism.appendChild(doc.createTextNode(name));
			mechanisms.appendChild(mechanism);
		}
		QCOMPARE(feature.start(mechanisms), !selected.isEmpty());
		QCOMPARE(stream.passwordRequests, selected.isEmpty() ? 0 : 1);
		QCOMPARE(stream.sent.size(), selected.isEmpty() ? 0 : 1);
		if (!selected.isEmpty())
			QCOMPARE(stream.sent.first().attribute("mechanism"), selected);
	}
};

QTEST_MAIN(SecurityTests)
#include "security_tests.moc"
