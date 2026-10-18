#include <gtest/gtest.h>

#include <QSet>
#include <QUrlQuery>

#include "lute_controller.hpp"
#include "lute_status.hpp"

using namespace lute;

TEST(LuteStatusHighlight, KnownStatusesAreNotDrawn)
{
    EXPECT_FALSE(highlightColor(StatusIgnored).isValid());
    EXPECT_FALSE(highlightColor(StatusWellKnown).isValid());
}

TEST(LuteStatusHighlight, LearningColorsAreDistinctAndNotGreen)
{
    QSet<QString> colors;
    for(int status = StatusUnknown; status <= StatusLearned5; ++status)
    {
        const QColor color = highlightColor(status);
        ASSERT_TRUE(color.isValid()) << status;
        colors.insert(color.name());
        EXPECT_LT(color.green(), qMax(color.red(), color.blue())) << status;
    }
    EXPECT_EQ(colors.size(), 6);

    const QColor learned = highlightColor(StatusLearned5);
    EXPECT_GT(learned.blue(), learned.red());
    EXPECT_GT(learned.blue(), learned.green());
}

TEST(LuteStatusHighlight, GoogleTranslateUrlEncodesTextAndLanguage)
{
    const QUrl url = LuteController::googleTranslateUrl(
        QStringLiteral("Ostatnie życzenie"), QStringLiteral("Polish"));
    const QUrlQuery query(url);
    EXPECT_EQ(query.queryItemValue(QStringLiteral("tl")), QStringLiteral("pl"));
    EXPECT_EQ(query.queryItemValue(QStringLiteral("text")),
              QStringLiteral("Ostatnie życzenie"));
    EXPECT_EQ(query.queryItemValue(QStringLiteral("sl")), QStringLiteral("auto"));
}
