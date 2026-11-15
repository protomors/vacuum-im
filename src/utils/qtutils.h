#ifndef QTUTILS_H
#define QTUTILS_H

#include <QtGlobal>
#include <QList>
#include <QMetaType>
#include <QMultiMap>
#include <QRegularExpression>
#include <QSet>

template <typename T>
inline QSet<T> setFromList(const QList<T> &list)
{
	return QSet<T>(list.constBegin(), list.constEnd());
}

// QRegularExpression::wildcardToRegularExpression() is a file-glob conversion.
// It does not match '*' and '?' against any character, including '/'.
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

#endif // QTUTILS_H
