#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QRegularExpression>
#include <QSet>

// QMultiMap no longer inherits QMap in Qt 6, so QMapIterator cannot walk one.
// Qt 5.15 has no QMultiMapIterator; QMapIterator accepts QMultiMap there.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
template <typename Key, typename T>
using MultiMapIterator = QMultiMapIterator<Key, T>;
#else
template <typename Key, typename T>
using MultiMapIterator = QMapIterator<Key, T>;
#endif

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

// QRegExp::Wildcard matches '*' and '?' against any character, including '/'.
// QRegularExpression::wildcardToRegularExpression() is a file-glob conversion and does not.
inline QRegularExpression regularExpressionFromWildcard(const QString &pattern, Qt::CaseSensitivity sensitivity, bool anchor)
{
	QString converted;
	converted.reserve(pattern.size() * 2);
	for (int index = 0; index < pattern.size(); ++index)
	{
		const QChar ch = pattern.at(index);
		if (ch == QLatin1Char('*'))
			converted += QLatin1String(".*");
		else if (ch == QLatin1Char('?'))
			converted += QLatin1Char('.');
		else
			converted += QRegularExpression::escape(QString(ch));
	}
	if (anchor)
		converted = QRegularExpression::anchoredPattern(converted);

	QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
	if (sensitivity == Qt::CaseInsensitive)
		options |= QRegularExpression::CaseInsensitiveOption;
	return QRegularExpression(converted, options);
}

#endif // QTCOMPAT_H
