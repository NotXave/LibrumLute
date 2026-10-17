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

    Q_PROPERTY(bool wordPopupVisible READ wordPopupVisible NOTIFY wordPopupChanged)
    Q_PROPERTY(QVariantMap activeWord READ activeWord NOTIFY wordPopupChanged)
    Q_PROPERTY(int highlightsRevision READ highlightsRevision NOTIFY highlightsChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)

    Q_PROPERTY(QString aiSuggestion READ aiSuggestion NOTIFY aiChanged)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY aiChanged)
    Q_PROPERTY(QString aiError READ aiError NOTIFY aiChanged)
    Q_PROPERTY(QVariantMap aiConfig READ aiConfig NOTIFY aiConfigChanged)

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

    bool wordPopupVisible() const;
    QVariantMap activeWord() const;
    int highlightsRevision() const { return m_highlightsRevision; }
    QVariantMap stats() const { return m_stats; }

    QString aiSuggestion() const;
    bool aiBusy() const;
    QString aiError() const;
    QVariantMap aiConfig() const;

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
    void selectLanguageByName(const QString& name);

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

    /// Ask the shell to open the Lute statistics / settings page.
    void openStatistics();
    void openSettings();

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
    const QVector<PhraseMatch>& phraseMatches(int pageIndex);

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
