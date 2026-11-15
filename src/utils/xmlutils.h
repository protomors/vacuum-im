#ifndef XMLUTILS_H
#define XMLUTILS_H

#include <QDomDocument>

// Keep namespace processing and parse diagnostics consistent across Qt versions.
template <typename Source>
inline bool setDomContent(QDomDocument &document, const Source &source, QString *errorMessage = nullptr)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto result = document.setContent(source, QDomDocument::ParseOption::UseNamespaceProcessing);
    if (errorMessage)
        *errorMessage = result.errorMessage;
    return bool(result);
#else
    return document.setContent(source, true, errorMessage);
#endif
}

#endif // XMLUTILS_H
