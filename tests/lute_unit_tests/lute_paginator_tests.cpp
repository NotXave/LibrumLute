#include <gtest/gtest.h>

#include "lute_paginator.hpp"
#include "lute_test_helpers.hpp"
#include "lute_text_parser.hpp"

using namespace lute;
using lute_test::englishLanguage;

TEST(LutePaginator, SplitsAtTheTokenThreshold)
{
    const LuteTextParser parser(englishLanguage());
    const QVector<QString> pages = LutePaginator::paginate(
        QStringLiteral("one two three four five six seven"), parser, 3, false);

    ASSERT_EQ(pages.size(), 3);
    EXPECT_EQ(pages.at(0), QStringLiteral("one two three"));
    EXPECT_EQ(pages.at(1), QStringLiteral("four five six"));
    EXPECT_EQ(pages.at(2), QStringLiteral("seven"));
}

TEST(LutePaginator, AlwaysBreaksAtAPageMarker)
{
    const LuteTextParser parser(englishLanguage());
    const QVector<QString> pages = LutePaginator::paginate(
        QStringLiteral("first page text\n\n---\n\nsecond page text"),
        parser, 250, false);

    ASSERT_EQ(pages.size(), 2);
    EXPECT_EQ(pages.at(0), QStringLiteral("first page text"));
    EXPECT_EQ(pages.at(1), QStringLiteral("second page text"));
}

TEST(LutePaginator, IgnoresEmptyText)
{
    const LuteTextParser parser(englishLanguage());
    EXPECT_TRUE(LutePaginator::paginate(QStringLiteral("   \n\n  "), parser)
                    .isEmpty());
}

TEST(LutePaginator, CanExtendToTheSentenceEnd)
{
    const LuteTextParser parser(englishLanguage());
    const QVector<QString> pages = LutePaginator::paginate(
        QStringLiteral("One two. Three four. Five six."), parser, 2, true);

    ASSERT_GE(pages.size(), 1);
    EXPECT_EQ(pages.at(0), QStringLiteral("One two."));
}
