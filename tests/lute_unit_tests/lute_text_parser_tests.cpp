#include <gtest/gtest.h>

#include "lute_test_helpers.hpp"
#include "lute_text_parser.hpp"

using namespace lute;
using lute_test::englishLanguage;

TEST(LuteTextParser, SplitsWordsLikeLute)
{
    const LuteTextParser parser(englishLanguage());

    const QVector<ParsedWord> words =
        parser.words(QStringLiteral("Mr. Smith walked to the harbor."));

    ASSERT_EQ(words.size(), 6);
    EXPECT_EQ(words.at(0).text, QStringLiteral("Mr"));
    EXPECT_EQ(words.at(1).text, QStringLiteral("Smith"));
    EXPECT_EQ(words.at(2).text, QStringLiteral("walked"));
    EXPECT_EQ(words.at(3).text, QStringLiteral("to"));
    EXPECT_EQ(words.at(4).text, QStringLiteral("the"));
    EXPECT_EQ(words.at(5).text, QStringLiteral("harbor"));

    // The offsets must point back into the original text.
    EXPECT_EQ(words.at(5).start, QStringLiteral("Mr. Smith walked to the harbor.")
                                     .indexOf(QStringLiteral("harbor")));
}

TEST(LuteTextParser, KeepsApostrophesInsideWords)
{
    const LuteTextParser parser(englishLanguage());

    const QVector<ParsedWord> words =
        parser.words(QStringLiteral("Don't give up, don't stop."));

    ASSERT_EQ(words.size(), 5);
    EXPECT_EQ(words.at(0).text, QStringLiteral("Don't"));
    EXPECT_EQ(words.at(1).text, QStringLiteral("give"));
    EXPECT_EQ(words.at(2).text, QStringLiteral("up"));
}

TEST(LuteTextParser, FindsTheSentenceAroundAnOffset)
{
    const LuteTextParser parser(englishLanguage());
    const QString text =
        QStringLiteral("First sentence. Second one is longer! Third?");

    EXPECT_EQ(parser.sentenceAt(text, text.indexOf(QStringLiteral("Second"))),
              QStringLiteral("Second one is longer!"));
    EXPECT_EQ(parser.sentenceAt(text, text.indexOf(QStringLiteral("First"))),
              QStringLiteral("First sentence."));
}

TEST(LuteTextParser, SentenceRangesCoverTheText)
{
    const LuteTextParser parser(englishLanguage());
    const QString text = QStringLiteral("One. Two! Three?");

    const QVector<QPair<int, int>> ranges = parser.sentenceRanges(text);
    ASSERT_EQ(ranges.size(), 3);
    for(const auto& range : ranges)
    {
        EXPECT_GE(range.first, 0);
        EXPECT_LE(range.second, text.size());
        EXPECT_LT(range.first, range.second);
    }
}

TEST(LuteTextParser, LowercaseIsTheLookupKey)
{
    const LuteTextParser parser(englishLanguage());

    EXPECT_EQ(parser.lowercase(QStringLiteral("HARBOR")), QStringLiteral("harbor"));
    EXPECT_EQ(parser.lowercase(QStringLiteral("Mr. Smith")),
              QStringLiteral("mr. smith"));
}

TEST(LuteTextParser, AppliesCharacterSubstitutions)
{
    const LuteTextParser parser(englishLanguage());

    // The curly apostrophe is folded to the straight one for lookups.
    EXPECT_EQ(parser.applySubstitutions(QStringLiteral("don\u2019t")),
              QStringLiteral("don't"));
}
