#ifndef INFOWIDGET_H
#define INFOWIDGET_H

#include <QPicture>
#include "../../definations/accountvaluenames.h"
#include "../../interfaces/imessagewidgets.h"
#include "../../interfaces/iaccountmanager.h"
#include "../../interfaces/iroster.h"
#include "../../interfaces/ipresence.h"
#include "../../interfaces/iclientinfo.h"
#include "../../interfaces/iavatars.h"
#include "ui_infowidget.h"

class InfoWidget : 
  public QWidget,
  public IInfoWidget
{
  Q_OBJECT;
  Q_INTERFACES(IInfoWidget);
public:
  InfoWidget(IMessageWidgets *AMessageWidgets, const Jid& AStreamJid, const Jid &AContactJid);
  ~InfoWidget();
  virtual QWidget *instance() { return this; }
  virtual const Jid &streamJid() const { return FStreamJid; }
  virtual void setStreamJid(const Jid &AStreamJid);
  virtual const Jid &contactJid() const { return FContactJid; }
  virtual void setContactJid(const Jid &AContactJid);
  virtual void autoUpdateFields();
  virtual void autoUpdateField(InfoField AField);
  virtual QVariant field(InfoField AField) const;
  virtual void setField(InfoField AField, const QVariant &AValue);
  virtual bool isFiledAutoUpdated(IInfoWidget::InfoField AField) const;
  virtual int autoUpdatedFields() const { return FAutoFields; }
  virtual void setFieldAutoUpdated(IInfoWidget::InfoField AField, bool AAuto);
  virtual bool isFieldVisible(IInfoWidget::InfoField AField) const;
  virtual int visibleFields() const { return FVisibleFields; }
  virtual void setFieldVisible(IInfoWidget::InfoField AField, bool AVisible);
signals:
  virtual void streamJidChanged(const Jid &ABefour);
  virtual void contactJidChanged(const Jid &ABefour);
  virtual void fieldChanged(IInfoWidget::InfoField AField, const QVariant &AValue);
protected:
  void initialize();
  QString showName(IPresence::Show AShow) const;
protected slots:
  void onAccountChanged(const QString &AName, const QVariant &AValue);  
  void onRosterItemReceived(const IRosterItem &ARosterItem);
  void onPresenceReceived(const IPresenceItem &APresenceItem);
  void onSoftwareInfoChanged(const Jid &AContactJid);
  void onAvatarChanged(const Jid &AContactJid);
private:
  Ui::InfoWidgetClass ui;
private:
  IMessageWidgets *FMessageWidgets;
  IAccount *FAccount;
  IRoster *FRoster;
  IPresence *FPresence;
  IClientInfo *FClientInfo;
  IAvatars *FAvatars;
private:
  int FAutoFields;
  int FVisibleFields;
  Jid FStreamJid;
  Jid FContactJid;
  int FContactShow;
  QString FContactName;
};

#endif // INFOWIDGET_H