#ifndef MAMOPTIONSWIDGET_H
#define MAMOPTIONSWIDGET_H

#include <QUuid>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/iaccountmanager.h>
#include "ui_mamoptionswidget.h"

class MamArchive;

class MamOptionsWidget :
	public QWidget,
	public IOptionsDialogWidget
{
	Q_OBJECT;
	Q_INTERFACES(IOptionsDialogWidget);
public:
	MamOptionsWidget(MamArchive *AArchive, IAccount *AAccount, QWidget *AParent);
	~MamOptionsWidget();
	virtual QWidget *instance() { return this; }
public slots:
	virtual void apply();
	virtual void reset();
signals:
	void modified();
	void childApply();
	void childReset();
protected slots:
	void onAccountStatusChanged(const QUuid &AAccountId);
	void onAccountPrefsChanged(const QUuid &AAccountId);
	void onStartToggled(bool AChecked);
	void onAddAlways();
	void onAddNever();
	void onRemoveAlways();
	void onRemoveNever();
	void onPrefsSelectionChanged();
	void onPolicyChanged();
private:
	void updateStatus();
	void updatePrefs();
	void fillList(QListWidget *AList, const QStringList &AJids);
	QStringList listValues(QListWidget *AList) const;
	void addJid(QListWidget *AList, QLineEdit *AEdit, QListWidget *AOther);
	void removeSelected(QListWidget *AList);
	QString normalizedJid(const QString &AText) const;
private:
	Ui::MamOptionsWidgetClass ui;
private:
	MamArchive *FArchive;
	IAccount *FAccount;
	bool FPrefsEdited;
};

#endif // MAMOPTIONSWIDGET_H
