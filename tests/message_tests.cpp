#include <QtTest>
#include <QTextBlock>
#include <definitions/namespaces.h>
#include <interfaces/imessagewidgets.h>
#include <plugins/simplemessagestyle/simplemessagestyle.h>
#include <utils/message.h>

// SimpleMessageStyle connects to this signal of the style engine
class TestStyleEngine : public QObject
{
	Q_OBJECT
signals:
	void styleWidgetAdded(IMessageStyle *AStyle, QWidget *AWidget);
};

class MessageTests : public QObject
{
	Q_OBJECT
private:
	TestStyleEngine *FEngine;
	SimpleMessageStyle *FStyle;
	QWidget *FView;
	QString text() const
	{
		// toPlainText() shows non-breaking spaces as spaces and line breaks as new lines
		return qobject_cast<QTextBrowser *>(FView)->document()->toPlainText();
	}
	bool append(const QString &AHtml, const QString &AContentId, int AKind = IMessageStyleContentOptions::KindMessage)
	{
		IMessageStyleContentOptions options;
		options.kind = AKind;
		options.direction = IMessageStyleContentOptions::DirectionOut;
		options.time = QDateTime::currentDateTime();
		options.senderId = "me";
		options.senderName = "Me";
		options.contentId = AContentId;
		return FStyle->appendContent(FView,AHtml,options);
	}
private slots:
	void init()
	{
		FEngine = new TestStyleEngine;
		FStyle = new SimpleMessageStyle(QStringLiteral(TEST_STYLE_PATH),NULL,FEngine);
		QVERIFY(FStyle->isValid());
		IMessageStyleOptions options;
		options.styleId = FStyle->styleId();
		FView = FStyle->createWidget(options,NULL);
		QVERIFY(FView != NULL);
	}
	void cleanup()
	{
		delete FView;
		delete FEngine;
	}

	void originAndStanzaIds()
	{
		Message message;
		QVERIFY(message.originId().isEmpty());
		message.setOriginId("origin-1");
		QCOMPARE(message.originId(),QString("origin-1"));
		QCOMPARE(message.stanza().firstElement("origin-id",NS_STANZA_ID).attribute("id"),QString("origin-1"));
		message.setOriginId(QString());
		QVERIFY(message.stanza().firstElement("origin-id",NS_STANZA_ID).isNull());

		message.stanza().addElement("stanza-id",NS_STANZA_ID).setAttribute("by","evil@example.org");
		message.stanza().firstElement("stanza-id",NS_STANZA_ID).setAttribute("id","forged");
		QDomElement trusted = message.stanza().addElement("stanza-id",NS_STANZA_ID);
		trusted.setAttribute("by","user@example.org");
		trusted.setAttribute("id","archive-1");
		QCOMPARE(message.stanzaId(Jid("user@example.org")),QString("archive-1"));
		QVERIFY(message.stanzaId(Jid("other@example.org")).isEmpty());

		message.setRandomId();
		QVERIFY(!message.id().startsWith('{'));
	}

	void contentIds()
	{
		Message archived;
		archived.setOriginId("abc");
		QCOMPARE(messageViewMessageId(archived),QString("abc"));
		archived.setId("xyz");
		QCOMPARE(messageViewMessageId(archived),QString("xyz"));
		QVERIFY(messageViewContentId(true,QString()).isEmpty());
		QVERIFY(messageViewContentId(true,"1") != messageViewContentId(false,"1"));
		QVERIFY(messageViewContentId(false,"1","alice") != messageViewContentId(false,"1","bob"));
	}

	void replaceKeepsOtherMessages()
	{
		QVERIFY(append("first message","out::1"));
		QVERIFY(append("second <b>message</b>","out::2"));
		QVERIFY(FStyle->hasContent(FView,"out::1"));
		QVERIFY(FStyle->hasContent(FView,"out::2"));
		QVERIFY(!FStyle->hasContent(FView,"out::3"));

		QVERIFY(FStyle->replaceContent(FView,"out::1","corrected<br>text"));
		QString plain = text();
		QVERIFY(!plain.contains("first message"));
		QVERIFY(plain.contains(QString("corrected\ntext")));
		QVERIFY(plain.contains("second message"));
		QVERIFY(plain.indexOf("corrected") < plain.indexOf("second message"));

		// Corrections may follow each other
		QVERIFY(FStyle->replaceContent(FView,"out::1","third version"));
		QVERIFY(!text().contains("corrected"));
		QVERIFY(text().contains("third version"));
		QVERIFY(FStyle->replaceContent(FView,"out::2","last one"));
		QVERIFY(text().contains("last one"));
		QVERIFY(!text().contains("second message"));
		QVERIFY(text().contains("third version"));
	}

	void labels()
	{
		QVERIFY(append("hello","out::1"));
		QVERIFY(append("world","out::2"));

		QVERIFY(FStyle->setContentLabel(FView,"out::1",500,"<span title='Delivered'>D1</span>"));
		QVERIFY(text().contains(QString("hello D1")));
		QVERIFY(FStyle->setContentLabel(FView,"out::1",500,"<span title='Read'>R1</span>"));
		QVERIFY(!text().contains("D1"));
		QVERIFY(text().contains("R1"));

		// Labels are ordered by id and kept when the message text changes
		QVERIFY(FStyle->setContentLabel(FView,"out::1",100,"E1"));
		QVERIFY(text().indexOf("E1") < text().indexOf("R1"));
		QVERIFY(FStyle->replaceContent(FView,"out::1","bye"));
		QVERIFY(text().contains(QString("bye E1 R1")));

		QVERIFY(FStyle->setContentLabel(FView,"out::1",500,QString()));
		QVERIFY(!text().contains("R1"));
		QVERIFY(text().contains("E1"));

		// Labels of the last message must not leak into the next messages
		QVERIFY(FStyle->setContentLabel(FView,"out::2",500,"R2"));
		QVERIFY(append("again","out::3"));
		QVERIFY(FStyle->setContentLabel(FView,"out::2",500,"X2"));
		QVERIFY(FStyle->replaceContent(FView,"out::3","changed"));
		QString plain = text();
		QVERIFY(plain.contains("world"));
		QVERIFY(plain.contains("X2"));
		QVERIFY(!plain.contains("R2"));
		QVERIFY(plain.contains("changed"));
		QVERIFY(!plain.contains("again"));
		QCOMPARE(plain.count("X2"),1);
	}

	void tooltips()
	{
		QVERIFY(append("text","out::1"));
		QVERIFY(FStyle->setContentLabel(FView,"out::1",500,"<span title='Read'>RR</span>"));
		QTextDocument *doc = qobject_cast<QTextBrowser *>(FView)->document();
		QTextCursor cursor = doc->find("RR");
		QVERIFY(!cursor.isNull());
		QCOMPARE(cursor.charFormat().toolTip(),QString("Read"));
	}

	void markersInMessageText()
	{
		// Markers coming from the message itself must not break the content
		QString html = QString("a%1b&#xFDDB;c&#64988;d").arg(QChar(0xFDDA));
		QVERIFY(append(html,"in::1"));
		QVERIFY(text().contains("abcd"));
		QVERIFY(FStyle->setContentLabel(FView,"in::1",100,"L"));
		QVERIFY(FStyle->replaceContent(FView,"in::1",QString("new%1text").arg(QChar(0xFDDC))));
		QVERIFY(text().contains(QString("newtext L")));
	}

	void duplicateIdsUseLastContent()
	{
		QVERIFY(append("history copy","out::1"));
		QVERIFY(append("live copy","out::1"));
		QVERIFY(FStyle->replaceContent(FView,"out::1","fixed"));
		QVERIFY(text().contains("history copy"));
		QVERIFY(!text().contains("live copy"));
		QVERIFY(text().contains("fixed"));
	}

	void statusesAreNotAddressable()
	{
		QVERIFY(append("went away","status::1",IMessageStyleContentOptions::KindStatus));
		QVERIFY(!FStyle->hasContent(FView,"status::1"));
		QVERIFY(!FStyle->replaceContent(FView,"status::1","changed"));
	}

	void clearRemovesContents()
	{
		QVERIFY(append("text","out::1"));
		IMessageStyleOptions options;
		options.styleId = FStyle->styleId();
		QVERIFY(FStyle->changeOptions(FView,options,true));
		QVERIFY(!FStyle->hasContent(FView,"out::1"));
	}
};

QTEST_MAIN(MessageTests)
#include "message_tests.moc"
