#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QList>
#include <QSet>

// QList::toSet() was deprecated in Qt 5.14 and removed in Qt 6.
template <typename T>
inline QSet<T> setFromList(const QList<T> &list)
{
	return QSet<T>(list.constBegin(), list.constEnd());
}

#endif // QTCOMPAT_H
