#ifndef JINGLE_H
#define JINGLE_H

#include <QMap>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ijingle.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/iservicediscovery.h>

// Routes Jingle requests (XEP-0166) to the applications that own the sessions
class Jingle :
	public QObject,
	public IPlugin,
	public IJingle,
	public IStanzaHandler
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IJingle IStanzaHandler);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.Jingle");
public:
	Jingle();
	~Jingle();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return JINGLE_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects() { return true; }
	virtual bool initSettings() { return true; }
	virtual bool startPlugin() { return true; }
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandleId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IJingle
	virtual QList<QString> applications() const;
	virtual IJingleHandler *applicationHandler(const QString &ADescriptionNS) const;
	virtual bool insertApplication(const QString &ADescriptionNS, IJingleHandler *AHandler);
	virtual void removeApplication(const QString &ADescriptionNS);
	virtual QString newSessionId() const;
	virtual IJingleHandler *sessionHandler(const Jid &AStreamJid, const QString &ASessionId) const;
	virtual bool insertSession(const Jid &AStreamJid, const QString &ASessionId, IJingleHandler *AHandler);
	virtual void removeSession(const Jid &AStreamJid, const QString &ASessionId);
	virtual Stanza makeRequest(const Jid &AContactJid, const QString &ASessionId, const QString &AAction, QDomElement &AJingle) const;
	virtual bool sendResult(const Jid &AStreamJid, const Stanza &ARequest) const;
	virtual bool sendError(const Jid &AStreamJid, const Stanza &ARequest, XmppStanzaError::ErrorCondition ACondition, const QString &AJingleCondition = QString()) const;
	virtual bool sendTerminate(const Jid &AStreamJid, const Jid &AContactJid, const QString &ASessionId, const QString &AReason, const QString &AText = QString()) const;
signals:
	void sessionInserted(const Jid &AStreamJid, const QString &ASessionId);
	void sessionRemoved(const Jid &AStreamJid, const QString &ASessionId);
protected:
	void processRequest(const Jid &AStreamJid, const Stanza &ARequest);
	void updateDiscoFeature();
	void watchHandler(IJingleHandler *AHandler);
protected slots:
	void onXmppStreamOpened(IXmppStream *AXmppStream);
	void onXmppStreamClosed(IXmppStream *AXmppStream);
	void onHandlerDestroyed(QObject *AObject);
private:
	IXmppStreamManager *FXmppStreamManager;
	IStanzaProcessor *FStanzaProcessor;
	IServiceDiscovery *FDiscovery;
private:
	bool FFeatureInserted;
	QMap<Jid, int> FSHIRequests;
	QMap<QString, IJingleHandler *> FApplications;
	QMap<Jid, QMap<QString, IJingleHandler *> > FSessions;
	QMap<IJingleHandler *, QObject *> FHandlerObjects;
};

#endif // JINGLE_H
