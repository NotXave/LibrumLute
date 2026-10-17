#include "lute_paginator.hpp"

#include <QRegularExpression>
#include <QStringList>

#include "lute_text_parser.hpp"

namespace lute
{

QVector<QString> LutePaginator::paginate(const QString& text,
                                         const LuteTextParser& parser,
                                         int threshold, bool splitBySentences)
{
    QVector<QString> pages;
    if(text.trimmed().isEmpty())
        return pages;
    if(threshold < 1)
        threshold = 1;

    // Lute treats a line with only "---" as a hard page break.
    const QRegularExpression pageBreak(
        QStringLiteral("(?m)^[\\t ]*---[\\t ]*$"));
    const QStringList segments = text.split(pageBreak);

    for(const QString& segment : segments)
    {
        const QVector<ParsedWord> tokens = parser.words(segment);
        if(tokens.isEmpty())
            continue;

        const int firstPageOfSegment = pages.size();
        int pageStart = 0;
        int tokenIndex = 0;
        while(tokenIndex < tokens.size())
        {
            int count = 0;
            while(tokenIndex < tokens.size() && count < threshold)
            {
                ++tokenIndex;
                ++count;
            }

            int end = tokens.at(tokenIndex - 1).end;
            if(splitBySentences && tokenIndex < tokens.size())
            {
                // Extend to the end of the sentence that contains the
                // threshold token.
                while(tokenIndex < tokens.size())
                {
                    const int gapStart = tokens.at(tokenIndex - 1).end;
                    const int gapEnd = tokens.at(tokenIndex).start;
                    const QString gap = segment.mid(gapStart, gapEnd - gapStart);
                    bool sentenceEnded = false;
                    for(const QChar character : gap)
                    {
                        if(parser.isSentenceEndCharacter(character))
                        {
                            sentenceEnded = true;
                            break;
                        }
                    }
                    if(sentenceEnded)
                    {
                        // Keep the sentence punctuation on the page.
                        end = tokens.at(tokenIndex).start;
                        break;
                    }
                    end = tokens.at(tokenIndex).end;
                    ++tokenIndex;
                }
            }

            const QString page = segment.mid(pageStart, end - pageStart).trimmed();
            if(!page.isEmpty())
                pages.append(page);
            pageStart = end;
        }

        if(pageStart < segment.size())
        {
            // The remainder is normally the punctuation after the last word;
            // keep it on the current page instead of creating a "." page.
            const QString tail = segment.mid(pageStart);
            if(pages.size() > firstPageOfSegment)
                pages.last() = (pages.last() + tail).trimmed();
            else if(!tail.trimmed().isEmpty())
                pages.append(tail.trimmed());
        }
    }

    return pages;
}

} // namespace lute
