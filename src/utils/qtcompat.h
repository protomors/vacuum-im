#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>
#include <QList>
#include <QMetaType>
#include <QSet>

// QList::toSet() was deprecated in Qt 5.14 and removed in Qt 6.
template <typename T>
inline QSet<T> setFromList(const QList<T> &list)
{
	return QSet<T>(list.constBegin(), list.constEnd());
}

// qRegisterMetaTypeStreamOperators() was removed in Qt 6. Registering the
// metatype is enough there, because the stream operators are found automatically.
template <typename T>
inline void registerStreamOperators(const char *name)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qRegisterMetaType<T>(name);
#else
	qRegisterMetaTypeStreamOperators<T>(name);
#endif
}

#endif // QTCOMPAT_H
