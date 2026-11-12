#include "spellhighlighter.h"

#include <QRegularExpression>

#include "spellchecker.h"
#include "spellbackend.h"

SpellHighlighter::SpellHighlighter(QTextDocument *ADocument, IMultiUserChat *AMultiUserChat) : QSyntaxHighlighter(ADocument)
{
	FEnabled = true;
	FMultiUserChat = AMultiUserChat;
	FCharFormat.setUnderlineColor(Qt::red);
	FCharFormat.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
}

void SpellHighlighter::setEnabled(bool AEnabled)
{
	if (FEnabled != AEnabled)
	{
		FEnabled = AEnabled;
		rehighlight();
	}
}

void SpellHighlighter::highlightBlock(const QString &AText)
{
	if (FEnabled)
	{
		// Match words (minimally) excluding digits within a word
		static const QRegularExpression expression("\\b[^\\s\\d]+\\b");

		int index = 0;
		QRegularExpressionMatch match = expression.match(AText, index);
		while (match.hasMatch())
		{
			index = match.capturedStart();
			const int length = match.capturedLength();
			if (!isUserNickName(match.captured()))
			{
				if (!SpellBackend::instance()->isCorrect(match.captured()))
					setFormat(index, length, FCharFormat);
			}
			index += length;
			match = expression.match(AText, index);
		}
	}
}

bool SpellHighlighter::isUserNickName(const QString &AText)
{
	return FMultiUserChat!=NULL && FMultiUserChat->findUser(AText)!=NULL;
}
