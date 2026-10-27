#include "phrase_matcher.hpp"
#include <algorithm>

namespace lute
{

QVector<PhraseMatch> PhraseMatcher::findMatches(
    const QVector<WordQuad>& pageWords, const QVector<LuteTerm>& terms,
    const LuteTextParser& parser)
{
    QVector<PhraseMatch> result;
    if(pageWords.isEmpty() || terms.isEmpty())
        return result;

    QStringList lowerWords;
    lowerWords.reserve(pageWords.size());
    for(const WordQuad& word : pageWords)
        lowerWords.append(parser.lowercase(word.word));

    struct Candidate
    {
        PhraseMatch match;
        int length = 0;
    };

    QVector<Candidate> candidates;

    for(const LuteTerm& term : terms)
    {
        if(term.tokenCount <= 1)
            continue;
        const QStringList rawTokens =
            term.text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if(rawTokens.size() <= 1)
            continue;
        QStringList tokens;
        tokens.reserve(rawTokens.size());
        for(const QString& token : rawTokens)
            tokens.append(parser.lowercase(token));

        for(int start = 0; start + tokens.size() <= lowerWords.size(); ++start)
        {
            bool matched = true;
            for(int offset = 0; offset < tokens.size(); ++offset)
            {
                if(lowerWords.at(start + offset) != tokens.at(offset))
                {
                    matched = false;
                    break;
                }
            }
            if(!matched)
                continue;

            Candidate candidate;
            candidate.length = tokens.size();
            candidate.match.termId = term.id;
            candidate.match.text = term.text;
            candidate.match.firstIndex = pageWords.at(start).index;
            candidate.match.lastIndex =
                pageWords.at(start + tokens.size() - 1).index;
            candidate.match.tokenCount = tokens.size();
            QRectF box;
            for(int offset = 0; offset < tokens.size(); ++offset)
            {
                const QRectF& wordRect = pageWords.at(start + offset).rect;
                box = box.isNull() ? wordRect : box.united(wordRect);
            }
            candidate.match.boundingRect = box;
            candidates.append(candidate);
        }
    }

    // Longest phrases win; earlier occurrences win ties.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b)
              {
                  if(a.length != b.length)
                      return a.length > b.length;
                  return a.match.firstIndex < b.match.firstIndex;
              });

    QVector<bool> consumed(pageWords.size(), false);
    for(const Candidate& candidate : candidates)
    {
        bool free_ = true;
        for(int i = candidate.match.firstIndex; i <= candidate.match.lastIndex;
            ++i)
        {
            if(i >= 0 && i < consumed.size() && consumed.at(i))
            {
                free_ = false;
                break;
            }
        }
        if(!free_)
            continue;
        for(int i = candidate.match.firstIndex; i <= candidate.match.lastIndex;
            ++i)
        {
            if(i >= 0 && i < consumed.size())
                consumed[i] = true;
        }
        result.append(candidate.match);
    }

    std::sort(result.begin(), result.end(),
              [](const PhraseMatch& a, const PhraseMatch& b)
              {
                  return a.firstIndex < b.firstIndex;
              });
    return result;
}

const PhraseMatch* PhraseMatcher::matchAt(const QVector<PhraseMatch>& matches,
                                          int wordIndex)
{
    const PhraseMatch* best = nullptr;
    for(const PhraseMatch& match : matches)
    {
        if(!match.contains(wordIndex))
            continue;
        if(!best || match.tokenCount > best->tokenCount)
            best = &match;
    }
    return best;
}

}  // namespace lute
