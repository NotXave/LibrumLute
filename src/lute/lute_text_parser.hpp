#pragma once

#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QVector>
#include "lute_language.hpp"

namespace lute
{

/// A word occurrence inside a run of text (offsets are UTF-16 positions).
struct ParsedWord
{
    QString text;
    int start = 0;
    int end = 0;
};

/// Lute-compatible text tokenizer and sentence splitter.
///
/// This mirrors Lute v3's `SpaceDelimitedParser` closely enough that the same
/// language settings (`LgRegexpWordCharacters`, `LgCharacterSubstitutions`,
/// `LgRegexpSplitSentences`, `LgExceptionsSplitSentences`) produce the same
/// terms, which is required for lookups in a shared `lute.db` to hit.
class LuteTextParser
{
public:
    LuteTextParser() = default;
    explicit LuteTextParser(const LuteLanguage& language);

    const LuteLanguage& language() const
    {
        return m_language;
    }

    void setLanguage(const LuteLanguage& language);

    /// Word character class body (the inside of `[...]`), e.g. `a-zA-ZÀ-Ö...`.
    QString wordCharacterClass() const
    {
        return m_wordClass;
    }

    /// All words in `text`, in reading order.
    QVector<ParsedWord> words(const QString& text) const;

    /// Half-open [start, end) character ranges of the sentences in `text`.
    QVector<QPair<int, int>> sentenceRanges(const QString& text) const;

    /// The full sentence containing `charOffset`, trimmed. Falls back to the
    /// surrounding line when no sentence boundary can be found.
    QString sentenceAt(const QString& text, int charOffset) const;

    /// Case-folding used for `words.WoTextLC` (Turkish aware, like Lute).
    QString lowercase(const QString& text) const;

    /// Applies `LgCharacterSubstitutions` to a piece of text.
    QString applySubstitutions(const QString& text) const;

    /// Expands literal `\uXXXX` / `\UXXXXXXXX` escapes that Lute stores in its
    /// regexp columns into real characters (PCRE uses `\x{...}` instead).
    static QString expandUnicodeEscapes(const QString& pattern);

    /// True if `ch` counts as a word character for the current language.
    bool isWordCharacter(QChar ch) const;

    /// True if `ch` is one of the language's sentence-ending characters.
    bool isSentenceEndCharacter(QChar ch) const;

private:
    void rebuild();
    bool matchesSentenceException(const QString& candidate) const;

    LuteLanguage m_language;
    QString m_wordClass;
    QRegularExpression m_wordRegex;
    QSet<QChar> m_splitCharacters;
    QVector<QRegularExpression> m_exceptionRegexes;
};

}  // namespace lute
