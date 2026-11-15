#include "hunspellchecker.h"

#include <hunspell.hxx>

#include <string>
#include <vector>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QCoreApplication>
#include <QStringDecoder>
#include <QStringEncoder>
#include <utils/logger.h>
#include "spellchecker.h"

static QStringEncoder dictEncoder(const QByteArray &encoding)
{
	if (!encoding.isEmpty())
	{
		QStringEncoder encoder(encoding);
		if (encoder.isValid())
			return encoder;
	}
	return QStringEncoder(QStringConverter::Utf8);
}

static QStringDecoder dictDecoder(const QByteArray &encoding)
{
	if (!encoding.isEmpty())
	{
		QStringDecoder decoder(encoding);
		if (decoder.isValid())
			return decoder;
	}
	return QStringDecoder(QStringConverter::Utf8);
}

HunspellChecker::HunspellChecker() : FHunSpell(NULL)
{
#if defined (Q_OS_WIN)
	FDictsPaths.append(QString("%1/hunspell").arg(QCoreApplication::applicationDirPath()));
#elif defined (Q_OS_LINUX)
	FDictsPaths.append("/usr/share/hunspell");
	FDictsPaths.append("/usr/share/myspell");
#elif defined (Q_OS_MAC)
	FDictsPaths.append(QString("%1/Library/Spelling").arg(QDir::homePath()));
#endif
}

HunspellChecker::~HunspellChecker()
{
	delete FHunSpell;
}

bool HunspellChecker::available() const
{
	return FHunSpell!=NULL;
}

bool HunspellChecker::writable() const
{
	return !FPersonalDictPath.isEmpty();
}

QString HunspellChecker::actuallLang()
{
	return FActualLang;
}

void HunspellChecker::setLang(const QString &ALang)
{
	if (FActualLang != ALang)
	{
		FActualLang = ALang;
		loadHunspell(FActualLang);
	}
}
QList<QString> HunspellChecker::dictionaries()
{
	QList<QString> availDicts;
	foreach(const QString &dictsPath, FDictsPaths)
	{
		QDir dir(dictsPath);
		foreach(QString dictFile, dir.entryList(QStringList("*.dic"),QDir::Files|QDir::Readable, QDir::Name|QDir::IgnoreCase))
		{
			if (dictFile.startsWith("hyph_"))
				continue;
			dictFile = dictFile.mid(0,dictFile.length()-4);
			if (!availDicts.contains(dictFile))
				availDicts.append(dictFile);
		}
	}
	return availDicts;
}

void HunspellChecker::setCustomDictPath(const QString &APath)
{
	Q_UNUSED(APath);
}

void HunspellChecker::setPersonalDictPath(const QString &APath)
{
	FPersonalDictPath = APath;
}

bool HunspellChecker::isCorrect(const QString &AWord)
{
	if(available())
	{
		QByteArray encWord = dictEncoder(FDictEncoding).encode(AWord);
		return FHunSpell->spell(std::string(encWord.constData()));
	}
	return true;
}

bool HunspellChecker::canAdd(const QString &AWord)
{
	QString trimmedWord = AWord.trimmed();
	if (writable() && !trimmedWord.isEmpty())
	{
		QStringEncoder encoder = dictEncoder(FDictEncoding);
		const QByteArray encoded = encoder.encode(trimmedWord);
		Q_UNUSED(encoded);
		return !encoder.hasError();
	}
	return false;
}

bool HunspellChecker::add(const QString &AWord)
{
	if (available() && canAdd(AWord))
	{
		QString trimmedWord = AWord.trimmed();
		QByteArray encWord = dictEncoder(FDictEncoding).encode(trimmedWord);
		FHunSpell->add(encWord.constData());
		savePersonalDict(trimmedWord);
		return true;
	}
	return false;
}

QList<QString> HunspellChecker::suggestions(const QString &AWord)
{
	QList<QString> words;
	if(available())
	{
		QByteArray encWord = dictEncoder(FDictEncoding).encode(AWord);
		const std::vector<std::string> suggestions = FHunSpell->suggest(std::string(encWord.constData()));
		QStringDecoder decoder = dictDecoder(FDictEncoding);
		for (const std::string &suggestion : suggestions)
			words.append(decoder.decode(suggestion.c_str()));
	}
	return words;
}

void HunspellChecker::loadHunspell(const QString &ALang)
{
	delete FHunSpell;
	FHunSpell = NULL;

	foreach(const QString &dictsPath, FDictsPaths)
	{
		QString dictFile = QString("%1/%2.dic").arg(dictsPath).arg(ALang);
		if (QFileInfo(dictFile).exists())
		{
			QString rulesFile = QString("%1/%2.aff").arg(dictsPath).arg(ALang);
			FHunSpell = new Hunspell(rulesFile.toLocal8Bit().constData(), dictFile.toLocal8Bit().constData());
			const char *encoding = FHunSpell->get_dic_encoding();
			QStringEncoder encoder(encoding);
			FDictEncoding = encoder.isValid() ? QByteArray(encoding) : QByteArray();
			loadPersonalDict();
			break;
		}
	}
}

void HunspellChecker::loadPersonalDict()
{
	if (available() && !FPersonalDictPath.isEmpty())
	{
		QDir dictDir(FPersonalDictPath);
		QFile file(dictDir.absoluteFilePath(PERSONAL_DICT_FILENAME));
		if (file.open(QIODevice::ReadOnly|QIODevice::Text))
		{
			while (!file.atEnd())
			{
				QString word = QString::fromUtf8(file.readLine()).trimmed();
				if (canAdd(word))
				{
					QByteArray encWord = dictEncoder(FDictEncoding).encode(word);
					FHunSpell->add(encWord.constData());
				}
			}
		}
		else if (file.exists())
		{
			REPORT_ERROR(QString("Failed to load personal dictionary from file: %1").arg(file.errorString()));
		}
	}
}

void HunspellChecker::savePersonalDict(const QString &AWord)
{
	if (!FPersonalDictPath.isEmpty() && !AWord.isEmpty())
	{
		QDir dictDir(FPersonalDictPath);
		QFile file(dictDir.absoluteFilePath(PERSONAL_DICT_FILENAME));
		if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		{
			file.write(AWord.toUtf8());
			file.write("\n");
			file.close();
		}
		else
		{
			REPORT_ERROR(QString("Failed to save personal dictionary to file: %1").arg(file.errorString()));
		}
	}
}
