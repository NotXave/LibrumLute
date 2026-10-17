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


TEST(LuteDatabase, ListsBooksAndReadsTextPages)
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
        QStringLiteral("A book"), QStringLiteral("file:///tmp/a.pdf"),
        english->id, &error);
    ASSERT_GT(bookId, 0) << error.toStdString();
    ASSERT_GT(database.createText(bookId, 1, QStringLiteral("Page one."), 2,
                                  &error),
              0);
    const qint64 secondPage = database.createText(
        bookId, 2, QStringLiteral("Page two."), 2, &error);
    ASSERT_GT(secondPage, 0);

    const QVector<LuteBook> books = database.books(english->id, true);
    ASSERT_EQ(books.size(), 1);
    EXPECT_EQ(books.first().title, QStringLiteral("A book"));
    EXPECT_EQ(books.first().pageCount, 2);
    EXPECT_EQ(books.first().currentPage, 1);

    const QVector<LuteText> texts = database.texts(bookId);
    ASSERT_EQ(texts.size(), 2);
    EXPECT_EQ(texts.first().order, 1);
    EXPECT_FALSE(texts.first().read);

    const auto second = database.text(bookId, 2);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->text, QStringLiteral("Page two."));

    ASSERT_TRUE(database.setBookCurrentPage(bookId, 2));
    const auto updated = database.book(bookId);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->currentPage, 2);

    ASSERT_TRUE(database.markTextRead(second->id));
    const QVector<LuteText> after = database.texts(bookId);
    ASSERT_EQ(after.size(), 2);
    EXPECT_TRUE(after.at(1).read);
}


TEST(LuteDatabase, SavesUpdatesAndGuardsLanguageDeletion)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();

    LuteLanguage language;
    language.name = QStringLiteral("Testish");
    language.regexpSplitSentences = QStringLiteral(".!?");
    language.regexpWordCharacters = QStringLiteral("a-z");
    language.parserType = QStringLiteral("spacedel");
    language.showRomanization = true;
    ASSERT_TRUE(database.saveLanguage(language, &error)) << error.toStdString();
    EXPECT_GT(language.id, 0);

    const auto created = database.languageById(language.id);
    ASSERT_TRUE(created.has_value());
    EXPECT_EQ(created->name, QStringLiteral("Testish"));
    EXPECT_TRUE(created->showRomanization);

    language.name = QStringLiteral("Testish 2");
    ASSERT_TRUE(database.saveLanguage(language, &error)) << error.toStdString();
    EXPECT_EQ(database.languageById(language.id)->name, QStringLiteral("Testish 2"));

    LuteTerm term;
    term.languageId = language.id;
    term.text = QStringLiteral("abc");
    term.status = 1;
    term.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(term, &error)) << error.toStdString();

    QString deleteError;
    EXPECT_FALSE(database.deleteLanguage(language.id, &deleteError));
    EXPECT_FALSE(deleteError.isEmpty());

    ASSERT_TRUE(database.deleteTerm(term.id, &error));
    EXPECT_TRUE(database.deleteLanguage(language.id, &error));
    EXPECT_FALSE(database.languageById(language.id).has_value());
}

TEST(LuteDatabase, FiltersTermsByTextAndStatus)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    LuteTerm harbor;
    harbor.languageId = english->id;
    harbor.text = QStringLiteral("Harbor");
    harbor.status = 3;
    harbor.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(harbor, &error));

    LuteTerm lighthouse;
    lighthouse.languageId = english->id;
    lighthouse.text = QStringLiteral("lighthouse");
    lighthouse.status = 5;
    lighthouse.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(lighthouse, &error));

    EXPECT_EQ(database.terms(english->id, QStringLiteral("har"), 0).size(), 1);
    EXPECT_EQ(database.terms(english->id, QString(), 5).size(), 1);
    EXPECT_EQ(database.terms(english->id, QStringLiteral("LIGHT"), 0)
                  .first()
                  .text,
              QStringLiteral("lighthouse"));
    EXPECT_EQ(database.terms(english->id, QString(), 0).size(), 2);
}


TEST(LuteDatabase, ExportsTermsAsTsv)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    LuteTerm harbor;
    harbor.languageId = english->id;
    harbor.text = QStringLiteral("harbor");
    harbor.status = 3;
    harbor.translation = QStringLiteral("Hafen");
    harbor.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(harbor, &error));

    LuteTerm phrase;
    phrase.languageId = english->id;
    phrase.text = QStringLiteral("run out of");
    phrase.status = 1;
    phrase.tokenCount = 3;
    ASSERT_TRUE(database.saveTerm(phrase, &error));

    const QString tsvPath = directory.filePath(QStringLiteral("terms.tsv"));
    ASSERT_TRUE(database.exportTermsTsv(tsvPath, english->id, &error))
        << error.toStdString();

    QFile file(tsvPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    EXPECT_TRUE(content.startsWith(
        QStringLiteral("WoText\tWoTranslation\tWoStatus\tWoTokenCount")));
    EXPECT_TRUE(content.contains(QStringLiteral("harbor\tHafen\t3\t1")));
    EXPECT_TRUE(content.contains(QStringLiteral("run out of\t\t1\t3")));
}


TEST(LuteDatabase, BacksUpTheDatabase)
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
    term.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(term, &error));

    const QString backupPath = directory.filePath(QStringLiteral("backup.db"));
    ASSERT_TRUE(database.backupTo(backupPath, &error)) << error.toStdString();
    ASSERT_TRUE(QFileInfo::exists(backupPath));

    LuteDbAccess copy;
    ASSERT_TRUE(copy.open(backupPath, &error)) << error.toStdString();
    const auto restored = copy.getTermByText(QStringLiteral("harbor"),
                                             english->id);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->status, 3);
}


TEST(LuteDatabase, ImportsTermsFromTsv)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = createTemplateDatabase(directory);

    LuteDbAccess database;
    QString error;
    ASSERT_TRUE(database.open(path, &error)) << error.toStdString();
    const auto english = database.languageByName(QStringLiteral("English"));
    ASSERT_TRUE(english.has_value());

    LuteTerm harbor;
    harbor.languageId = english->id;
    harbor.text = QStringLiteral("harbor");
    harbor.status = 3;
    harbor.translation = QStringLiteral("Hafen");
    harbor.tokenCount = 1;
    ASSERT_TRUE(database.saveTerm(harbor, &error));

    LuteTerm phrase;
    phrase.languageId = english->id;
    phrase.text = QStringLiteral("run out of");
    phrase.status = 1;
    phrase.tokenCount = 3;
    ASSERT_TRUE(database.saveTerm(phrase, &error));

    const QString tsvPath = directory.filePath(QStringLiteral("terms.tsv"));
    ASSERT_TRUE(database.exportTermsTsv(tsvPath, english->id, &error));

    // Remove the terms, then import the file again.
    ASSERT_TRUE(database.deleteTerm(harbor.id, &error));
    ASSERT_TRUE(database.deleteTerm(phrase.id, &error));
    EXPECT_EQ(database.terms(english->id, QString(), 0).size(), 0);

    int imported = 0;
    ASSERT_TRUE(database.importTermsTsv(tsvPath, english->id, &imported, &error))
        << error.toStdString();
    EXPECT_EQ(imported, 2);

    const auto restored = database.getTermByText(QStringLiteral("harbor"),
                                                 english->id);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->status, 3);
    EXPECT_EQ(restored->translation, QStringLiteral("Hafen"));
    const auto restoredPhrase =
        database.getTermByText(QStringLiteral("run out of"), english->id);
    ASSERT_TRUE(restoredPhrase.has_value());
    EXPECT_EQ(restoredPhrase->tokenCount, 3);
}
