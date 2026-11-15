#ifndef CALLDIALOG_H
#define CALLDIALOG_H

#include <QLabel>
#include <QTimer>
#include <QImage>
#include <QWidget>
#include <QPointer>
#include <QPushButton>
#include "callsession.h"

// Window of one call: who it is with, what is going on and the buttons to answer, mute and hang up
class CallDialog :
	public QWidget
{
	Q_OBJECT;
public:
	CallDialog(CallSession *ASession, const QString &AContactName, const QImage &AAvatar, QWidget *AParent = NULL);
	~CallDialog();
	CallSession *session() const;
	// Short description of the call state, the same text is used in notifications and chat windows
	static QString stateText(const CallSession *ASession);
	static QString durationText(int ASeconds);
signals:
	void dialogDestroyed();
protected:
	void updateDialog();
	void closeEvent(QCloseEvent *AEvent);
protected slots:
	void onSessionStateChanged();
	void onSessionMuteChanged();
	void onDurationTimerTimeout();
	void onAcceptButtonClicked();
	void onHangupButtonClicked();
	void onMuteButtonToggled(bool AChecked);
private:
	QLabel *FAvatarLabel;
	QLabel *FNameLabel;
	QLabel *FStateLabel;
	QPushButton *FAcceptButton;
	QPushButton *FHangupButton;
	QPushButton *FMuteButton;
private:
	QTimer FDurationTimer;
	QTimer FCloseTimer;
	QString FFinalText;
	QPointer<CallSession> FSession;
};

#endif // CALLDIALOG_H
