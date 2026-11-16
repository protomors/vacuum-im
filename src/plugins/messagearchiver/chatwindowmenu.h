#ifndef CHATWINDOWMENU_H
#define CHATWINDOWMENU_H

#include <interfaces/imessagearchiver.h>
#include <interfaces/imessagewidgets.h>

class ChatWindowMenu :
	public Menu
{
	Q_OBJECT;
public:
	ChatWindowMenu(IMessageArchiver *AArchiver, IMessageToolBarWidget *AToolBarWidget, QWidget *AParent);
	~ChatWindowMenu();
	Jid streamJid() const;
	Jid contactJid() const;
protected:
	void createActions();
	void updateMenu();
	void setArchivingEnabled(bool AEnabled);
protected slots:
	void onActionTriggered(bool);
	void onArchivePrefsChanged(const Jid &AStreamJid);
	void onArchiveRequestCompleted(const QString &AId);
	void onArchiveRequestFailed(const QString &AId, const XmppError &AError);
	void onToolBarWidgetAddressChanged(const Jid &AStreamBefore, const Jid &AContactBefore);
private:
	IMessageToolBarWidget *FToolBarWidget;
	IMessageArchiver *FArchiver;
private:
	Action *FEnableArchiving;
	Action *FDisableArchiving;
private:
	QString FSaveRequest;
};

#endif // CHATWINDOWMENU_H
