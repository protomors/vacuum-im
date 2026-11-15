#ifndef MEDIASESSION_H
#define MEDIASESSION_H

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct MediaIceServer
{
	MediaIceServer() {
		port = 0;
	}
	QString type;      // stun, turn or turns
	QString host;
	int port;
	QString transport; // udp or tcp
	QString username;
	QString password;
};

struct MediaSessionPrivate;

// One audio call over WebRTC: microphone -> Opus -> SRTP over ICE and back to the speakers.
// The owner exchanges the session descriptions and ICE candidates with the remote party.
class MediaSession :
	public QObject
{
	Q_OBJECT;
public:
	enum Role {
		Offerer,
		Answerer
	};
	enum TestMode {
		RealDevices,
		TestDevices // generated tone instead of the microphone, no playback
	};
public:
	MediaSession(QObject *AParent = NULL);
	~MediaSession();
	// Empty list when calls can be made, otherwise names of the missing GStreamer elements
	static QStringList missingElements();
	static void setTestMode(TestMode AMode);
	bool isStarted() const;
	// AOpusPayload is the RTP payload type the remote party offered for Opus, 0 to pick our own
	bool start(Role ARole, const QList<MediaIceServer> &AServers, int AOpusPayload = 0);
	void stop();
	// AType is "offer" or "answer"
	bool setRemoteDescription(const QString &AType, const QString &ASdp);
	void addRemoteCandidate(int AMLineIndex, const QString &ACandidate);
	bool isMuted() const;
	void setMuted(bool AMuted);
	QString errorString() const;
signals:
	void localDescriptionCreated(const QString &AType, const QString &ASdp);
	void localCandidateFound(int AMLineIndex, const QString &ACandidate);
	void connected();
	void disconnected();
	void failed(const QString &AError);
	void connectionFailed();
	void remoteAudioStarted();
protected:
	Q_INVOKABLE void handleLocalDescription(const QString &AType, const QString &ASdp);
	Q_INVOKABLE void handleLocalCandidate(int AMLineIndex, const QString &ACandidate);
	Q_INVOKABLE void handleConnectionState(int AState);
	Q_INVOKABLE void handleError(const QString &AError);
	Q_INVOKABLE void handleRemoteAudio();
private:
	friend struct MediaSessionPrivate;
	MediaSessionPrivate *d;
};

#endif // MEDIASESSION_H
