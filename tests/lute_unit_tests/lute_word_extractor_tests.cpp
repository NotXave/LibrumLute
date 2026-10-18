#include <gtest/gtest.h>

#include <QFileInfo>

#include "lute_word_extractor.hpp"

using namespace lute;

TEST(LuteWordExtractor, ProducesPerLinePartsAndBaselines)
{
    const QString path = QStringLiteral(LUTE_TEST_DATA_DIR "/sample_en.pdf");
    ASSERT_TRUE(QFileInfo::exists(path)) << path.toStdString();

    LuteWordExtractor extractor;
    QString error;
    ASSERT_TRUE(extractor.open(path, &error)) << error.toStdString();
    ASSERT_GT(extractor.pageCount(), 0);

    const QVector<WordQuad>& words = extractor.words(0);
    ASSERT_FALSE(words.isEmpty());

    for(const WordQuad& word : words)
    {
        EXPECT_TRUE(word.rect.isValid()) << word.word.toStdString();
        EXPECT_FALSE(word.parts.isEmpty()) << word.word.toStdString();

        QRectF united;
        for(const WordPart& part : word.parts)
        {
            EXPECT_GE(part.rect.left(), -0.01);
            EXPECT_GE(part.rect.top(), -0.01);
            EXPECT_LE(part.rect.right(), 1.01);
            EXPECT_LE(part.rect.bottom(), 1.01);
            EXPECT_GE(part.baseline, 0.0);
            EXPECT_LE(part.baseline, 1.0);
            united = united.isNull() ? part.rect : united.united(part.rect);
        }

        ASSERT_FALSE(united.isNull());
        EXPECT_NEAR(united.left(), word.rect.left(), 0.002);
        EXPECT_NEAR(united.top(), word.rect.top(), 0.002);
        EXPECT_NEAR(united.right(), word.rect.right(), 0.002);
        EXPECT_NEAR(united.bottom(), word.rect.bottom(), 0.002);
    }
}
