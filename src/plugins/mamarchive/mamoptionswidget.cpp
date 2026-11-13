#include "mamoptionswidget.h"

#include <QDate>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <utils/jid.h>
#include "mamarchive.h"

MamOptionsWidget::MamOptionsWidget(MamArchive *AArchive, IAccount *AAccount, QWidget *AParent) : QWidget(AParent)
{
	ui.setupUi(this);
	FArchive = AArchive;
	FAccount = AAccount;
	FPrefsEdited = false;

	ui.cmbPolicy->addItem(tr("All messages"),QString("always"));
	ui.cmbPolicy->addItem(tr("Roster contacts"),QString("roster"));
	ui.cmbPolicy->addItem(tr("No messages"),QString("never"));
	ui.lstAlways->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui.lstNever->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui.pbtRemoveAlways->setEnabled(false);
	ui.pbtRemoveNever->setEnabled(false);

	connect(ui.chbDownload,SIGNAL(toggled(bool)),SIGNAL(modified()));
	connect(ui.chbConferences,SIGNAL(toggled(bool)),SIGNAL(modified()));
	connect(ui.chbStart,SIGNAL(toggled(bool)),SIGNAL(modified()));
	connect(ui.dedStart,SIGNAL(dateChanged(QDate)),SIGNAL(modified()));
	connect(ui.chbStart,SIGNAL(toggled(bool)),SLOT(onStartToggled(bool)));
	connect(ui.cmbPolicy,SIGNAL(currentIndexChanged(int)),SLOT(onPolicyChanged()));
	connect(ui.pbtAddAlways,SIGNAL(clicked()),SLOT(onAddAlways()));
	connect(ui.pbtAddNever,SIGNAL(clicked()),SLOT(onAddNever()));
	connect(ui.pbtRemoveAlways,SIGNAL(clicked()),SLOT(onRemoveAlways()));
	connect(ui.pbtRemoveNever,SIGNAL(clicked()),SLOT(onRemoveNever()));
	connect(ui.ledAlways,SIGNAL(returnPressed()),SLOT(onAddAlways()));
	connect(ui.ledNever,SIGNAL(returnPressed()),SLOT(onAddNever()));
	connect(ui.lstAlways,SIGNAL(itemSelectionChanged()),SLOT(onPrefsSelectionChanged()));
	connect(ui.lstNever,SIGNAL(itemSelectionChanged()),SLOT(onPrefsSelectionChanged()));
	connect(FArchive->instance(),SIGNAL(accountStatusChanged(const QUuid &)),SLOT(onAccountStatusChanged(const QUuid &)));
	connect(FArchive->instance(),SIGNAL(accountPrefsChanged(const QUuid &)),SLOT(onAccountPrefsChanged(const QUuid &)));

	reset();
}

MamOptionsWidget::~MamOptionsWidget()
{

}

void MamOptionsWidget::apply()
{
	OptionsNode node = FAccount->optionsNode();
	bool oldDownload = node.value("mam-download").toBool();
	bool oldConferences = node.value("mam-conferences").toBool();
	QString oldStart = node.value("mam-start").toString();

	bool download = ui.chbDownload->isChecked();
	bool conferences = ui.chbConferences->isChecked();
	QString start = ui.chbStart->isChecked() ? ui.dedStart->date().toString(Qt::ISODate) : QString();

	node.setValue(download,"mam-download");
	node.setValue(conferences,"mam-conferences");
	node.setValue(start,"mam-start");

	if (download!=oldDownload || conferences!=oldConferences || start!=oldStart)
		FArchive->applyAccountOptions(FAccount,conferences!=oldConferences || start!=oldStart);

	if (ui.grbPrefs->isVisible())
	{
		QString policy = ui.cmbPolicy->currentData().toString();
		QStringList always = listValues(ui.lstAlways);
		QStringList never = listValues(ui.lstNever);
		if (policy!=FArchive->prefsPolicy(FAccount->accountId()) || always!=FArchive->prefsAlways(FAccount->accountId()) || never!=FArchive->prefsNever(FAccount->accountId()))
		{
			FPrefsEdited = false;
			FArchive->setAccountPrefs(FAccount,policy,always,never);
		}
	}

	emit childApply();
}

void MamOptionsWidget::reset()
{
	OptionsNode node = FAccount->optionsNode();
	QString start = node.value("mam-start").toString();
	QDate date = QDate::fromString(start,Qt::ISODate);

	const QSignalBlocker downloadBlocker(ui.chbDownload);
	const QSignalBlocker conferencesBlocker(ui.chbConferences);
	const QSignalBlocker startBlocker(ui.chbStart);
	const QSignalBlocker dateBlocker(ui.dedStart);

	ui.chbDownload->setChecked(node.value("mam-download").toBool());
	ui.chbConferences->setChecked(node.value("mam-conferences").toBool());
	ui.chbStart->setChecked(date.isValid());
	ui.dedStart->setDate(date.isValid() ? date : QDate::currentDate());
	ui.dedStart->setEnabled(ui.chbStart->isChecked());
	FPrefsEdited = false;
	updateStatus();
	updatePrefs();
	emit childReset();
}

void MamOptionsWidget::onAccountStatusChanged(const QUuid &AAccountId)
{
	if (AAccountId == FAccount->accountId())
		updateStatus();
}

void MamOptionsWidget::onAccountPrefsChanged(const QUuid &AAccountId)
{
	if (AAccountId == FAccount->accountId())
		updatePrefs();
}

void MamOptionsWidget::onStartToggled(bool AChecked)
{
	ui.dedStart->setEnabled(AChecked);
}

void MamOptionsWidget::onAddAlways()
{
	addJid(ui.lstAlways,ui.ledAlways,ui.lstNever);
}

void MamOptionsWidget::onAddNever()
{
	addJid(ui.lstNever,ui.ledNever,ui.lstAlways);
}

void MamOptionsWidget::onRemoveAlways()
{
	removeSelected(ui.lstAlways);
}

void MamOptionsWidget::onRemoveNever()
{
	removeSelected(ui.lstNever);
}

void MamOptionsWidget::onPolicyChanged()
{
	FPrefsEdited = true;
	emit modified();
}

void MamOptionsWidget::onPrefsSelectionChanged()
{
	ui.pbtRemoveAlways->setEnabled(!ui.lstAlways->selectedItems().isEmpty());
	ui.pbtRemoveNever->setEnabled(!ui.lstNever->selectedItems().isEmpty());
}

void MamOptionsWidget::updateStatus()
{
	ui.lblStatus->setText(FArchive->statusText(FAccount->accountId()));
}

void MamOptionsWidget::updatePrefs()
{
	bool available = FArchive->prefsAvailable(FAccount->accountId());
	QString note = FArchive->prefsNote(FAccount->accountId());
	ui.lblPrefsNote->setVisible(!note.isEmpty());
	ui.lblPrefsNote->setText(note);

	bool wasVisible = ui.grbPrefs->isVisible();
	ui.grbPrefs->setVisible(available);
	if (!available)
		return;
	if (wasVisible && FPrefsEdited)
	{
		ui.lblPrefsError->setText(FArchive->prefsError(FAccount->accountId()));
		ui.lblPrefsError->setVisible(!ui.lblPrefsError->text().isEmpty());
		return;
	}

	const QSignalBlocker policyBlocker(ui.cmbPolicy);
	int policyIndex = ui.cmbPolicy->findData(FArchive->prefsPolicy(FAccount->accountId()));
	ui.cmbPolicy->setCurrentIndex(policyIndex>=0 ? policyIndex : 0);
	fillList(ui.lstAlways,FArchive->prefsAlways(FAccount->accountId()));
	fillList(ui.lstNever,FArchive->prefsNever(FAccount->accountId()));
	ui.lblPrefsError->setText(FArchive->prefsError(FAccount->accountId()));
	ui.lblPrefsError->setVisible(!ui.lblPrefsError->text().isEmpty());
	onPrefsSelectionChanged();
}

void MamOptionsWidget::fillList(QListWidget *AList, const QStringList &AJids)
{
	AList->clear();
	foreach (const QString &jid, AJids)
		AList->addItem(jid);
}

QStringList MamOptionsWidget::listValues(QListWidget *AList) const
{
	QStringList values;
	for (int i=0; i<AList->count(); ++i)
		values.append(AList->item(i)->text());
	values.sort();
	return values;
}

void MamOptionsWidget::addJid(QListWidget *AList, QLineEdit *AEdit, QListWidget *AOther)
{
	QString jid = normalizedJid(AEdit->text());
	if (jid.isEmpty())
	{
		ui.lblPrefsError->setText(tr("Enter a valid address."));
		ui.lblPrefsError->setVisible(true);
		return;
	}

	QList<QListWidgetItem *> existing = AList->findItems(jid,Qt::MatchExactly);
	if (existing.isEmpty())
		AList->addItem(jid);
	QList<QListWidgetItem *> other = AOther->findItems(jid,Qt::MatchExactly);
	qDeleteAll(other);
	AEdit->clear();
	ui.lblPrefsError->clear();
	ui.lblPrefsError->setVisible(false);
	FPrefsEdited = true;
	emit modified();
}

void MamOptionsWidget::removeSelected(QListWidget *AList)
{
	qDeleteAll(AList->selectedItems());
	FPrefsEdited = true;
	onPrefsSelectionChanged();
	emit modified();
}

QString MamOptionsWidget::normalizedJid(const QString &AText) const
{
	Jid jid(AText.trimmed());
	if (!jid.isValid())
		return QString();
	return jid.hasResource() ? jid.pFull() : jid.pBare();
}
