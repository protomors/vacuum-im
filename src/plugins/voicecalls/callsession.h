#ifndef CALLSESSION_H
#define CALLSESSION_H

#include <QMap>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QDateTime>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/ijingle.h>
#include "mediasession.h"

// One voice call: Jingle Message Initiation (XEP-0353) to find the device that takes the call,
// then a Jingle RTP session (XEP-0166, XEP-0167) with that device
class CallSession :
	public QObject,
	public IStanzaRequestOwner
{
	Q_OBJECT;
	Q_INTERFACES(IStanzaRequestOwner);
public:
	enum State {
		Proposing,   // outgoing: waiting for a device of the contact to take the call
		Ringing,     // incoming: waiting for the user to answer
		Proceeding,  // incoming: answered, waiting for the session from the caller
		Initiating,  // outgoing: session offered, waiting for it to be accepted
		Connecting,  // session accepted, media connection is being established
		Active,
		Ended
	};
	enum EndReason {
		NoReason,
		LocalHangup,
		RemoteHangup,
		LocalDecline,
		RemoteDecline,
		RemoteBusy,
		RemoteCancel,      // the caller gave up before the call was answered
		NoAnswer,
		AnsweredElsewhere, // another device of this account took the call
		DeclinedElsewhere,
		ConnectionLost,
		Failed
	};
public:
	CallSession(IStanzaProcessor *AStanzaProcessor, IJingle *AJingle, const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, bool AOutgoing, QObject *AParent);
	~CallSession();
	virtual QObject *instance() { return this; }
	//IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
	//CallSession
	Jid streamJid() const;
	Jid contactJid() const;
	QString sessionId() const;
	bool isOutgoing() const;
	State state() const;
	EndReason endReason() const;
	QString errorString() const;
	bool isRemoteRinging() const;
	bool isAnswered() const;
	bool wasActive() const;
	int duration() const;
	bool isMuted() const;
	void setMuted(bool AMuted);
	// Fetches STUN and TURN servers from the account server (XEP-0215) before media starts
	void requestIceServers();
	void setFallbackStunServer(const QString &AServer);
	// Outgoing call
	void startProposal();
	void startSession();
	// Incoming call
	void receiveProposal();
	bool receiveSession(const Stanza &ARequest);
	// User actions
	void accept();
	void decline();
	void hangup();
	// Stanzas of this call
	void processMessage(const Jid &AFrom, const QDomElement &AElem, bool AFromSelf);
	bool processRequest(const Stanza &ARequest);
	// Ends the call without telling the remote party
	void abort(EndReason AReason, const QString &AError = QString());
signals:
	void stateChanged();
	void muteChanged();
	void ended();
protected:
	void setState(State AState);
	void end(EndReason AReason, const QString &AError = QString());
	void terminate(const QString &AJingleReason, EndReason AReason, const QString &AError = QString());
	bool hasJingleSession() const;
	void startMedia();
	void sendJingleMessage(const QString &AAction, const Jid &ATo);
	Stanza makeJingleRequest(const QString &AAction, QDomElement &AJingle);
	bool sendJingleRequest(Stanza &ARequest);
	void sendLocalCandidates();
	void storeLocalCredentials(const QDomElement &AJingle);
	bool acceptSessionOffer(const Stanza &ARequest, QString &AJingleReason, QString &AError);
	void applySessionAccept(const QDomElement &AJingle);
	void applyTransportInfo(const QDomElement &AJingle);
	void parseIceServers(const QDomElement &AServices);
	static EndReason endReasonFromJingle(const QString &AReason, State AState, bool AOutgoing);
protected slots:
	void onStateTimeout();
	void onCandidatesTimeout();
	void onReconnectTimeout();
	void onMediaLocalDescription(const QString &AType, const QString &ASdp);
	void onMediaLocalCandidate(int AMLineIndex, const QString &ACandidate);
	void onMediaConnected();
	void onMediaDisconnected();
	void onMediaFailed(const QString &AError);
	void onMediaConnectionFailed();
private:
	IStanzaProcessor *FStanzaProcessor;
	IJingle *FJingle;
private:
	Jid FStreamJid;
	Jid FContactJid;
	QString FSessionId;
	bool FOutgoing;
	bool FProposed;
	State FState;
	EndReason FEndReason;
	QString FError;
	bool FRemoteRinging;
	bool FAnswered;
	bool FAccepted;
	bool FMuted;
	QDateTime FActiveTime;
	QDateTime FEndTime;
private:
	MediaSession *FMedia;
	bool FMediaWaiting;
	bool FDescriptionSent;
	bool FHasOffer;
	Stanza FSessionOffer;
	QStringList FContentNames;
	QMap<QString, QPair<QString,QString> > FLocalCredentials;
	QList<QPair<int, QString> > FLocalCandidates;
	QList<QPair<int, QString> > FRemoteCandidates;
private:
	bool FServicesReady;
	QString FServicesRequest;
	QString FFallbackStun;
	QList<MediaIceServer> FIceServers;
	QMap<QString, QString> FRequests;
private:
	QTimer FStateTimer;
	QTimer FCandidatesTimer;
	QTimer FReconnectTimer;
};

#endif // CALLSESSION_H
