#ifndef XMLUTILS_H
#define XMLUTILS_H

#include <QDomDocument>

// Parse namespace-aware XML and expose diagnostics to callers.
template <typename Source>
inline bool setDomContent(QDomDocument &document, const Source &source, QString *errorMessage = nullptr)
{
    const auto result = document.setContent(source, QDomDocument::ParseOption::UseNamespaceProcessing);
    if (errorMessage)
        *errorMessage = result.errorMessage;
    return bool(result);
}

#endif // XMLUTILS_H
