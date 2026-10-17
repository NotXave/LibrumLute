#pragma once

#include "lute_book.hpp"
#include "lute_tag.hpp"
#include "lute_language.hpp"
#include "lute_stats.hpp"
#include "lute_term.hpp"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>

namespace lute {

/// Read/write gateway to an official Lute v3 `lute.db` (SQLite).
///
/// This is Loop 0 of the plan ("Option A: direct QtSql access"): no Python
/// server is needed at runtime. The class only ever touches Lute's own tables
/// and columns, so a database written here stays fully usable by Lute v3 and
/// vice versa.
class LuteDbAccess
{
public:
    LuteDbAccess();
    ~LuteDbAccess();

    LuteDbAccess(const LuteDbAccess&) = delete;
    LuteDbAccess& operator=(const LuteDbAccess&) = delete;

    bool open(const QString& dbPath, QString* error = nullptr);
    void close();
    bool isOpen() const;
    QString path() const;
    QString lastError() const;

    // ---- languages --------------------------------------------------------
    QVector<LuteLanguage> languages();
    std::optional<LuteLanguage> languageById(int languageId);
    std::optional<LuteLanguage> languageByName(const QString& name);

    /// Writes all terms of a language as TSV (text, translation, status,
    /// token count) for Lute/Anki import.
    bool exportTermsTsv(const QString& path, int languageId,
                        QString* error = nullptr);
    /// Anki-friendly TSV: text, translation, comma separated tags, status.
    bool exportAnkiTsv(const QString& path, int languageId,
                       QString* error = nullptr);
    /// Imports a TSV written by exportTermsTsv (header optional).
    bool importTermsTsv(const QString& path, int languageId, int* imported = nullptr,
                        QString* error = nullptr);

    /// Consistent copy of the database via SQLite's VACUUM INTO.
    bool backupTo(const QString& path, QString* error = nullptr);

    bool saveLanguage(LuteLanguage& language, QString* error = nullptr);
    bool deleteLanguage(int languageId, QString* error = nullptr);

    // ---- terms ------------------------------------------------------------
    /// `getTermByText` from the plan: single case-insensitive lookup.
    std::optional<LuteTerm> getTermByText(const QString& word, int languageId);

    /// `getTermsForPage` from the plan: status per lower-cased word.
    QHash<QString, int> getTermsForPage(const QStringList& words, int languageId);

    /// Full term records for a batch of words (used by the popup and overlay).
    QHash<QString, LuteTerm> getTermMap(const QStringList& words, int languageId);

    /// Terms of a language, optionally filtered by text and status.
    QVector<LuteTerm> terms(int languageId, const QString& search = QString(),
                            int status = 0, int limit = 500);
    std::optional<LuteTerm> termById(qint64 termId);

    /// Terms stored with `WoTokenCount > 1`, i.e. phrases like "run out of".
    QVector<LuteTerm> multiWordTerms(int languageId);

    QVector<LuteTerm> termsByStatus(int languageId, const QVector<int>& statuses,
                                    int limit = 0);

    /// Inserts a new term or updates the existing row with the same
    /// (WoTextLC, WoLgID) pair. `term.id` is filled in on return.
    bool saveTerm(LuteTerm& term, QString* error = nullptr);
    bool updateStatus(qint64 wordId, int status, QString* error = nullptr);
    bool updateTranslation(qint64 wordId, const QString& translation, QString* error = nullptr);
    bool updateRomanization(qint64 wordId, const QString& romanization, QString* error = nullptr);
    bool deleteTerm(qint64 wordId, QString* error = nullptr);

    // ---- tags -------------------------------------------------------------
    QVector<LuteTag> tags();
    bool saveTag(LuteTag& tag, QString* error = nullptr);
    bool deleteTag(qint64 tagId, QString* error = nullptr);
    QVector<LuteTag> tagsForTerm(qint64 termId);
    QVector<LuteTerm> parentsForTerm(qint64 termId);
    bool setTermParents(qint64 termId, const QVector<qint64>& parentIds,
                        QString* error = nullptr);
    bool setTermTags(qint64 termId, const QVector<qint64>& tagIds,
                     QString* error = nullptr);
    int setStatusForTerms(int languageId, const QString& search,
                          int statusFilter, int newStatus,
                          QString* error = nullptr);

    // ---- statistics -------------------------------------------------------
    QHash<int, int> statusCounts(int languageId);
    QVector<DailyCount> statusChangesPerDay(int languageId, int days);
    QVector<DailyCount> createdPerDay(int languageId, int days);
    int totalTerms(int languageId);
    int wordsReadTotal(int languageId);
    int wordsReadLast7Days(int languageId);
    bool recordWordsRead(int languageId, int wordCount, qint64 textId = 0);

    // ---- library (books/texts) --------------------------------------------
    /// All books of a language, newest first, with page count and current page.
    QVector<LuteBook> books(int languageId, bool includeArchived = true);
    std::optional<LuteBook> book(qint64 bookId);

    /// The pages of a book, in reading order (without the text contents).
    QVector<LuteText> texts(qint64 bookId);

    QVector<LuteDictionary> dictionaries(int languageId);
    bool setDictionaryActive(qint64 dictionaryId, bool active,
                             QString* error = nullptr);
    bool setDictionaryUri(qint64 dictionaryId, const QString& uri,
                          QString* error = nullptr);

    QVector<LuteBookmark> bookmarks(qint64 bookId);
    std::optional<LuteBookmark> bookmarkForText(qint64 textId);
    qint64 addBookmark(qint64 textId, const QString& title,
                       QString* error = nullptr);
    bool deleteBookmark(qint64 bookmarkId, QString* error = nullptr);
    /// One page including its text.
    std::optional<LuteText> text(qint64 bookId, int order);

    bool setBookCurrentPage(qint64 bookId, int page);
    bool markTextRead(qint64 textId);

    std::optional<qint64> findBookBySource(const QString& sourceUri, int languageId);
    qint64 findOrCreateBook(const QString& title, const QString& sourceUri, int languageId,
                            QString* error = nullptr);
    std::optional<qint64> findTextForBook(qint64 bookId);
    qint64 createText(qint64 bookId, int order, const QString& text, int wordCount,
                      QString* error = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lute
