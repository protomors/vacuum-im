#include "calldialog.h"

#include <QPixmap>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <definitions/resources.h>
#include <definitions/menuicons.h>
#include <utils/iconstorage.h>

#define AVATAR_SIZE          64
#define CLOSE_TIMEOUT        4000

CallDialog::CallDialog(CallSession *ASession, const QString &AContactName, const QImage &AAvatar, QWidget *AParent) : QWidget(AParent,Qt::Window)
{
	setAttribute(Qt::WA_DeleteOnClose,true);
	setWindowTitle(tr("Call - %1").arg(AContactName));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_VOICECALLS_CALL,0,0,"windowIcon");

	FSession = ASession;

	FAvatarLabel = new QLabel(this);
	FAvatarLabel->setAlignment(Qt::AlignCenter);
	if (!AAvatar.isNull())
		FAvatarLabel->setPixmap(QPixmap::fromImage(AAvatar.scaled(AVATAR_SIZE,AVATAR_SIZE,Qt::KeepAspectRatio,Qt::SmoothTransformation)));
	else
		FAvatarLabel->setVisible(false);

	FNameLabel = new QLabel(this);
	FNameLabel->setAlignment(Qt::AlignCenter);
	FNameLabel->setTextFormat(Qt::PlainText);
	FNameLabel->setText(AContactName);
	QFont nameFont = FNameLabel->font();
	nameFont.setBold(true);
	nameFont.setPointSize(nameFont.pointSize()+2);
	FNameLabel->setFont(nameFont);

	FStateLabel = new QLabel(this);
	FStateLabel->setAlignment(Qt::AlignCenter);
	FStateLabel->setTextFormat(Qt::PlainText);
	FStateLabel->setWordWrap(true);

	FAcceptButton = new QPushButton(tr("Answer"),this);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(FAcceptButton,MNI_VOICECALLS_CALL);
	connect(FAcceptButton,SIGNAL(clicked()),SLOT(onAcceptButtonClicked()));

	FHangupButton = new QPushButton(this);
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(FHangupButton,MNI_VOICECALLS_HANGUP);
	connect(FHangupButton,SIGNAL(clicked()),SLOT(onHangupButtonClicked()));

	FMuteButton = new QPushButton(tr("Mute"),this);
	FMuteButton->setCheckable(true);
	FMuteButton->setToolTip(tr("Turn your microphone off or on"));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(FMuteButton,MNI_VOICECALLS_MUTE);
	connect(FMuteButton,SIGNAL(toggled(bool)),SLOT(onMuteButtonToggled(bool)));

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->addStretch();
	buttonsLayout->addWidget(FAcceptButton);
	buttonsLayout->addWidget(FMuteButton);
	buttonsLayout->addWidget(FHangupButton);
	buttonsLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16,16,16,12);
	mainLayout->setSpacing(8);
	mainLayout->addWidget(FAvatarLabel);
	mainLayout->addWidget(FNameLabel);
	mainLayout->addWidget(FStateLabel);
	mainLayout->addSpacing(8);
	mainLayout->addLayout(buttonsLayout);
	setMinimumWidth(300);

	FDurationTimer.setInterval(1000);
	connect(&FDurationTimer,SIGNAL(timeout()),SLOT(onDurationTimerTimeout()));

	FCloseTimer.setSingleShot(true);
	connect(&FCloseTimer,SIGNAL(timeout()),SLOT(close()));

	connect(ASession,SIGNAL(stateChanged()),SLOT(onSessionStateChanged()));
	connect(ASession,SIGNAL(muteChanged()),SLOT(onSessionMuteChanged()));

	updateDialog();
}

CallDialog::~CallDialog()
{
	emit dialogDestroyed();
}

CallSession *CallDialog::session() const
{
	return FSession;
}

QString CallDialog::stateText(const CallSession *ASession)
{
	switch (ASession->state())
	{
	case CallSession::Proposing:
		return ASession->isRemoteRinging() ? tr("Ringing...") : tr("Calling...");
	case CallSession::Ringing:
		return tr("Incoming call");
	case CallSession::Initiating:
		if (ASession->isAnswered())
			return tr("Connecting...");
		return ASession->isRemoteRinging() ? tr("Ringing...") : tr("Calling...");
	case CallSession::Proceeding:
	case CallSession::Connecting:
		return tr("Connecting...");
	case CallSession::Active:
		return tr("Talking, %1").arg(durationText(ASession->duration()));
	case CallSession::Ended:
		switch (ASession->endReason())
		{
		case CallSession::LocalHangup:
		case CallSession::RemoteHangup:
			if (ASession->wasActive())
				return tr("Call ended, %1").arg(durationText(ASession->duration()));
			return ASession->isOutgoing() ? tr("Call cancelled") : tr("Call ended");
		case CallSession::LocalDecline:
		case CallSession::RemoteDecline:
			return tr("Call declined");
		case CallSession::RemoteBusy:
			return tr("The line is busy");
		case CallSession::RemoteCancel:
			return tr("Missed call");
		case CallSession::NoAnswer:
			return tr("No answer");
		case CallSession::AnsweredElsewhere:
			return tr("Call answered on another device");
		case CallSession::DeclinedElsewhere:
			return tr("Call declined on another device");
		case CallSession::ConnectionLost:
			if (ASession->wasActive())
				return tr("Connection lost, %1").arg(durationText(ASession->duration()));
			return ASession->errorString().isEmpty() ? tr("Connection lost") : ASession->errorString();
		default:
			return ASession->errorString().isEmpty() ? tr("Call failed") : tr("Call failed: %1").arg(ASession->errorString());
		}
	}
	return QString();
}

QString CallDialog::durationText(int ASeconds)
{
	int hours = ASeconds / 3600;
	int minutes = (ASeconds % 3600) / 60;
	int seconds = ASeconds % 60;
	if (hours > 0)
		return QString("%1:%2:%3").arg(hours).arg(minutes,2,10,QChar('0')).arg(seconds,2,10,QChar('0'));
	return QString("%1:%2").arg(minutes,2,10,QChar('0')).arg(seconds,2,10,QChar('0'));
}

void CallDialog::updateDialog()
{
	if (FSession)
	{
		CallSession::State state = FSession->state();
		bool ended = state==CallSession::Ended;
		bool ringing = !FSession->isOutgoing() && state==CallSession::Ringing;
		bool talking = state==CallSession::Connecting || state==CallSession::Active;

		FFinalText = stateText(FSession);
		FStateLabel->setText(FFinalText);

		FAcceptButton->setVisible(ringing);
		FAcceptButton->setDefault(ringing);
		FMuteButton->setVisible(talking);
		FMuteButton->setText(FSession->isMuted() ? tr("Unmute") : tr("Mute"));
		FHangupButton->setText(ended ? tr("Close") : (ringing ? tr("Decline") : tr("Hang Up")));

		if (state == CallSession::Active)
		{
			if (!FDurationTimer.isActive())
				FDurationTimer.start();
		}
		else
		{
			FDurationTimer.stop();
		}

		if (ended && !FCloseTimer.isActive())
		{
			// What the user has done himself needs no explanation, a failure stays until it is read
			CallSession::EndReason reason = FSession->endReason();
			if (reason==CallSession::LocalHangup || reason==CallSession::LocalDecline)
				FCloseTimer.start(0);
			else if (reason != CallSession::Failed)
				FCloseTimer.start(CLOSE_TIMEOUT);
		}
	}
	else
	{
		FStateLabel->setText(FFinalText);
		FAcceptButton->setVisible(false);
		FMuteButton->setVisible(false);
		FHangupButton->setText(tr("Close"));
		FDurationTimer.stop();
	}
}

void CallDialog::closeEvent(QCloseEvent *AEvent)
{
	// A call must not go on without a window to end it
	if (FSession && FSession->state()!=CallSession::Ended)
		FSession->hangup();
	QWidget::closeEvent(AEvent);
}

void CallDialog::onSessionStateChanged()
{
	updateDialog();
}

void CallDialog::onSessionMuteChanged()
{
	if (FSession)
	{
		FMuteButton->blockSignals(true);
		FMuteButton->setChecked(FSession->isMuted());
		FMuteButton->blockSignals(false);
		updateDialog();
	}
}

void CallDialog::onDurationTimerTimeout()
{
	if (FSession && FSession->state()==CallSession::Active)
		FStateLabel->setText(stateText(FSession));
}

void CallDialog::onAcceptButtonClicked()
{
	if (FSession)
		FSession->accept();
}

void CallDialog::onHangupButtonClicked()
{
	if (FSession && FSession->state()!=CallSession::Ended)
		FSession->hangup();
	else
		close();
}

void CallDialog::onMuteButtonToggled(bool AChecked)
{
	if (FSession)
		FSession->setMuted(AChecked);
}
