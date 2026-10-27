#pragma once

#include <QRectF>
#include <QString>
#include <QVector>
#include "lute_term.hpp"
#include "lute_text_parser.hpp"
#include "word_quad.hpp"

namespace lute
{

/// One occurrence of a multi-word term on a page.
struct PhraseMatch
{
    qint64 termId = 0;
    QString text;
    int firstIndex = -1;
    int lastIndex = -1;
    QRectF boundingRect;
    int tokenCount = 0;

    bool contains(int wordIndex) const
    {
        return wordIndex >= firstIndex && wordIndex <= lastIndex;
    }
};

/// Matches Lute's multi-word terms ("run out of", "give up", ...) against the
/// word sequence of a PDF page.
///
/// Matching is greedy longest-first, case-insensitive, and never re-uses a
/// token for two phrases, which is how Lute highlights phrases inside a text.
class PhraseMatcher
{
public:
    static QVector<PhraseMatch> findMatches(const QVector<WordQuad>& pageWords,
                                            const QVector<LuteTerm>& terms,
                                            const LuteTextParser& parser);

    /// The match covering `wordIndex`, preferring the longest phrase.
    static const PhraseMatch* matchAt(const QVector<PhraseMatch>& matches,
                                      int wordIndex);
};

}  // namespace lute
