#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <optional>

#include "lute_book.hpp"
#include "lute_db_access.hpp"
#include "lute_language.hpp"
#include "lute_term.hpp"
#include "phrase_matcher.hpp"
#include "word_quad.hpp"

namespace lute
{

class LuteAiAccess;
class LuteTextParser;
class LuteWordExtractor;

/***
 * The single object the QML reading page talks to: it owns the lute.db
 * connection, extracts the words of the open PDF via MuPDF, answers "which
 * word is at this point", manages the Lute statuses and provides the AI
 * translation. Everything it stores ends up in the same lute.db that Lute v3
 * itself uses.
 */
class LuteController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool databaseOpen READ databaseOpen NOTIFY databaseChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databaseChanged)
    Q_PROPERTY(QVariantList languages READ languages NOTIFY languageChanged)
    Q_PROPERTY(int languageId READ languageId NOTIFY languageChanged)
    Q_PROPERTY(QString languageName READ languageName NOTIFY languageChanged)
    Q_PROPERTY(QVariantList statusLegend READ statusLegend CONSTANT)

    Q_PROPERTY(bool documentOpen READ documentOpen NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(QString documentTitle READ documentTitle NOTIFY documentChanged)
    Q_PROPERTY(bool documentRegistered READ documentRegistered NOTIFY documentChanged)
    Q_PROPERTY(int libraryRevision READ libraryRevision NOTIFY libraryChanged)

    Q_PROPERTY(bool wordPopupVisible READ wordPopupVisible NOTIFY wordPopupChanged)
    Q_PROPERTY(QVariantMap activeWord READ activeWord NOTIFY wordPopupChanged)
    Q_PROPERTY(int highlightsRevision READ highlightsRevision NOTIFY highlightsChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)

    Q_PROPERTY(QString aiSuggestion READ aiSuggestion NOTIFY aiChanged)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY aiChanged)
    Q_PROPERTY(QString aiError READ aiError NOTIFY aiChanged)
    Q_PROPERTY(QVariantMap aiConfig READ aiConfig NOTIFY aiConfigChanged)

    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(QVariantList terms READ terms NOTIFY termsChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QVariantList dictionaries READ dictionaries NOTIFY dictionariesChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList learnedSeries READ learnedSeries NOTIFY statsChanged)
    Q_PROPERTY(QVariantList createdSeries READ createdSeries NOTIFY statsChanged)
    Q_PROPERTY(QVariantMap currentBook READ currentBook NOTIFY readerChanged)
    Q_PROPERTY(bool readerOpen READ readerOpen NOTIFY readerChanged)
    Q_PROPERTY(QString readerHtml READ readerHtml NOTIFY readerChanged)
    Q_PROPERTY(QString readerTitle READ readerTitle NOTIFY readerChanged)
    Q_PROPERTY(int readerPage READ readerPage NOTIFY readerChanged)
    Q_PROPERTY(int readerPageCount READ readerPageCount NOTIFY readerChanged)
    Q_PROPERTY(bool readerPageRead READ readerPageRead NOTIFY readerChanged)

    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(bool lastMessageIsError READ lastMessageIsError NOTIFY
                   lastMessageChanged)

public:
    explicit LuteController(QObject* parent = nullptr);
    ~LuteController() override;

    bool databaseOpen() const;
    QString databasePath() const;
    QVariantList languages() const;
    int languageId() const;
    QString languageName() const;
    QVariantList statusLegend() const;

    bool documentOpen() const;
    int pageCount() const;
    QString documentTitle() const;
    bool documentRegistered() const;
    int libraryRevision() const { return m_libraryRevision; }

    bool wordPopupVisible() const;
    QVariantMap activeWord() const;
    int highlightsRevision() const { return m_highlightsRevision; }
    QVariantMap stats() const { return m_stats; }

    QString aiSuggestion() const;
    bool aiBusy() const;
    QString aiError() const;
    QVariantMap aiConfig() const;

    QVariantList books() const { return m_books; }
    QVariantList terms() const { return m_terms; }
    QVariantList tags() const { return m_tags; }
    QVariantList dictionaries() const { return m_dictionaries; }
    QVariantList bookmarks() const { return m_bookmarks; }
    QVariantList learnedSeries() const { return m_learnedSeries; }
    QVariantList createdSeries() const { return m_createdSeries; }
    QVariantMap currentBook() const { return m_currentBookMap; }
    bool readerOpen() const { return m_readerOpen; }
    QString readerHtml() const { return m_readerHtml; }
    QString readerTitle() const { return m_currentBook.title; }
    int readerPage() const { return m_readerPage; }
    int readerPageCount() const { return m_readerPageCount; }
    bool readerPageRead() const { return m_readerPageRead; }

    QString lastMessage() const;
    bool lastMessageIsError() const;

public slots:
    /// Opens (or creates) the lute.db and selects the last used language.
    void initialize();

    /// Switches to an existing lute.db.
    bool openDatabase(const QString& path);

    /// Creates a fresh lute.db from the shipped template and opens it.
    bool createDatabase(const QString& path);

    /// Saves AI settings and keeps the controller in sync (QML convenience).
    Q_INVOKABLE void saveAiSettings(const QString& provider, const QString& model,
                                    const QString& baseUrl, const QString& apiKey,
                                    const QString& targetLanguage) {
        saveAiConfiguration(provider, model, baseUrl, apiKey, targetLanguage);
    }

    /// Opens `path` as the document Lute works on.
    bool setDocumentPath(const QString& path);
    void closeDocument();

    void selectLanguage(int languageId);
    bool selectLanguageByName(const QString& name);

    /// All highlighted words of a page (cached) for the overlay.
    QVariantList highlightsForPage(int pageIndex);

    /// Every extracted word of a page; used by the QA self test.
    QVariantList pageWords(int pageIndex);

    /// Sets the Lute status of the n-th word of a page (used for seeding and
    /// bulk actions).
    bool setStatusForWord(int pageIndex, int wordIndex, int status);

    /// Selecting a word at a normalised page point opens the Lute popup.
    bool selectAtPagePoint(int pageIndex, qreal normalizedX,
                           qreal normalizedY);

    /// Selects the range between two normalised page points (a drag selection
    /// in Librum's PageView).
    bool selectRangeAtPage(int pageIndex, qreal startX, qreal startY,
                           qreal endX, qreal endY);
    void closeWordPopup();

    void setActiveStatus(int status);
    void setActiveTranslation(const QString& text);
    void setActiveRomanization(const QString& text);
    void deleteActiveTerm();

    void requestAiSuggestion();
    void applyAiSuggestion();

    void refreshStats();
    void addDocumentToLuteLibrary();

    /// Adds an arbitrary PDF to the Lute library, even when no document is
    /// open in the reader.
    bool addPdfToLuteLibrary(const QString& path);

    /// Ask the shell to open the Lute statistics / settings page.
    void openStatistics();
    void openSettings();

    /// Ask the shell to go back to the document that is being read.
    void openReader();

    // ---- Lute library and text reader ---------------------------------
    /// Reloads the book list of the selected language.
    void refreshLibrary();
    bool openBook(qint64 bookId);
    bool openPage(int page);
    void nextPage();
    void previousPage();
    void markPageRead();
    void closeBook();
    /// Opens the word popup for the n-th token of the current text page.
    bool selectTextWord(int index);
    /// Ask the shell to show the Lute library / the Lute text reader.
    void openLibrary();
    void showReader();

    /// Creates a new Lute book from pasted/imported text using Lute's
    /// pagination rules.
    bool createBook(const QString& title, const QString& text,
                    bool splitBySentences = false);
    /// Imports a UTF-8 text file as a new Lute book.
    bool importTextFile(const QString& path);

    /// Reloads the term list (search text and status filter, 0 = all).
    void refreshTerms(const QString& search = QString(), int status = 0);
    void openTerms();
    /// Opens the Lute popup for a stored term (used by the term list).
    bool selectTerm(qint64 termId);

    /// Creates or updates a language (keys of `languages()` entries).
    bool saveLanguage(const QVariantMap& language);
    bool deleteLanguage(int languageId);
    void openLanguages();
    void openLuteStatistics();
    bool exportTerms(const QString& path);
    bool backupDatabase(const QString& path);
    bool restoreDatabase(const QString& path);
    bool importTerms(const QString& path);
    void refreshTags();
    bool saveTag(qint64 id, const QString& text, const QString& comment);
    bool deleteTag(qint64 id);
    bool deleteTermById(qint64 id);
    bool toggleBookmark(int page);
    bool copyActiveWord();
    bool toggleActiveTag(qint64 tagId);
    bool linkActiveParent(const QString& parentText);
    bool unlinkActiveParent(qint64 parentId);
    void openTags();

    void saveAiConfiguration(const QString& provider, const QString& model,
                             const QString& baseUrl, const QString& apiKey,
                             const QString& targetLanguage);
    void messageShown();

signals:
    void databaseChanged();
    void languageChanged();
    void documentChanged();
    void wordPopupChanged();
    void highlightsChanged();
    void statsChanged();
    void aiChanged();
    void aiConfigChanged();
    void lastMessageChanged();
    void showStatisticsRequested();
    void showSettingsRequested();
    void showReaderRequested();
    void libraryChanged();
    void booksChanged();
    void termsChanged();
    void showTermsRequested();
    void showLanguagesRequested();
    void showStatisticsPageRequested();
    void tagsChanged();
    void bookmarksChanged();
    void dictionariesChanged();
    void showTagsRequested();
    void readerChanged();
    void showLibraryRequested();
    void showLuteReaderRequested();

private:
    void loadLanguages();
    void invalidateHighlights();
    void rebuildDocumentVocabulary();
    void recordPageVisit(int pageIndex);
    std::optional<LuteLanguage> currentLanguage() const;
    QString defaultDatabasePath() const;
    void setMessage(const QString& message, bool isError = false);
    void buildActiveWord(int pageIndex, const WordQuad& word, int firstIndex,
                         int lastIndex, bool isPhrase);
    void refreshActiveTerm();
    void refreshActiveFamily();
    void loadBookmarks();
    void loadDictionaries();
    const QVector<PhraseMatch>& phraseMatches(int pageIndex);
    bool importDocument(LuteWordExtractor& extractor, const QString& path);
    void selectFallbackLanguage();
    QString buildReaderHtml(const QString& text);

    LuteDbAccess m_database;
    std::unique_ptr<LuteWordExtractor> m_extractor;
    std::unique_ptr<LuteAiAccess> m_ai;
    std::unique_ptr<LuteTextParser> m_parser;

    QVector<LuteLanguage> m_languages;
    int m_languageId = 0;
    QVector<LuteTerm> m_phraseTerms;

    QString m_documentPath;
    int m_pageCount = 0;
    bool m_documentRegistered = false;
    int m_libraryRevision = 0;

    QVariantList m_books;
    QVariantList m_terms;
    QVariantList m_tags;
    QVariantList m_bookmarks;
    QVariantList m_dictionaries;
    QVariantList m_learnedSeries;
    QVariantList m_createdSeries;
    bool m_termsLoaded = false;
    QString m_termSearch;
    int m_termStatus = 0;
    LuteBook m_currentBook;
    QVariantMap m_currentBookMap;
    bool m_readerOpen = false;
    qint64 m_readerTextId = 0;
    int m_readerPage = 1;
    int m_readerPageCount = 0;
    bool m_readerPageRead = false;
    QString m_readerHtml;
    QString m_readerText;
    QVector<ParsedWord> m_textTokens;

    QSet<int> m_visitedPages;
    QHash<int, QVariantList> m_highlightCache;
    QHash<int, QVector<PhraseMatch>> m_phraseCache;
    int m_highlightsRevision = 0;

    bool m_popupVisible = false;
    WordQuad m_activeQuad;
    int m_activeFirstIndex = -1;
    int m_activeLastIndex = -1;
    bool m_activeIsPhrase = false;
    std::optional<LuteTerm> m_activeTerm;
    QVariantMap m_activeWord;

    QVariantMap m_stats;
    int m_documentTokenCount = 0;
    int m_documentKnownTokens = 0;
    bool m_documentCoverageEstimated = false;
    QHash<QString, int> m_documentWordStatus;

    QString m_aiSuggestion;
    QString m_aiError;

    QString m_lastMessage;
    bool m_lastMessageIsError = false;
};

} // namespace lute
