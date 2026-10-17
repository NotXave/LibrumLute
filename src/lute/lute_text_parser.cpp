#include "lute_text_parser.hpp"

#include <QChar>
#include <QList>
#include <QStringList>

#include <algorithm>

namespace lute {
namespace {

const QSet<QChar>& defaultSplitCharacters()
{
    static const QSet<QChar> chars = {QLatin1Char('.'), QLatin1Char('!'), QLatin1Char('?')};
    return chars;
}

/// Lute's default `character_substitutions` value.
const QString& defaultSubstitutions()
{
    static const QString value =
        QStringLiteral("´='|`='|’='|‘='|...=…|..=‥");
    return value;
}

bool isApostropheLike(QChar ch)
{
    return ch == QLatin1Char('\'') || ch == QChar(0x2019) || ch == QChar(0x2018);
}

} // namespace

LuteTextParser::LuteTextParser(const LuteLanguage& language)
{
    setLanguage(language);
}

void LuteTextParser::setLanguage(const LuteLanguage& language)
{
    m_language = language;
    if (m_language.characterSubstitutions.isEmpty())
        m_language.characterSubstitutions = defaultSubstitutions();
    if (m_language.regexpSplitSentences.isEmpty())
        m_language.regexpSplitSentences = QStringLiteral(".!?");
    rebuild();
}

QString LuteTextParser::expandUnicodeEscapes(const QString& pattern)
{
    QString result;
    result.reserve(pattern.size());
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern.at(i);
        if (c == QLatin1Char('\\') && i + 1 < pattern.size()) {
            const QChar next = pattern.at(i + 1);
            const bool longForm = next == QLatin1Char('U');
            if ((next == QLatin1Char('u') || longForm) && i + 1 + (longForm ? 8 : 4) < pattern.size()) {
                const int digits = longForm ? 8 : 4;
                bool ok = false;
                const uint code = pattern.mid(i + 2, digits).toUInt(&ok, 16);
                if (ok && code > 0 && code <= 0x10FFFF) {
                    const char32_t point = char32_t(code);
                    result.append(QString::fromUcs4(&point, 1));
                    i += 1 + digits;
                    continue;
                }
            }
        }
        result.append(c);
    }
    return result;
}

void LuteTextParser::rebuild()
{
    m_wordClass = expandUnicodeEscapes(m_language.regexpWordCharacters).trimmed();
    if (m_wordClass.isEmpty()) {
        // Approximation of Lute's generated default: Unicode letters plus the
        // combining marks and modifier symbols it includes.
        m_wordClass = QStringLiteral("\\p{L}\\p{M}\\p{Sk}\\p{Cf}");
    }

    m_wordRegex = QRegularExpression(QStringLiteral("[%1]+").arg(m_wordClass));
    if (!m_wordRegex.isValid()) {
        qWarning("Invalid LgRegexpWordCharacters '%s' (%s); falling back to letters",
                 qUtf8Printable(m_language.regexpWordCharacters),
                 qUtf8Printable(m_wordRegex.errorString()));
        m_wordRegex = QRegularExpression(QStringLiteral("[\\p{L}\\p{M}]+"));
    }

    m_splitCharacters = defaultSplitCharacters();
    QString splitBody = m_language.regexpSplitSentences.trimmed();
    if (splitBody.startsWith(QLatin1Char('[')) && splitBody.endsWith(QLatin1Char(']')))
        splitBody = splitBody.mid(1, splitBody.size() - 2);
    splitBody = expandUnicodeEscapes(splitBody);
    if (!splitBody.isEmpty()) {
        const QRegularExpression splitRegex(QStringLiteral("[%1]").arg(splitBody));
        if (splitRegex.isValid()) {
            m_splitCharacters.clear();
            for (QChar ch : splitBody) {
                if (ch == QLatin1Char('\\'))
                    continue;
                m_splitCharacters.insert(ch);
            }
            // Keep the structural ASCII enders even if the language only lists
            // exotic ones: Lute always splits on ASCII `.!?`.
            m_splitCharacters.insert(QLatin1Char('.'));
        }
    }

    m_exceptionRegexes.clear();
    QString exceptions = m_language.exceptionsSplitSentences.trimmed();
    if (exceptions.size() >= 2
        && ((exceptions.startsWith(QLatin1Char('\'')) && exceptions.endsWith(QLatin1Char('\'')))
            || (exceptions.startsWith(QLatin1Char('"')) && exceptions.endsWith(QLatin1Char('"'))))) {
        exceptions = exceptions.mid(1, exceptions.size() - 2);
    }
    const QStringList parts = exceptions.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        QString pattern = part.trimmed();
        if (pattern.isEmpty())
            continue;
        // Lute escapes the dots of the exception list before compiling it.
        pattern.replace(QLatin1Char('.'), QStringLiteral("\\."));
        QRegularExpression re(QStringLiteral("^(?:%1)$").arg(pattern),
                              QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::UseUnicodePropertiesOption);
        if (re.isValid())
            m_exceptionRegexes.append(re);
    }
}

bool LuteTextParser::isWordCharacter(QChar ch) const
{
    return m_wordRegex.match(QString(ch)).hasMatch();
}

bool LuteTextParser::isSentenceEndCharacter(QChar ch) const
{
    return m_splitCharacters.contains(ch);
}

QString LuteTextParser::applySubstitutions(const QString& text) const
{
    QString result = text;
    const QStringList replacements = m_language.characterSubstitutions.split(QLatin1Char('|'));
    for (const QString& replacement : replacements) {
        const QStringList fromTo = replacement.trimmed().split(QLatin1Char('='));
        if (fromTo.size() >= 2)
            result.replace(fromTo.at(0).trimmed(), fromTo.at(1).trimmed());
    }
    result.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    return result;
}

QString LuteTextParser::lowercase(const QString& text) const
{
    if (m_language.name.compare(QLatin1String("Turkish"), Qt::CaseInsensitive) == 0) {
        QString turkish = text;
        turkish.replace(QChar(0x0130), QLatin1Char('i')); // İ
        turkish.replace(QLatin1Char('I'), QChar(0x0131)); // I -> ı
        return turkish.toLower();
    }
    return text.toLower();
}

QVector<ParsedWord> LuteTextParser::words(const QString& text) const
{
    QVector<ParsedWord> result;
    auto it = m_wordRegex.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        ParsedWord word;
        word.text = match.captured(0);
        word.start = match.capturedStart(0);
        word.end = match.capturedEnd(0);
        result.append(word);
    }
    return result;
}

bool LuteTextParser::matchesSentenceException(const QString& candidate) const
{
    if (candidate.isEmpty())
        return false;
    for (const QRegularExpression& re : m_exceptionRegexes) {
        if (re.match(candidate).hasMatch())
            return true;
    }
    return false;
}

QVector<QPair<int, int>> LuteTextParser::sentenceRanges(const QString& text) const
{
    QVector<QPair<int, int>> ranges;
    const int n = text.size();
    if (n == 0)
        return ranges;

    auto isCloser = [](QChar ch) {
        return ch == QLatin1Char('"') || ch == QLatin1Char('\'') || ch == QChar(0x2019)
            || ch == QChar(0x201D) || ch == QChar(0x00BB) || ch == QLatin1Char(')')
            || ch == QLatin1Char(']') || ch == QLatin1Char('}');
    };

    int sentenceStart = 0;
    int i = 0;
    while (i < n) {
        const QChar ch = text.at(i);

        // A blank line (paragraph break from the PDF layout) always ends a
        // sentence, even if the paragraph has no final punctuation.
        if (ch == QLatin1Char('\n') && i + 1 < n && text.at(i + 1) == QLatin1Char('\n')) {
            int end = i;
            while (end > sentenceStart && text.at(end - 1).isSpace())
                --end;
            if (end > sentenceStart)
                ranges.append({sentenceStart, end});
            sentenceStart = i + 2;
            i += 2;
            continue;
        }

        if (!m_splitCharacters.contains(ch)) {
            ++i;
            continue;
        }

        // "3.14", "U.S.", "e.g." -> the character is inside a word.
        const bool nextIsCharacter = (i + 1 < n)
            && (text.at(i + 1).isLetterOrNumber() || isWordCharacter(text.at(i + 1)));
        if (nextIsCharacter) {
            ++i;
            continue;
        }

        // Collect the token that ends with this punctuation mark, e.g. "Mr."
        int back = i - 1;
        while (back >= 0 && (isWordCharacter(text.at(back)) || text.at(back) == QLatin1Char('.')
                             || isApostropheLike(text.at(back)))) {
            --back;
        }
        const QString precedingToken = text.mid(back + 1, i - back).simplified();
        if (matchesSentenceException(precedingToken)) {
            ++i;
            continue;
        }
        // A single uppercase letter followed by a dot is almost always an
        // initial ("J. R. R. Tolkien"), never a sentence end.
        if (precedingToken.size() == 2 && precedingToken.at(1) == QLatin1Char('.')
            && precedingToken.at(0).isUpper() && precedingToken.at(0).isLetter()) {
            ++i;
            continue;
        }

        int end = i + 1;
        while (end < n && isCloser(text.at(end)))
            ++end;
        while (end < n && text.at(end).isSpace())
            ++end;
        ranges.append({sentenceStart, qMin(end, n)});
        sentenceStart = end;
        i = end;
    }

    if (sentenceStart < n) {
        int end = n;
        while (end > sentenceStart && text.at(end - 1).isSpace())
            --end;
        ranges.append({sentenceStart, end});
    }
    return ranges;
}

QString LuteTextParser::sentenceAt(const QString& text, int charOffset) const
{
    if (text.isEmpty())
        return {};
    const int offset = qBound(0, charOffset, text.size() - 1);
    const auto ranges = sentenceRanges(text);
    for (const auto& range : ranges) {
        if (offset >= range.first && offset < range.second) {
            QString sentence = text.mid(range.first, range.second - range.first).simplified();
            if (sentence.size() > 600) {
                // Keep the context window sane for the AI prompt.
                const int relative = offset - range.first;
                const int start = qMax(0, relative - 250);
                sentence = text.mid(range.first + start, 500).simplified();
            }
            return sentence;
        }
    }
    return text.mid(0, qMin(text.size(), 300)).simplified();
}

} // namespace lute
