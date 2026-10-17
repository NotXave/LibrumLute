#include <gtest/gtest.h>

#include "lute_test_helpers.hpp"
#include "lute_text_parser.hpp"
#include "phrase_matcher.hpp"

using namespace lute;
using lute_test::englishLanguage;
using lute_test::makeWord;

namespace
{

QVector<WordQuad> sentenceWords()
{
    return {makeWord(QStringLiteral("Ellen"), 0, 0.10),
            makeWord(QStringLiteral("had"), 1, 0.16),
            makeWord(QStringLiteral("run"), 2, 0.20),
            makeWord(QStringLiteral("out"), 3, 0.24),
            makeWord(QStringLiteral("of"), 4, 0.28),
            makeWord(QStringLiteral("patience"), 5, 0.31)};
}

LuteTerm phraseTerm()
{
    LuteTerm term;
    term.id = 7;
    term.languageId = 21;
    term.text = QStringLiteral("run out of");
    term.tokenCount = 3;
    term.status = 3;
    return term;
}

} // namespace

TEST(PhraseMatcher, FindsAPhraseOnAPage)
{
    const LuteTextParser parser(englishLanguage());
    const QVector<PhraseMatch> matches =
        PhraseMatcher::findMatches(sentenceWords(), {phraseTerm()}, parser);

    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches.first().termId, 7);
    EXPECT_EQ(matches.first().firstIndex, 2);
    EXPECT_EQ(matches.first().lastIndex, 4);
    EXPECT_EQ(matches.first().tokenCount, 3);
    EXPECT_FALSE(matches.first().boundingRect.isNull());
}

TEST(PhraseMatcher, FindsThePhraseForAnyOfItsWords)
{
    const LuteTextParser parser(englishLanguage());
    const QVector<PhraseMatch> matches =
        PhraseMatcher::findMatches(sentenceWords(), {phraseTerm()}, parser);

    ASSERT_EQ(matches.size(), 1);
    for(int index = 2; index <= 4; ++index)
    {
        const PhraseMatch* match = PhraseMatcher::matchAt(matches, index);
        ASSERT_NE(match, nullptr) << index;
        EXPECT_EQ(match->firstIndex, 2);
        EXPECT_EQ(match->lastIndex, 4);
    }

    EXPECT_EQ(PhraseMatcher::matchAt(matches, 0), nullptr);
    EXPECT_EQ(PhraseMatcher::matchAt(matches, 5), nullptr);
}

TEST(PhraseMatcher, IgnoresSingleWordTerms)
{
    const LuteTextParser parser(englishLanguage());
    LuteTerm term = phraseTerm();
    term.tokenCount = 1;

    const QVector<PhraseMatch> matches =
        PhraseMatcher::findMatches(sentenceWords(), {term}, parser);
    EXPECT_TRUE(matches.isEmpty());
}

TEST(PhraseMatcher, DoesNotMatchAcrossMissingWords)
{
    const LuteTextParser parser(englishLanguage());
    QVector<WordQuad> words = sentenceWords();
    words.removeAt(3); // remove "out"

    const QVector<PhraseMatch> matches =
        PhraseMatcher::findMatches(words, {phraseTerm()}, parser);
    EXPECT_TRUE(matches.isEmpty());
}

TEST(PhraseMatcher, PrefersTheLongerPhrase)
{
    const LuteTextParser parser(englishLanguage());

    LuteTerm shorter;
    shorter.id = 1;
    shorter.text = QStringLiteral("out of");
    shorter.tokenCount = 2;
    shorter.status = 3;

    LuteTerm longer = phraseTerm();
    longer.id = 2;

    const QVector<PhraseMatch> matches =
        PhraseMatcher::findMatches(sentenceWords(), {shorter, longer}, parser);
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches.first().termId, 2);
    EXPECT_EQ(matches.first().tokenCount, 3);
}
