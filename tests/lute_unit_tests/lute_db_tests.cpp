#include <gtest/gtest.h>

#include <QTemporaryDir>

#include "lute_db_access.hpp"
#include "lute_schema.hpp"

using namespace lute;

namespace
{

QString createTemplateDatabase(QTemporaryDir& directory)
{
    const QString path = directory.filePath(QStringLiteral("lute.db"));
    QString error;
    EXPECT_TRUE(LuteSchema::createDatabase(path, &error)) << error.toStdString();
    return path;
}

} // namespace

TEST(LuteDatabase, CreatesTheOfficialTemplate)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const QString path = createTemplateDatabase(directory);
    ASSERT_TRUE(LuteSchema::isLuteDatabase(path));

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();

    const QVector<LuteLanguage> languages = database.languages();
    EXPECT_GT(languages.size(), 50);

    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());
    EXPECT_TRUE(english->isValid());
    EXPECT_TRUE(LuteLanguage::isSupportedParserType(english->parserType));
}

TEST(LuteDatabase, SavesReadsAndUpdatesATerm)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    LuteTerm term;
    term.languageId = english->id;
    term.text = QStringLiteral("harbor");
    term.status = 3;
    term.translation = QStringLiteral("Hafen");
    term.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(term, &error)) << error.toStdString();
    EXPECT_GT(term.id, 0);

    // Case-insensitive lookup, like Lute's WoTextLC key.
    const auto loaded = database.getTermByText(QStringLiteral("HARBOR"), english->id);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->status, 3);
    EXPECT_EQ(loaded->translation, QStringLiteral("Hafen"));
    EXPECT_EQ(loaded->textLC, QStringLiteral("harbor"));

    ASSERT_TRUE(database.updateStatus(loaded->id, 5, &error));
    const auto updated = database.getTermByText(QStringLiteral("harbor"), english->id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->status, 5);

    const QHash<int, int> counts = database.statusCounts(english->id);
    EXPECT_EQ(counts.value(5), 1);
    EXPECT_EQ(counts.value(3), 0);

    const QHash<QString, int> pageStatuses =
        database.getTermsForPage({QStringLiteral("harbor"), QStringLiteral("lighthouse")},
                                 english->id);
    const int harborStatus = pageStatuses.value(QStringLiteral("harbor"), 0);
    const int lighthouseStatus =
        pageStatuses.value(QStringLiteral("lighthouse"), 0);
    EXPECT_EQ(harborStatus, 5);
    EXPECT_EQ(lighthouseStatus, 0);
}

TEST(LuteDatabase, StoresPhrasesAsMultiWordTerms)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    LuteTerm phrase;
    phrase.languageId = english->id;
    phrase.text = QStringLiteral("run out of");
    phrase.status = 3;
    phrase.tokenCount = 3;
    ASSERT_TRUE(database.saveTerm(phrase, &error)) << error.toStdString();

    const QVector<LuteTerm> phrases = database.multiWordTerms(english->id);
    ASSERT_EQ(phrases.size(), 1);
    EXPECT_EQ(phrases.first().text, QStringLiteral("run out of"));
    EXPECT_EQ(phrases.first().tokenCount, 3);
    EXPECT_EQ(phrases.first().status, 3);
}

TEST(LuteDatabase, RecordsReadingStatistics)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    EXPECT_TRUE(database.recordWordsRead(english->id, 120, 0));
    EXPECT_TRUE(database.recordWordsRead(english->id, 30, 0));
    EXPECT_EQ(database.wordsReadTotal(english->id), 150);
}

TEST(LuteDatabase, CreatesAndFindsBooksAndTexts)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    const qint64 bookId = database.findOrCreateBook(
        QStringLiteral("The Quiet Harbor"), QStringLiteral("file:///tmp/book.pdf"),
        english->id, &error);
    ASSERT_GT(bookId, 0) << error.toStdString();

    // Calling it twice must not create a second book.
    EXPECT_EQ(database.findOrCreateBook(QStringLiteral("The Quiet Harbor"),
                                       QStringLiteral("file:///tmp/book.pdf"),
                                       english->id, &error),
              bookId);
    EXPECT_FALSE(database.findTextForBook(bookId).has_value());

    const qint64 textId = database.createText(
        bookId, 1, QStringLiteral("A short text about a harbor."), 6, &error);
    ASSERT_GT(textId, 0) << error.toStdString();
    EXPECT_TRUE(database.findTextForBook(bookId).has_value());
}
