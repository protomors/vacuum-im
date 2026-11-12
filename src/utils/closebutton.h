#ifndef CLOSEBUTTON_H
#define CLOSEBUTTON_H

#include <QAbstractButton>
#include "utilsexport.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QEnterEvent;
#endif

class UTILS_EXPORT CloseButton : 
	public QAbstractButton
{
	Q_OBJECT;
public:
	CloseButton(QWidget *AParent = NULL);
	~CloseButton();
	QSize sizeHint() const;
protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *AEvent);
#else
	void enterEvent(QEvent *AEvent);
#endif
	void leaveEvent(QEvent *AEvent);
	void paintEvent(QPaintEvent *AEvent);
};

#endif // CLOSEBUTTON_H
