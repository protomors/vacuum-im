#include "chatwindowmenu.h"

#include <definitions/resources.h>
#include <definitions/menuicons.h>
#include <utils/menu.h>
#include <QActionGroup>

ChatWindowMenu::ChatWindowMenu(IMessageArchiver *AArchiver, IMessageToolBarWidget *AToolBarWidget, QWidget *AParent) : Menu(AParent)
{
	FToolBarWidget = AToolBarWidget;
	connect(FToolBarWidget->messageWindow()->address()->instance(),SIGNAL(addressChanged(const Jid &,const Jid &)),SLOT(onToolBarWidgetAddressChanged(const Jid &,const Jid &)));

	FArchiver = AArchiver;
	connect(FArchiver->instance(),SIGNAL(archivePrefsChanged(const Jid &)),SLOT(onArchivePrefsChanged(const Jid &)));
	connect(FArchiver->instance(),SIGNAL(requestCompleted(const QString &)),SLOT(onArchiveRequestCompleted(const QString &)));
	connect(FArchiver->instance(),SIGNAL(requestFailed(const QString &, const XmppError &)),SLOT(onArchiveRequestFailed(const QString &, const XmppError &)));

	createActions();
	updateMenu();
}

ChatWindowMenu::~ChatWindowMenu()
{

}

Jid ChatWindowMenu::streamJid() const
{
	return FToolBarWidget->messageWindow()->address()->streamJid();
}

Jid ChatWindowMenu::contactJid() const
{
	return FToolBarWidget->messageWindow()->address()->contactJid();
}

void ChatWindowMenu::createActions()
{
	QActionGroup *archivingGroup = new QActionGroup(this);

	FEnableArchiving = new Action(this);
	FEnableArchiving->setCheckable(true);
	FEnableArchiving->setText(tr("Enable Message Archiving"));
	FEnableArchiving->setActionGroup(archivingGroup);
	connect(FEnableArchiving,SIGNAL(triggered(bool)),SLOT(onActionTriggered(bool)));
	addAction(FEnableArchiving,AG_DEFAULT,false);

	FDisableArchiving = new Action(this);
	FDisableArchiving->setCheckable(true);
	FDisableArchiving->setText(tr("Disable Message Archiving"));
	FDisableArchiving->setActionGroup(archivingGroup);
	connect(FDisableArchiving,SIGNAL(triggered(bool)),SLOT(onActionTriggered(bool)));
	addAction(FDisableArchiving,AG_DEFAULT,false);
}

void ChatWindowMenu::updateMenu()
{
	if (FArchiver->isArchivePrefsEnabled(streamJid()))
	{
		IArchiveItemPrefs iprefs = FArchiver->archiveItemPrefs(streamJid(),contactJid());

		FEnableArchiving->setChecked(iprefs.save != ARCHIVE_SAVE_FALSE);
		FEnableArchiving->setEnabled(FSaveRequest.isEmpty());

		FDisableArchiving->setChecked(iprefs.save == ARCHIVE_SAVE_FALSE);
		FDisableArchiving->setEnabled(FSaveRequest.isEmpty());
	}
	else
	{
		FEnableArchiving->setEnabled(false);
		FEnableArchiving->setChecked(false);

		FDisableArchiving->setEnabled(false);
		FDisableArchiving->setChecked(false);
	}
}

void ChatWindowMenu::setArchivingEnabled(bool AEnabled)
{
	Jid itemJid = contactJid().bare();
	IArchiveItemPrefs iprefs = FArchiver->archiveItemPrefs(streamJid(),itemJid);
	if ((iprefs.save != ARCHIVE_SAVE_FALSE) != AEnabled)
	{
		IArchiveStreamPrefs sprefs = FArchiver->archivePrefs(streamJid());
		if (AEnabled)
			iprefs.save = sprefs.defaultPrefs.save!=ARCHIVE_SAVE_FALSE ? sprefs.defaultPrefs.save : QString(ARCHIVE_SAVE_MESSAGE);
		else
			iprefs.save = ARCHIVE_SAVE_FALSE;

		if (iprefs != sprefs.defaultPrefs)
		{
			sprefs.itemPrefs.insert(itemJid,iprefs);
			FSaveRequest = FArchiver->setArchivePrefs(streamJid(),sprefs);
		}
		else
		{
			FSaveRequest = FArchiver->removeArchiveItemPrefs(streamJid(),itemJid);
		}
	}
}

void ChatWindowMenu::onActionTriggered(bool)
{
	Action *action = qobject_cast<Action *>(sender());
	if (action && FSaveRequest.isEmpty())
	{
		if (action == FEnableArchiving)
			setArchivingEnabled(true);
		else if (action == FDisableArchiving)
			setArchivingEnabled(false);
		updateMenu();
	}
}

void ChatWindowMenu::onArchivePrefsChanged(const Jid &AStreamJid)
{
	if (streamJid() == AStreamJid)
		updateMenu();
}

void ChatWindowMenu::onArchiveRequestCompleted(const QString &AId)
{
	if (FSaveRequest == AId)
	{
		FSaveRequest.clear();
		updateMenu();
	}
}

void ChatWindowMenu::onArchiveRequestFailed(const QString &AId, const XmppError &AError)
{
	if (FSaveRequest == AId)
	{
		if (FToolBarWidget->messageWindow()->viewWidget() != NULL)
		{
			IMessageStyleContentOptions options;
			options.kind = IMessageStyleContentOptions::KindStatus;
			options.type |= IMessageStyleContentOptions::TypeEvent;
			options.direction = IMessageStyleContentOptions::DirectionIn;
			options.time = QDateTime::currentDateTime();
			FToolBarWidget->messageWindow()->viewWidget()->appendText(tr("Failed to change archive preferences: %1").arg(AError.errorMessage()),options);
		}
		FSaveRequest.clear();
		updateMenu();
	}
}

void ChatWindowMenu::onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore)
{
	Q_UNUSED(AStreamBefore); Q_UNUSED(AContactBefore);
	updateMenu();
}
