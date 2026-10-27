#pragma once

#include <QString>

namespace lute
{

/// A row of `lute.db`'s `languages` table.
///
/// The three regexp-ish columns are used by Lute's text parser and are honoured
/// here as well so that a word is tokenised exactly like Lute would tokenise
/// it.
struct LuteLanguage
{
    int id = 0;
    QString name;
    QString characterSubstitutions;
    QString regexpSplitSentences;
    QString exceptionsSplitSentences;
    QString regexpWordCharacters;
    bool rightToLeft = false;
    bool showRomanization = false;
    QString parserType = QStringLiteral("spacedel");

    bool isValid() const
    {
        return id > 0;
    }

    /// Lute parser types this application can tokenise ("space delimited"
    /// ones).
    static bool isSupportedParserType(const QString& type)
    {
        return type == QLatin1String("spacedel") ||
               type == QLatin1String("spaceormark") ||
               type == QLatin1String("generic");
    }
};

}  // namespace lute
