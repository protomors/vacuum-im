#include "archiveaccountoptionswidget.h"
#include <utils/qtutils.h>

#include <QKeyEvent>
#include <QLineEdit>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QIntValidator>
#include <QTextDocument>
#include <definitions/optionvalues.h>

#define ONE_DAY           (24*60*60)
#define ONE_MONTH         (ONE_DAY*31)
#define ONE_YEAR          (ONE_DAY*365)

enum Columns {
	COL_JID,
	COL_SAVE,
	COL_EXPIRE,
	COL_EXACT
};

ArchiveDelegate::ArchiveDelegate(IMessageArchiver *AArchiver, QObject *AParent) : QItemDelegate(AParent)
{
	FArchiver = AArchiver;
}

QString ArchiveDelegate::expireName(int AExpire)
{
	QString name;
	int years = AExpire/ONE_YEAR;
	int months = (AExpire-years*ONE_YEAR)/ONE_MONTH;
	int days = (AExpire-years*ONE_YEAR-months*ONE_MONTH)/ONE_DAY;

	if (AExpire>0)
	{
		if (years > 0)
		{
			name +=  tr("%n year(s)","",years);
		}
		if (months > 0)
		{
			if (!name.isEmpty())
				name += " ";
			name += tr("%n month(s)","",months);
		}
		if (days > 0)
		{
			if (!name.isEmpty())
				name += " ";
			name += tr("%n day(s)","",days);
		}
	}
	else
	{
		name = tr("Never");
	}

	return name;
}

QString ArchiveDelegate::exactMatchName(bool AExact)
{
	return AExact ? tr("Yes") : tr("No");
}

QString ArchiveDelegate::saveModeName(const QString &ASaveMode)
{
	if (ASaveMode == ARCHIVE_SAVE_FALSE)
		return tr("Nothing");
	else if (ASaveMode == ARCHIVE_SAVE_BODY)
		return tr("Body");
	else if (ASaveMode == ARCHIVE_SAVE_MESSAGE)
		return tr("Message");
	else if (ASaveMode == ARCHIVE_SAVE_STREAM)
		return tr("Stream");
	else
		return tr("Unknown");
}

void ArchiveDelegate::updateComboBox(int AColumn, QComboBox *AComboBox)
{
	switch (AColumn)
	{
	case COL_SAVE:
		AComboBox->addItem(saveModeName(ARCHIVE_SAVE_MESSAGE),ARCHIVE_SAVE_MESSAGE);
		AComboBox->addItem(saveModeName(ARCHIVE_SAVE_BODY),ARCHIVE_SAVE_BODY);
		AComboBox->addItem(saveModeName(ARCHIVE_SAVE_FALSE),ARCHIVE_SAVE_FALSE);
		break;
	case COL_EXPIRE:
		AComboBox->setEditable(true);
		AComboBox->addItem(expireName(0),0);
		AComboBox->addItem(expireName(ONE_DAY),ONE_DAY);
		AComboBox->addItem(expireName(7*ONE_DAY),7*ONE_DAY);
		AComboBox->addItem(expireName(ONE_MONTH),ONE_MONTH);
		AComboBox->addItem(expireName(6*ONE_MONTH),6*ONE_MONTH);
		AComboBox->addItem(expireName(ONE_YEAR),ONE_YEAR);
		AComboBox->addItem(expireName(5*ONE_YEAR),5*ONE_YEAR);
		AComboBox->addItem(expireName(10*ONE_YEAR),10*ONE_YEAR);
		AComboBox->setInsertPolicy(QComboBox::NoInsert);
		AComboBox->lineEdit()->setValidator(new QIntValidator(0,50*ONE_YEAR,AComboBox->lineEdit()));
		break;
	case COL_EXACT:
		AComboBox->addItem(exactMatchName(false),false);
		AComboBox->addItem(exactMatchName(true),true);
		break;
	default:
		break;
	}
}

QWidget *ArchiveDelegate::createEditor(QWidget *AParent, const QStyleOptionViewItem &AOption, const QModelIndex &AIndex) const
{
	Q_UNUSED(AOption);
	switch (AIndex.column())
	{
	case COL_SAVE:
	case COL_EXACT:
		{
			QComboBox *comboBox = new QComboBox(AParent);
			updateComboBox(AIndex.column(),comboBox);
			return comboBox;
		}
	case COL_EXPIRE:
		{
			QComboBox *comboBox = new QComboBox(AParent);
			updateComboBox(AIndex.column(),comboBox);
			connect(comboBox,SIGNAL(currentIndexChanged(int)),SLOT(onExpireIndexChanged(int)));
			return comboBox;
		}
	}
	return NULL;
}

void ArchiveDelegate::setEditorData(QWidget *AEditor, const QModelIndex &AIndex) const
{
	switch (AIndex.column())
	{
	case COL_SAVE:
	case COL_EXACT:
		{
			QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
			if (comboBox)
				comboBox->setCurrentIndex(comboBox->findData(AIndex.data(Qt::UserRole)));
		}
		break;
	case COL_EXPIRE:
		{
			QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
			if (comboBox)
				comboBox->setEditText(QString::number(AIndex.data(Qt::UserRole).toInt()/ONE_DAY));
		}
		break;
	default:
		break;
	}
}

void ArchiveDelegate::setModelData(QWidget *AEditor, QAbstractItemModel *AModel, const QModelIndex &AIndex) const
{
	switch (AIndex.column())
	{
	case COL_SAVE:
	case COL_EXACT:
		{
			QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
			if (comboBox)
			{
				int index = comboBox->currentIndex();
				AModel->setData(AIndex,comboBox->itemText(index),Qt::DisplayRole);
				AModel->setData(AIndex,comboBox->itemData(index),Qt::UserRole);
			}
		}
		break;
	case COL_EXPIRE:
		{
			QComboBox *comboBox = qobject_cast<QComboBox *>(AEditor);
			if (comboBox)
			{
				int expire = comboBox->currentText().toInt()*ONE_DAY;
				AModel->setData(AIndex,expireName(expire),Qt::DisplayRole);
				AModel->setData(AIndex,expire,Qt::UserRole);
			}
		}
		break;
	default:
		break;
	}
}

void ArchiveDelegate::updateEditorGeometry(QWidget *AEditor, const QStyleOptionViewItem &AOption, const QModelIndex &AIndex) const
{
	QItemDelegate::updateEditorGeometry(AEditor,AOption,AIndex);
	int widthDelta = AEditor->sizeHint().width() - AEditor->width();
	if (widthDelta>0)
		AEditor->setGeometry(AEditor->x()-widthDelta,AEditor->y(),AEditor->width()+widthDelta,AEditor->height());
}

void ArchiveDelegate::onExpireIndexChanged(int AIndex)
{
	QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
	if (comboBox)
		comboBox->setEditText(QString::number(comboBox->itemData(AIndex).toInt()/ONE_DAY));
}

ArchiveAccountOptionsWidget::ArchiveAccountOptionsWidget(IMessageArchiver *AArchiver, const Jid &AStreamJid, QWidget *AParent) : QWidget(AParent)
{
	ui.setupUi(this);
	FArchiver = AArchiver;
	FStreamJid = AStreamJid;

	ArchiveDelegate *delegat = new ArchiveDelegate(AArchiver,ui.tbwItemPrefs);
	ui.tbwItemPrefs->setItemDelegate(delegat);
	ui.tbwItemPrefs->horizontalHeader()->setSectionResizeMode(COL_JID,QHeaderView::Stretch);
	ui.tbwItemPrefs->horizontalHeader()->setSectionResizeMode(COL_SAVE,QHeaderView::ResizeToContents);
	ui.tbwItemPrefs->horizontalHeader()->setSectionResizeMode(COL_EXPIRE,QHeaderView::ResizeToContents);
	ui.tbwItemPrefs->horizontalHeader()->setSectionResizeMode(COL_EXACT,QHeaderView::ResizeToContents);

	ui.cmbModeSave->addItem(tr("Save messages with formatting"),ARCHIVE_SAVE_MESSAGE);
	ui.cmbModeSave->addItem(tr("Save only messages text"),ARCHIVE_SAVE_BODY);
	ui.cmbModeSave->addItem(tr("Do not save messages"),ARCHIVE_SAVE_FALSE);

	ArchiveDelegate::updateComboBox(COL_EXPIRE,ui.cmbExpireTime);
	ui.cmbExpireTime->installEventFilter(this);
	connect(ui.cmbExpireTime,SIGNAL(currentIndexChanged(int)),SLOT(onExpireIndexChanged(int)));

	ui.lblDefault->setText(QString("<h3>%1</h3>").arg(ui.lblDefault->text()));
	ui.lblEntity->setText(QString("<h3>%1</h3>").arg(ui.lblEntity->text()));

	connect(ui.pbtAdd,SIGNAL(clicked()),SLOT(onAddItemPrefClicked()));
	connect(ui.pbtRemove,SIGNAL(clicked()),SLOT(onRemoveItemPrefClicked()));
	connect(FArchiver->instance(),SIGNAL(archivePrefsOpened(const Jid &)),SLOT(onArchivePrefsOpened(const Jid &)));
	connect(FArchiver->instance(),SIGNAL(archivePrefsChanged(const Jid &)),SLOT(onArchivePrefsChanged(const Jid &)));
	connect(FArchiver->instance(),SIGNAL(archivePrefsClosed(const Jid &)),SLOT(onArchivePrefsClosed(const Jid &)));
	connect(FArchiver->instance(),SIGNAL(requestCompleted(const QString &)),SLOT(onArchiveRequestCompleted(const QString &)));
	connect(FArchiver->instance(),SIGNAL(requestFailed(const QString &, const XmppError &)),SLOT(onArchiveRequestFailed(const QString &, const XmppError &)));

	connect(ui.cmbModeSave,SIGNAL(currentIndexChanged(int)),SIGNAL(modified()));
	connect(ui.cmbExpireTime,SIGNAL(currentIndexChanged(int)),SIGNAL(modified()));
	connect(ui.cmbExpireTime->lineEdit(),SIGNAL(textChanged(const QString &)),SIGNAL(modified()));
	connect(delegat,SIGNAL(commitData(QWidget *)),SIGNAL(modified()));

	reset();
}

ArchiveAccountOptionsWidget::~ArchiveAccountOptionsWidget()
{

}

void ArchiveAccountOptionsWidget::apply()
{
	if (FSaveRequests.isEmpty())
	{
		IArchiveStreamPrefs prefs = FArchiver->archivePrefs(FStreamJid);
		prefs.defaultPrefs.save = ui.cmbModeSave->itemData(ui.cmbModeSave->currentIndex()).toString();
		prefs.defaultPrefs.expire = ui.cmbExpireTime->itemData(ui.cmbExpireTime->currentIndex()).toInt();

		foreach(const Jid &itemJid, FTableItems.keys())
		{
			QTableWidgetItem *jidItem = FTableItems.value(itemJid);
			prefs.itemPrefs[itemJid].save = ui.tbwItemPrefs->item(jidItem->row(),COL_SAVE)->data(Qt::UserRole).toString();
			prefs.itemPrefs[itemJid].expire = ui.tbwItemPrefs->item(jidItem->row(),COL_EXPIRE)->data(Qt::UserRole).toInt();
			prefs.itemPrefs[itemJid].exactmatch = ui.tbwItemPrefs->item(jidItem->row(),COL_EXACT)->data(Qt::UserRole).toBool();
		}

		foreach(const Jid &itemJid, prefs.itemPrefs.keys())
		{
			if (!FTableItems.contains(itemJid))
				prefs.itemPrefs[itemJid].save = QString();
		}

		QString requestId = FArchiver->setArchivePrefs(FStreamJid,prefs);
		if (!requestId.isEmpty())
			FSaveRequests.append(requestId);

		FLastError = XmppError::null;
		updateWidget();
	}

	emit childApply();
}

void ArchiveAccountOptionsWidget::reset()
{
	FTableItems.clear();
	ui.tbwItemPrefs->clearContents();
	ui.tbwItemPrefs->setRowCount(0);

	if (FArchiver->isReady(FStreamJid))
		onArchivePrefsChanged(FStreamJid);

	FLastError = XmppError::null;
	updateWidget();

	emit childReset();
}

void ArchiveAccountOptionsWidget::updateWidget()
{
	bool requesting = !FSaveRequests.isEmpty();

	ui.wdtDefault->setEnabled(!requesting && FArchiver->isArchivePrefsEnabled(FStreamJid));
	ui.wdtEntity->setEnabled(!requesting && FArchiver->isArchivePrefsEnabled(FStreamJid));

	if (requesting)
		ui.lblStatus->setText(tr("Waiting for host response..."));
	else if (!FArchiver->isReady(FStreamJid))
		ui.lblStatus->setText(tr("History preferences is not available"));
	else if (!FLastError.isNull())
		ui.lblStatus->setText(tr("Failed to save archive preferences: %1").arg(FLastError.errorMessage()));
	else if (!ui.lblStatus->text().isEmpty())
		ui.lblStatus->setText(tr("Preferences accepted"));

	setEnabled(FArchiver->isReady(FStreamJid));
}

void ArchiveAccountOptionsWidget::updateColumnsSize()
{
	ui.tbwItemPrefs->horizontalHeader()->reset();
}

void ArchiveAccountOptionsWidget::updateItemPrefs(const Jid &AItemJid, const IArchiveItemPrefs &APrefs)
{
	if (!FTableItems.contains(AItemJid))
	{
		QTableWidgetItem *jidItem = new QTableWidgetItem(AItemJid.uFull());
		QTableWidgetItem *saveItem = new QTableWidgetItem();
		QTableWidgetItem *expireItem = new QTableWidgetItem();
		QTableWidgetItem *exactItem = new QTableWidgetItem();
		ui.tbwItemPrefs->setRowCount(ui.tbwItemPrefs->rowCount()+1);
		ui.tbwItemPrefs->setItem(ui.tbwItemPrefs->rowCount()-1,COL_JID,jidItem);
		ui.tbwItemPrefs->setItem(jidItem->row(),COL_SAVE,saveItem);
		ui.tbwItemPrefs->setItem(jidItem->row(),COL_EXPIRE,expireItem);
		ui.tbwItemPrefs->setItem(jidItem->row(),COL_EXACT,exactItem);
		ui.tbwItemPrefs->verticalHeader()->setSectionResizeMode(jidItem->row(),QHeaderView::ResizeToContents);
		FTableItems.insert(AItemJid,jidItem);
	}

	QTableWidgetItem *jidItem = FTableItems.value(AItemJid);
	ui.tbwItemPrefs->item(jidItem->row(),COL_SAVE)->setText(ArchiveDelegate::saveModeName(APrefs.save));
	ui.tbwItemPrefs->item(jidItem->row(),COL_SAVE)->setData(Qt::UserRole,APrefs.save);
	ui.tbwItemPrefs->item(jidItem->row(),COL_EXPIRE)->setText(ArchiveDelegate::expireName(APrefs.expire));
	ui.tbwItemPrefs->item(jidItem->row(),COL_EXPIRE)->setData(Qt::UserRole,APrefs.expire);
	ui.tbwItemPrefs->item(jidItem->row(),COL_EXACT)->setText(ArchiveDelegate::exactMatchName(APrefs.exactmatch));
	ui.tbwItemPrefs->item(jidItem->row(),COL_EXACT)->setData(Qt::UserRole,APrefs.exactmatch);

	updateColumnsSize();
}

void ArchiveAccountOptionsWidget::removeItemPrefs(const Jid &AItemJid)
{
	if (FTableItems.contains(AItemJid))
	{
		QTableWidgetItem *jidItem = FTableItems.take(AItemJid);
		ui.tbwItemPrefs->removeRow(jidItem->row());
		updateColumnsSize();
	}
}

bool ArchiveAccountOptionsWidget::eventFilter(QObject *AObject, QEvent *AEvent)
{
	bool hooked = false;
	if (AObject == ui.cmbExpireTime)
	{
		bool appendItem = false;
		if (AEvent->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>(AEvent);
			if (keyEvent && (keyEvent->key()==Qt::Key_Return || keyEvent->key()==Qt::Key_Enter))
			{
				hooked = true;
				setFocus();
			}
		}
		else if (AEvent->type() == QEvent::FocusOut)
		{
			appendItem = true;
		}
		else if (AEvent->type() == QEvent::FocusIn)
		{
			ui.cmbExpireTime->setEditText(QString::number(ui.cmbExpireTime->itemData(ui.cmbExpireTime->currentIndex()).toInt()/ONE_DAY));
		}
		if (appendItem)
		{
			bool ok = false;
			int expireIndex = ui.cmbExpireTime->currentIndex();
			int expire = ui.cmbExpireTime->currentText().toInt(&ok)*ONE_DAY;
			if (ok)
			{
				expireIndex = ui.cmbExpireTime->findData(expire);
				if (expireIndex<0)
				{
					ui.cmbExpireTime->addItem(ArchiveDelegate::expireName(expire),expire);
					expireIndex = ui.cmbExpireTime->count()-1;
				}
			}
			ui.cmbExpireTime->setCurrentIndex(expireIndex);
		}
	}
	return hooked ? true : QWidget::eventFilter(AObject,AEvent);
}

void ArchiveAccountOptionsWidget::onAddItemPrefClicked()
{
	Jid itemJid = Jid::fromUserInput(QInputDialog::getText(this,tr("New item preferences"),tr("Enter item JID:")));
	if (itemJid.isValid() && !FTableItems.contains(itemJid))
	{
		IArchiveItemPrefs itemPrefs = FArchiver->archiveItemPrefs(FStreamJid,itemJid);
		updateItemPrefs(itemJid,itemPrefs);
		ui.tbwItemPrefs->setCurrentItem(FTableItems.value(itemJid));
		emit modified();
	}
	else if (!itemJid.isEmpty())
	{
		QMessageBox::warning(this,tr("Unacceptable item JID"),tr("'%1' is not valid JID or already exists").arg(itemJid.uFull().toHtmlEscaped()));
	}
}

void ArchiveAccountOptionsWidget::onRemoveItemPrefClicked()
{
	QList<QTableWidgetItem *> selectedRows;
	foreach(QTableWidgetItem *item, ui.tbwItemPrefs->selectedItems())
		if (item->column() == COL_JID)
			selectedRows.append(item);

	foreach(QTableWidgetItem *item, selectedRows)
	{
		removeItemPrefs(FTableItems.key(item));
		emit modified();
	}
}

void ArchiveAccountOptionsWidget::onExpireIndexChanged(int AIndex)
{
	if (ui.cmbExpireTime->hasFocus() || ui.cmbExpireTime->lineEdit()->hasFocus())
		ui.cmbExpireTime->setEditText(QString::number(ui.cmbExpireTime->itemData(AIndex).toInt()/ONE_DAY));
}

void ArchiveAccountOptionsWidget::onArchivePrefsOpened( const Jid &AStreamJid )
{
	if (AStreamJid == FStreamJid)
		updateWidget();
}

void ArchiveAccountOptionsWidget::onArchivePrefsChanged(const Jid &AStreamJid)
{
	if (AStreamJid == FStreamJid)
	{
		IArchiveStreamPrefs prefs = FArchiver->archivePrefs(AStreamJid);

		ui.cmbModeSave->setCurrentIndex(ui.cmbModeSave->findData(prefs.defaultPrefs.save));

		int expireIndex = ui.cmbExpireTime->findData(prefs.defaultPrefs.expire);
		if (expireIndex<0)
		{
			ui.cmbExpireTime->addItem(ArchiveDelegate::expireName(prefs.defaultPrefs.expire),prefs.defaultPrefs.expire);
			expireIndex = ui.cmbExpireTime->count()-1;
		}
		ui.cmbExpireTime->setCurrentIndex(expireIndex);

		QSet<Jid> oldItems = setFromList(FTableItems.keys());
		foreach(const Jid &itemJid, prefs.itemPrefs.keys())
		{
			oldItems -= itemJid;
			updateItemPrefs(itemJid,prefs.itemPrefs.value(itemJid));
		}

		foreach(const Jid &itemJid, oldItems)
		{
			removeItemPrefs(itemJid);
		}

		updateWidget();
		updateColumnsSize();
	}
}

void ArchiveAccountOptionsWidget::onArchivePrefsClosed( const Jid &AStreamJid )
{
	if (AStreamJid == FStreamJid)
		updateWidget();
}

void ArchiveAccountOptionsWidget::onArchiveRequestCompleted(const QString &AId)
{
	if (FSaveRequests.removeOne(AId))
	{
		updateWidget();
	}
}

void ArchiveAccountOptionsWidget::onArchiveRequestFailed(const QString &AId, const XmppError &AError)
{
	if (FSaveRequests.removeOne(AId))
	{
		FLastError = AError;
		updateWidget();
		emit modified();
	}
}
