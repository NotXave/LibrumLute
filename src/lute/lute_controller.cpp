#include "lute_controller.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointF>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#include "lute_ai_access.hpp"
#include "lute_paginator.hpp"
#include "lute_schema.hpp"
#include "lute_status.hpp"
#include "lute_text_parser.hpp"
#include "lute_word_extractor.hpp"

namespace lute
{
namespace
{

QVariantMap rectToMap(const QRectF& rect)
{
    QVariantMap map;
    map.insert(QStringLiteral("x"), rect.x());
    map.insert(QStringLiteral("y"), rect.y());
    map.insert(QStringLiteral("w"), rect.width());
    map.insert(QStringLiteral("h"), rect.height());
    return map;
}

QVariantMap statusMap(int status)
{
    QVariantMap map;
    map.insert(QStringLiteral("status"), status);
    map.insert(QStringLiteral("label"), statusLabel(status));
    map.insert(QStringLiteral("abbrev"), statusAbbreviation(status));
    map.insert(QStringLiteral("color"), statusColor(status).name());
    return map;
}

int wordIndexAt(const QVector<WordQuad>& words, QPointF point,
                qreal padding = 0.0025)
{
    int best = -1;
    qreal bestArea = 1e30;
    for(int i = 0; i < words.size(); ++i)
    {
        QRectF rect = words.at(i).rect;
        rect.adjust(-padding, -padding * 2, padding, padding * 2);
        if(!rect.contains(point))
            continue;
        const qreal area = words.at(i).rect.width() * words.at(i).rect.height();
        if(area < bestArea)
        {
            bestArea = area;
            best = i;
        }
    }
    return best;
}

constexpr int kMaxSampledPages = 40;

} // namespace

LuteController::LuteController(QObject* parent) :
    QObject(parent),
    m_extractor(std::make_unique<LuteWordExtractor>()),
    m_ai(std::make_unique<LuteAiAccess>())
{
    connect(m_ai.get(), &LuteAiAccess::suggestionReady, this,
            [this](const QString& suggestion) {
                m_aiSuggestion = suggestion;
                m_aiError.clear();
                m_activeWord.insert(QStringLiteral("aiSuggestion"), suggestion);
                emit aiChanged();
                emit wordPopupChanged();
            });
    connect(m_ai.get(), &LuteAiAccess::requestFailed, this,
            [this](const QString& error) {
                m_aiError = error;
                emit aiChanged();
            });
    connect(m_ai.get(), &LuteAiAccess::busyChanged, this,
            [this]() { emit aiChanged(); });
}

LuteController::~LuteController()
{
    if(m_ai)
        m_ai->cancel();
}

bool LuteController::databaseOpen() const
{
    return m_database.isOpen();
}

QString LuteController::databasePath() const
{
    return m_database.path();
}

QVariantList LuteController::languages() const
{
    QVariantList list;
    for(const LuteLanguage& language : m_languages)
    {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), language.id);
        entry.insert(QStringLiteral("name"), language.name);
        entry.insert(QStringLiteral("parserType"), language.parserType);
        entry.insert(QStringLiteral("characterSubstitutions"),
                     language.characterSubstitutions);
        entry.insert(QStringLiteral("regexpSplitSentences"),
                     language.regexpSplitSentences);
        entry.insert(QStringLiteral("exceptionsSplitSentences"),
                     language.exceptionsSplitSentences);
        entry.insert(QStringLiteral("regexpWordCharacters"),
                     language.regexpWordCharacters);
        entry.insert(QStringLiteral("rightToLeft"), language.rightToLeft);
        entry.insert(QStringLiteral("showRomanization"),
                     language.showRomanization);
        entry.insert(QStringLiteral("supported"),
                     LuteLanguage::isSupportedParserType(language.parserType));
        list.append(entry);
    }
    return list;
}

int LuteController::languageId() const
{
    return m_languageId;
}

QString LuteController::languageName() const
{
    const auto language = currentLanguage();
    return language ? language->name : QString();
}

QVariantList LuteController::statusLegend() const
{
    QVariantList list;
    for(int status : {1, 2, 3, 4, 5, 99, 98})
        list.append(statusMap(status));
    return list;
}

bool LuteController::documentOpen() const
{
    return m_extractor && m_extractor->isOpen();
}

int LuteController::pageCount() const
{
    return m_pageCount;
}

QString LuteController::documentTitle() const
{
    if(!documentOpen())
        return {};
    const QString title = m_extractor->title();
    if(!title.isEmpty())
        return title;
    return QFileInfo(m_documentPath).completeBaseName();
}

bool LuteController::documentRegistered() const
{
    return m_documentRegistered;
}

bool LuteController::wordPopupVisible() const
{
    return m_popupVisible;
}

QVariantMap LuteController::activeWord() const
{
    return m_activeWord;
}

QString LuteController::aiSuggestion() const
{
    return m_aiSuggestion;
}

bool LuteController::aiBusy() const
{
    return m_ai && m_ai->isBusy();
}

QString LuteController::aiError() const
{
    return m_aiError;
}

QVariantMap LuteController::aiConfig() const
{
    QSettings settings;
    LuteAiConfig config = LuteAiConfig::defaultsFor(
        settings.value(QStringLiteral("lute/aiProvider"),
                       QStringLiteral("ollama"))
            .toString());
    config.model = settings.value(QStringLiteral("lute/aiModel"), config.model)
                       .toString();
    config.baseUrl =
        settings.value(QStringLiteral("lute/aiBaseUrl"), config.baseUrl).toString();
    config.apiKey = settings.value(QStringLiteral("lute/aiApiKey")).toString();
    config.targetLanguage =
        settings
            .value(QStringLiteral("lute/aiTargetLanguage"),
                   QStringLiteral("German"))
            .toString();

    QVariantMap map;
    map.insert(QStringLiteral("provider"), config.provider);
    map.insert(QStringLiteral("providerLabel"), config.providerLabel());
    map.insert(QStringLiteral("baseUrl"), config.resolvedBaseUrl());
    map.insert(QStringLiteral("model"), config.model);
    map.insert(QStringLiteral("apiKey"), config.apiKey);
    map.insert(QStringLiteral("targetLanguage"), config.targetLanguage);
    return map;
}

QString LuteController::lastMessage() const
{
    return m_lastMessage;
}

bool LuteController::lastMessageIsError() const
{
    return m_lastMessageIsError;
}

QString LuteController::defaultDatabasePath() const
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("lute.db"));
}

void LuteController::initialize()
{
    QSettings settings;
    QString path = settings.value(QStringLiteral("lute/databasePath")).toString();
    if(path.isEmpty())
        path = defaultDatabasePath();

    if(!QFileInfo::exists(path))
    {
        QString error;
        if(!LuteSchema::createDatabase(path, &error))
        {
            setMessage(error, true);
            return;
        }
    }

    QString error;
    if(!m_database.open(path, &error))
    {
        setMessage(error, true);
        return;
    }
    settings.setValue(QStringLiteral("lute/databasePath"), path);

    loadLanguages();
    if(m_languages.isEmpty())
    {
        setMessage(QStringLiteral("No languages found in lute.db"), true);
        return;
    }

    // Language ids are only unique inside one lute.db, so the stored name is
    // the reliable key when the user switches databases.
    const QString wantedName =
        settings.value(QStringLiteral("lute/languageName")).toString();
    if(wantedName.isEmpty() || !selectLanguageByName(wantedName))
        selectFallbackLanguage();

    m_ai->setConfig([this]() {
        QSettings aiSettings;
        LuteAiConfig config = LuteAiConfig::defaultsFor(
            aiSettings.value(QStringLiteral("lute/aiProvider"),
                             QStringLiteral("ollama"))
                .toString());
        config.model =
            aiSettings.value(QStringLiteral("lute/aiModel"), config.model)
                .toString();
        config.baseUrl =
            aiSettings.value(QStringLiteral("lute/aiBaseUrl"), config.baseUrl)
                .toString();
        config.apiKey =
            aiSettings.value(QStringLiteral("lute/aiApiKey")).toString();
        config.targetLanguage =
            aiSettings
                .value(QStringLiteral("lute/aiTargetLanguage"),
                       QStringLiteral("German"))
                .toString();
        return config;
    }());

    emit databaseChanged();
    emit aiConfigChanged();
    refreshStats();
}

bool LuteController::openDatabase(const QString& path)
{
    if(!QFileInfo::exists(path))
    {
        setMessage(QStringLiteral("'%1' does not exist").arg(path), true);
        return false;
    }

    QString error;
    if(!m_database.open(path, &error))
    {
        setMessage(error, true);
        return false;
    }

    QSettings().setValue(QStringLiteral("lute/databasePath"), path);
    const QString previousLanguageName = languageName();
    loadLanguages();
    if(m_languages.isEmpty())
    {
        setMessage(QStringLiteral("No languages found in the database"), true);
        emit databaseChanged();
        return false;
    }

    m_languageId = 0;
    if(previousLanguageName.isEmpty()
       || !selectLanguageByName(previousLanguageName))
        selectFallbackLanguage();

    emit databaseChanged();
    rebuildDocumentVocabulary();
    refreshStats();
    setMessage(QStringLiteral("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool LuteController::createDatabase(const QString& path)
{
    QString error;
    if(!LuteSchema::createDatabase(path, &error))
    {
        setMessage(error, true);
        return false;
    }
    return openDatabase(path);
}

void LuteController::loadLanguages()
{
    m_languages.clear();
    if(m_database.isOpen())
        m_languages = m_database.languages();
}

std::optional<LuteLanguage> LuteController::currentLanguage() const
{
    for(const LuteLanguage& language : m_languages)
    {
        if(language.id == m_languageId)
            return language;
    }
    return std::nullopt;
}

void LuteController::selectLanguage(int languageId)
{
    std::optional<LuteLanguage> language;
    for(const LuteLanguage& candidate : m_languages)
    {
        if(candidate.id == languageId)
        {
            language = candidate;
            break;
        }
    }
    if(!language)
        return;

    m_languageId = languageId;
    QSettings().setValue(QStringLiteral("lute/languageId"), languageId);
    QSettings().setValue(QStringLiteral("lute/languageName"), language->name);

    if(m_extractor)
        m_extractor->setLanguage(*language);
    m_parser = std::make_unique<LuteTextParser>(*language);
    m_phraseTerms = m_database.multiWordTerms(languageId);

    invalidateHighlights();
    emit languageChanged();
    refreshLibrary();
    rebuildDocumentVocabulary();
    refreshStats();
}

bool LuteController::selectLanguageByName(const QString& name)
{
    for(const LuteLanguage& language : m_languages)
    {
        if(language.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            selectLanguage(language.id);
            return true;
        }
    }
    return false;
}

void LuteController::selectFallbackLanguage()
{
    // A Lute database does not have to contain English (the user's own
    // library might only have Polish); fall back to the first language so the
    // learning features are usable immediately.
    if(selectLanguageByName(QStringLiteral("English")))
        return;
    if(!m_languages.isEmpty())
        selectLanguage(m_languages.first().id);
}

void LuteController::refreshLibrary()
{
    m_books.clear();
    if(m_database.isOpen() && m_languageId > 0)
    {
        const QVector<LuteBook> books =
            m_database.books(m_languageId, true);
        for(const LuteBook& book : books)
        {
            QVariantMap map;
            map.insert(QStringLiteral("id"), book.id);
            map.insert(QStringLiteral("title"), book.title);
            map.insert(QStringLiteral("pages"), book.pageCount);
            map.insert(QStringLiteral("currentPage"), book.currentPage);
            map.insert(QStringLiteral("archived"), book.archived);
            map.insert(QStringLiteral("language"), book.languageName);
            map.insert(QStringLiteral("source"), book.sourceUri);
            m_books.append(map);
        }
    }
    emit booksChanged();
}

void LuteController::refreshTerms(const QString& search, int status)
{
    m_termsLoaded = true;
    m_termSearch = search;
    m_termStatus = status;
    m_terms.clear();

    if(m_database.isOpen() && m_languageId > 0)
    {
        const QVector<LuteTerm> terms =
            m_database.terms(m_languageId, search, status);
        for(const LuteTerm& term : terms)
        {
            QVariantMap map;
            map.insert(QStringLiteral("id"), term.id);
            map.insert(QStringLiteral("word"), term.text);
            map.insert(QStringLiteral("status"), term.status);
            map.insert(QStringLiteral("statusLabel"), statusLabel(term.status));
            map.insert(QStringLiteral("statusAbbrev"), statusAbbreviation(term.status));
            map.insert(QStringLiteral("color"), statusColor(term.status).name());
            map.insert(QStringLiteral("translation"), term.translation);
            map.insert(QStringLiteral("romanization"), term.romanization);
            map.insert(QStringLiteral("tokenCount"), term.tokenCount);
            map.insert(QStringLiteral("isPhrase"), term.tokenCount > 1);
            m_terms.append(map);
        }
    }
    emit termsChanged();
}

void LuteController::openTerms()
{
    refreshTerms(m_termSearch, m_termStatus);
    emit showTermsRequested();
}

bool LuteController::saveLanguage(const QVariantMap& values)
{
    if(!m_database.isOpen())
    {
        setMessage(QStringLiteral("Open a lute.db first"), true);
        return false;
    }

    LuteLanguage language;
    language.id = values.value(QStringLiteral("id")).toInt();
    language.name = values.value(QStringLiteral("name")).toString().trimmed();
    if(language.name.isEmpty())
    {
        setMessage(QStringLiteral("The language needs a name"), true);
        return false;
    }
    language.characterSubstitutions =
        values.value(QStringLiteral("characterSubstitutions")).toString();
    language.regexpSplitSentences =
        values.value(QStringLiteral("regexpSplitSentences")).toString();
    language.exceptionsSplitSentences =
        values.value(QStringLiteral("exceptionsSplitSentences")).toString();
    language.regexpWordCharacters =
        values.value(QStringLiteral("regexpWordCharacters")).toString();
    language.rightToLeft = values.value(QStringLiteral("rightToLeft")).toBool();
    language.showRomanization =
        values.value(QStringLiteral("showRomanization")).toBool();
    language.parserType =
        values.value(QStringLiteral("parserType")).toString();
    if(language.parserType.isEmpty())
        language.parserType = QStringLiteral("spacedel");

    QString error;
    if(!m_database.saveLanguage(language, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not save the language")
                                   : error,
                   true);
        return false;
    }

    const int editedId = language.id;
    loadLanguages();
    if(m_languageId == editedId)
    {
        m_languageId = 0;
        selectLanguage(editedId);
    }
    else
    {
        emit languageChanged();
    }
    refreshLibrary();
    setMessage(QStringLiteral("Language \"%1\" saved").arg(language.name));
    return true;
}

bool LuteController::deleteLanguage(int languageId)
{
    QString error;
    if(!m_database.deleteLanguage(languageId, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not delete the language")
                                   : error,
                   true);
        return false;
    }
    const bool wasCurrent = m_languageId == languageId;
    if(wasCurrent)
        m_languageId = 0;
    loadLanguages();
    if(wasCurrent)
        selectFallbackLanguage();
    else
        emit languageChanged();
    refreshLibrary();
    setMessage(QStringLiteral("Language deleted"));
    return true;
}

void LuteController::openLanguages()
{
    emit showLanguagesRequested();
}

void LuteController::refreshTags()
{
    m_tags.clear();
    if(m_database.isOpen())
    {
        for(const LuteTag& tag : m_database.tags())
        {
            QVariantMap map;
            map.insert(QStringLiteral("id"), tag.id);
            map.insert(QStringLiteral("text"), tag.text);
            map.insert(QStringLiteral("comment"), tag.comment);
            m_tags.append(map);
        }
    }
    emit tagsChanged();
}

bool LuteController::saveTag(qint64 id, const QString& text, const QString& comment)
{
    if(text.trimmed().isEmpty())
    {
        setMessage(QStringLiteral("The tag needs a name"), true);
        return false;
    }
    LuteTag tag;
    tag.id = id;
    tag.text = text.trimmed();
    tag.comment = comment;
    QString error;
    if(!m_database.saveTag(tag, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not save the tag") : error, true);
        return false;
    }
    refreshTags();
    return true;
}

bool LuteController::deleteTag(qint64 id)
{
    QString error;
    if(!m_database.deleteTag(id, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not delete the tag") : error, true);
        return false;
    }
    refreshTags();
    return true;
}

bool LuteController::toggleActiveTag(qint64 tagId)
{
    if(!m_popupVisible || !m_activeTerm || !m_activeTerm->isValid())
        return false;

    QVector<qint64> ids;
    bool present = false;
    for(const LuteTag& tag : m_database.tagsForTerm(m_activeTerm->id))
    {
        ids.append(tag.id);
        if(tag.id == tagId)
            present = true;
    }
    if(present)
        ids.removeAll(tagId);
    else
        ids.append(tagId);

    QString error;
    if(!m_database.setTermTags(m_activeTerm->id, ids, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not update the tags")
                                   : error,
                   true);
        return false;
    }

    QVariantList names;
    for(const LuteTag& tag : m_database.tagsForTerm(m_activeTerm->id))
        names.append(tag.text);
    m_activeWord.insert(QStringLiteral("tags"), names);
    emit wordPopupChanged();
    return true;
}

void LuteController::openTags()
{
    refreshTags();
    emit showTagsRequested();
}

bool LuteController::restoreDatabase(const QString& path)
{
    if(!QFileInfo::exists(path))
    {
        setMessage(QStringLiteral("'%1' does not exist").arg(path), true);
        return false;
    }
    const QString current = m_database.path();
    if(current.isEmpty())
    {
        setMessage(QStringLiteral("Open a lute.db first"), true);
        return false;
    }

    m_database.close();
    if(!QFile::remove(current) || !QFile::copy(path, current))
    {
        setMessage(QStringLiteral("Could not replace %1").arg(current), true);
        initialize();
        return false;
    }
    initialize();
    refreshLibrary();
    if(m_termsLoaded)
        refreshTerms(m_termSearch, m_termStatus);
    setMessage(QStringLiteral("lute.db restored from %1").arg(path));
    return true;
}

bool LuteController::importTerms(const QString& path)
{
    if(!m_database.isOpen() || m_languageId <= 0)
    {
        setMessage(QStringLiteral("Open a lute.db first"), true);
        return false;
    }
    int imported = 0;
    QString error;
    if(!m_database.importTermsTsv(path, m_languageId, &imported, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Import failed") : error,
                   true);
        return false;
    }
    if(m_termsLoaded)
        refreshTerms(m_termSearch, m_termStatus);
    refreshStats();
    setMessage(QStringLiteral("%1 terms imported").arg(imported));
    return true;
}

bool LuteController::backupDatabase(const QString& path)
{
    if(!m_database.isOpen())
    {
        setMessage(QStringLiteral("Open a lute.db first"), true);
        return false;
    }
    QString error;
    if(!m_database.backupTo(path, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Backup failed") : error,
                   true);
        return false;
    }
    setMessage(QStringLiteral("lute.db backed up to %1").arg(path));
    return true;
}

bool LuteController::exportTerms(const QString& path)
{
    if(!m_database.isOpen() || m_languageId <= 0)
    {
        setMessage(QStringLiteral("Open a lute.db first"), true);
        return false;
    }
    QString error;
    if(!m_database.exportTermsTsv(path, m_languageId, &error))
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not export terms")
                                   : error,
                   true);
        return false;
    }
    setMessage(QStringLiteral("Terms exported to %1").arg(path));
    return true;
}

void LuteController::openLuteStatistics()
{
    refreshStats();
    emit showStatisticsPageRequested();
}

bool LuteController::selectTerm(qint64 termId)
{
    const auto term = m_database.termById(termId);
    if(!term)
        return false;

    WordQuad word;
    word.word = term->text;
    word.page = 0;
    word.index = 0;
    word.rect = QRectF();
    buildActiveWord(0, word, 0,
                    term->tokenCount > 0 ? term->tokenCount - 1 : 0,
                    term->tokenCount > 1);
    return true;
}

bool LuteController::openBook(qint64 bookId)
{
    if(!m_database.isOpen())
        return false;

    const auto book = m_database.book(bookId);
    if(!book)
    {
        setMessage(QStringLiteral("Book %1 could not be opened").arg(bookId),
                   true);
        return false;
    }

    m_currentBook = *book;
    m_currentBookMap.clear();
    m_currentBookMap.insert(QStringLiteral("id"), book->id);
    m_currentBookMap.insert(QStringLiteral("title"), book->title);
    m_currentBookMap.insert(QStringLiteral("pages"), book->pageCount);
    m_currentBookMap.insert(QStringLiteral("currentPage"), book->currentPage);
    m_currentBookMap.insert(QStringLiteral("language"), book->languageName);

    m_readerPageCount = m_database.texts(bookId).size();
    m_readerOpen = true;
    if(!openPage(book->currentPage))
    {
        m_readerOpen = false;
        emit readerChanged();
        return false;
    }
    emit readerChanged();
    return true;
}

bool LuteController::openPage(int page)
{
    if(!m_currentBook.isValid() || !m_database.isOpen())
        return false;

    if(page < 1)
        page = 1;
    if(m_readerPageCount > 0 && page > m_readerPageCount)
        page = m_readerPageCount;

    const auto text = m_database.text(m_currentBook.id, page);
    if(!text)
        return false;

    m_readerPage = page;
    m_readerTextId = text->id;
    m_readerText = text->text;
    m_readerPageRead = text->read;
    m_readerHtml = buildReaderHtml(m_readerText);
    m_database.setBookCurrentPage(m_currentBook.id, page);
    m_currentBook.currentPage = page;
    emit readerChanged();
    return true;
}

void LuteController::nextPage()
{
    if(m_readerOpen && m_readerPage < m_readerPageCount)
        openPage(m_readerPage + 1);
}

void LuteController::previousPage()
{
    if(m_readerOpen && m_readerPage > 1)
        openPage(m_readerPage - 1);
}

void LuteController::markPageRead()
{
    if(!m_readerOpen || m_readerTextId <= 0)
        return;
    if(m_database.markTextRead(m_readerTextId))
    {
        m_readerPageRead = true;
        emit readerChanged();
    }
}

void LuteController::closeBook()
{
    m_readerOpen = false;
    m_readerTextId = 0;
    m_readerText.clear();
    m_readerHtml.clear();
    m_textTokens.clear();
    emit readerChanged();
}

bool LuteController::selectTextWord(int index)
{
    if(!m_readerOpen || !m_parser || index < 0
       || index >= m_textTokens.size())
        return false;

    const ParsedWord& token = m_textTokens.at(index);
    WordQuad word;
    word.word = token.text;
    word.sentence = m_parser->sentenceAt(m_readerText, token.start);
    word.page = m_readerPage;
    word.index = index;
    word.rect = QRectF();
    buildActiveWord(m_readerPage, word, index, index, false);
    return true;
}

void LuteController::openLibrary()
{
    if(m_database.isOpen())
        refreshLibrary();
    emit showLibraryRequested();
}

void LuteController::showReader()
{
    emit showLuteReaderRequested();
}

bool LuteController::createBook(const QString& title, const QString& text,
                               bool splitBySentences)
{
    if(!m_database.isOpen() || m_languageId <= 0 || !m_parser)
    {
        setMessage(QStringLiteral("Open a lute.db and select a language first"),
                   true);
        return false;
    }

    const QString cleanTitle = title.trimmed();
    if(cleanTitle.isEmpty())
    {
        setMessage(QStringLiteral("Please enter a title"), true);
        return false;
    }
    if(text.trimmed().isEmpty())
    {
        setMessage(QStringLiteral("Please enter some text"), true);
        return false;
    }

    const QVector<QString> pages =
        LutePaginator::paginate(text, *m_parser, 250, splitBySentences);
    if(pages.isEmpty())
    {
        setMessage(QStringLiteral("The text contains no words"), true);
        return false;
    }

    QString error;
    const qint64 bookId =
        m_database.findOrCreateBook(cleanTitle, QString(), m_languageId, &error);
    if(bookId <= 0)
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not create the book")
                                   : error,
                   true);
        return false;
    }
    if(m_database.findTextForBook(bookId))
    {
        setMessage(QStringLiteral("A book called \"%1\" already exists")
                       .arg(cleanTitle),
                   true);
        return false;
    }

    int order = 1;
    for(const QString& page : pages)
    {
        const int wordCount = m_parser->words(page).size();
        if(m_database.createText(bookId, order, page, wordCount, &error) <= 0)
        {
            setMessage(error.isEmpty() ? QStringLiteral("Could not add the page")
                                       : error,
                       true);
            return false;
        }
        ++order;
    }

    refreshLibrary();
    setMessage(QStringLiteral("\"%1\" was added with %2 pages")
                   .arg(cleanTitle)
                   .arg(pages.size()));
    return true;
}

bool LuteController::importTextFile(const QString& path)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
    {
        setMessage(QStringLiteral("Cannot read \"%1\": %2")
                       .arg(path, file.errorString()),
                   true);
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll());
    file.close();
    return createBook(QFileInfo(path).completeBaseName(), text, false);
}

QString LuteController::buildReaderHtml(const QString& text)
{
    m_textTokens.clear();
    if(!m_parser)
        return text.toHtmlEscaped().replace(QLatin1Char('\n'),
                                            QStringLiteral("<br/>"));

    m_textTokens = m_parser->words(text);

    QStringList keys;
    keys.reserve(m_textTokens.size());
    for(const ParsedWord& token : m_textTokens)
        keys.append(m_parser->lowercase(token.text));

    const QHash<QString, int> statuses =
        m_database.getTermsForPage(keys, m_languageId);

    QString html;
    int last = 0;
    for(int i = 0; i < m_textTokens.size(); ++i)
    {
        const ParsedWord& token = m_textTokens.at(i);
        if(token.start > last)
            html += text.mid(last, token.start - last).toHtmlEscaped();

        const QString wordText = text.mid(token.start, token.end - token.start);
        const int status =
            statuses.value(m_parser->lowercase(wordText), 0);
        const QString color = status != 0 ? statusColor(status).name()
                                         : QStringLiteral("#D5D5D5");
        QString style = QStringLiteral("color:%1;text-decoration:none").arg(color);
        if(status != 0)
            style += QStringLiteral(";text-decoration:underline");

        html += QStringLiteral("<a href=\"w:%1\" style=\"%2\">%3</a>")
                    .arg(i)
                    .arg(style, wordText.toHtmlEscaped());
        last = token.end;
    }
    if(last < text.size())
        html += text.mid(last).toHtmlEscaped();

    html.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return html;
}

bool LuteController::setDocumentPath(const QString& path)
{
    if(!m_extractor)
        m_extractor = std::make_unique<LuteWordExtractor>();

    QString error;
    if(!m_extractor->open(path, &error))
    {
        setMessage(error, true);
        return false;
    }

    if(const auto language = currentLanguage())
        m_extractor->setLanguage(*language);

    m_documentPath = path;
    m_pageCount = m_extractor->pageCount();
    m_visitedPages.clear();
    m_highlightCache.clear();
    m_phraseCache.clear();
    m_documentRegistered = false;
    closeWordPopup();

    if(m_database.isOpen() && m_languageId > 0)
    {
        if(const auto bookId = m_database.findBookBySource(path, m_languageId))
            m_documentRegistered = m_database.findTextForBook(*bookId).has_value();
    }

    QSettings().setValue(QStringLiteral("lute/lastDocument"), path);

    emit documentChanged();
    invalidateHighlights();
    rebuildDocumentVocabulary();
    refreshStats();
    return true;
}

void LuteController::closeDocument()
{
    if(m_extractor)
        m_extractor->close();
    m_documentPath.clear();
    m_pageCount = 0;
    m_visitedPages.clear();
    m_highlightCache.clear();
    m_phraseCache.clear();
    m_documentRegistered = false;
    closeWordPopup();
    emit documentChanged();
    invalidateHighlights();
    refreshStats();
}

void LuteController::invalidateHighlights()
{
    m_highlightCache.clear();
    m_phraseCache.clear();
    ++m_highlightsRevision;
    emit highlightsChanged();
}

const QVector<PhraseMatch>& LuteController::phraseMatches(int pageIndex)
{
    const auto cached = m_phraseCache.constFind(pageIndex);
    if(cached != m_phraseCache.constEnd())
        return *cached;

    QVector<PhraseMatch> matches;
    if(m_phraseTerms.isEmpty() || !documentOpen() || !m_parser)
    {
        return *m_phraseCache.insert(pageIndex, matches);
    }
    matches = PhraseMatcher::findMatches(m_extractor->words(pageIndex),
                                        m_phraseTerms, *m_parser);
    return *m_phraseCache.insert(pageIndex, matches);
}

QVariantList LuteController::highlightsForPage(int pageIndex)
{
    const auto cached = m_highlightCache.constFind(pageIndex);
    if(cached != m_highlightCache.constEnd())
        return *cached;

    QVariantList list;
    if(!documentOpen() || pageIndex < 0 || pageIndex >= m_pageCount
       || m_languageId <= 0)
        return list;

    const QVector<WordQuad>& words = m_extractor->words(pageIndex);
    if(words.isEmpty())
        return list;

    QStringList texts;
    texts.reserve(words.size());
    for(const WordQuad& word : words)
        texts.append(word.word);

    const QHash<QString, int> statuses =
        m_database.getTermsForPage(texts, m_languageId);

    const QVector<PhraseMatch>& matches = phraseMatches(pageIndex);
    QVector<bool> consumed(words.size(), false);

    for(const PhraseMatch& match : matches)
    {
        for(int i = match.firstIndex; i <= match.lastIndex; ++i)
        {
            if(i >= 0 && i < consumed.size())
                consumed[i] = true;
        }

        int status = 0;
        for(const LuteTerm& term : m_phraseTerms)
        {
            if(term.id == match.termId)
            {
                status = term.status;
                break;
            }
        }
        if(status == 0)
            continue;

        QVariantMap entry = statusMap(status);
        entry.insert(QStringLiteral("kind"), QStringLiteral("phrase"));
        entry.insert(QStringLiteral("text"), match.text);
        entry.insert(QStringLiteral("firstIndex"), match.firstIndex);
        entry.insert(QStringLiteral("lastIndex"), match.lastIndex);
        entry.insert(QStringLiteral("tokenCount"), match.tokenCount);
        const QVariantMap rect = rectToMap(match.boundingRect);
        for(auto it = rect.constBegin(); it != rect.constEnd(); ++it)
            entry.insert(it.key(), it.value());

        QVariantList parts;
        for(int i = match.firstIndex; i <= match.lastIndex; ++i)
        {
            if(i >= 0 && i < words.size())
                parts.append(rectToMap(words.at(i).rect));
        }
        entry.insert(QStringLiteral("parts"), parts);
        list.append(entry);
    }

    for(int i = 0; i < words.size(); ++i)
    {
        if(consumed.value(i))
            continue;
        const WordQuad& word = words.at(i);
        const int status =
            m_parser ? statuses.value(m_parser->lowercase(word.word), 0) : 0;
        if(status == 0)
            continue;

        QVariantMap entry = statusMap(status);
        entry.insert(QStringLiteral("kind"), QStringLiteral("word"));
        entry.insert(QStringLiteral("text"), word.word);
        entry.insert(QStringLiteral("firstIndex"), word.index);
        entry.insert(QStringLiteral("lastIndex"), word.index);
        entry.insert(QStringLiteral("tokenCount"), 1);
        const QVariantMap rect = rectToMap(word.rect);
        for(auto it = rect.constBegin(); it != rect.constEnd(); ++it)
            entry.insert(it.key(), it.value());
        list.append(entry);
    }

    recordPageVisit(pageIndex);
    m_highlightCache.insert(pageIndex, list);
    return list;
}

QVariantList LuteController::pageWords(int pageIndex)
{
    QVariantList list;
    if(!documentOpen() || pageIndex < 0 || pageIndex >= m_pageCount)
        return list;

    const QVector<WordQuad>& words = m_extractor->words(pageIndex);
    for(int i = 0; i < words.size() && i < 500; ++i)
    {
        QVariantMap map;
        map.insert(QStringLiteral("word"), words.at(i).word);
        map.insert(QStringLiteral("sentence"), words.at(i).sentence);
        map.insert(QStringLiteral("index"), words.at(i).index);
        map.insert(QStringLiteral("rect"), rectToMap(words.at(i).rect));
        list.append(map);
    }
    return list;
}

bool LuteController::setStatusForWord(int pageIndex, int wordIndex, int status)
{
    if(!documentOpen() || m_languageId <= 0 || !m_database.isOpen())
        return false;

    const QVector<WordQuad>& words = m_extractor->words(pageIndex);
    if(wordIndex < 0 || wordIndex >= words.size())
        return false;

    const WordQuad& word = words.at(wordIndex);
    LuteTerm term;
    term.languageId = m_languageId;
    term.text = word.word;
    term.tokenCount = 1;
    term.status = status;

    QString error;
    if(!m_database.saveTerm(term, &error))
    {
        setMessage(error, true);
        return false;
    }
    invalidateHighlights();
    refreshStats();
    return true;
}

void LuteController::recordPageVisit(int pageIndex)
{
    if(m_visitedPages.contains(pageIndex) || !m_database.isOpen()
       || m_languageId <= 0)
        return;
    m_visitedPages.insert(pageIndex);
    const int wordCount = m_extractor ? m_extractor->words(pageIndex).size() : 0;
    if(wordCount > 0)
        m_database.recordWordsRead(m_languageId, wordCount);
}

void LuteController::buildActiveWord(int pageIndex, const WordQuad& word,
                                     int firstIndex, int lastIndex,
                                     bool isPhrase)
{
    m_activeFirstIndex = firstIndex;
    m_activeLastIndex = lastIndex;
    m_activeIsPhrase = isPhrase;
    m_activeQuad = word;

    if(isPhrase && m_extractor)
    {
        const QVector<WordQuad>& words = m_extractor->words(pageIndex);
        QRectF box;
        QStringList parts;
        for(int i = firstIndex; i <= lastIndex && i < words.size(); ++i)
        {
            if(i < 0)
                continue;
            box = box.isNull() ? words.at(i).rect : box.united(words.at(i).rect);
            parts << words.at(i).word;
        }
        if(!box.isNull())
        {
            m_activeQuad.rect = box;
            m_activeQuad.word = parts.join(QLatin1Char(' '));
        }
    }

    const auto term = m_database.getTermByText(m_activeQuad.word, m_languageId);
    m_activeTerm = term;

    QVariantMap map;
    map.insert(QStringLiteral("word"), m_activeQuad.word);
    map.insert(QStringLiteral("sentence"), m_activeQuad.sentence);
    map.insert(QStringLiteral("page"), pageIndex);
    map.insert(QStringLiteral("rect"), rectToMap(m_activeQuad.rect));
    map.insert(QStringLiteral("isPhrase"), isPhrase);
    map.insert(QStringLiteral("tokenCount"),
               isPhrase ? (lastIndex - firstIndex + 1) : 1);
    map.insert(QStringLiteral("hasTerm"), term.has_value());
    map.insert(QStringLiteral("termId"), term ? term->id : 0);
    map.insert(QStringLiteral("status"), term ? term->status : 0);
    map.insert(QStringLiteral("statusLabel"),
               statusLabel(term ? term->status : 0));
    map.insert(QStringLiteral("statusAbbrev"),
               statusAbbreviation(term ? term->status : 0));
    map.insert(QStringLiteral("translation"),
               term ? term->translation : QString());
    map.insert(QStringLiteral("romanization"),
               term ? term->romanization : QString());
    const auto language = currentLanguage();
    map.insert(QStringLiteral("showRomanization"),
               language ? language->showRomanization : false);
    map.insert(QStringLiteral("aiSuggestion"), QString());
    QVariantList tagNames;
    if(term)
    {
        for(const LuteTag& tag : m_database.tagsForTerm(term->id))
            tagNames.append(tag.text);
    }
    map.insert(QStringLiteral("tags"), tagNames);

    m_activeWord = map;
    m_popupVisible = true;
    m_aiSuggestion.clear();
    m_aiError.clear();
    emit aiChanged();
    emit wordPopupChanged();
}

bool LuteController::selectAtPagePoint(int pageIndex, qreal normalizedX,
                                       qreal normalizedY)
{
    if(!documentOpen())
        return false;

    const QVector<WordQuad>& words = m_extractor->words(pageIndex);
    const int index = wordIndexAt(words, QPointF(normalizedX, normalizedY));
    if(index < 0)
    {
        closeWordPopup();
        return false;
    }

    const QVector<PhraseMatch>& matches = phraseMatches(pageIndex);
    const PhraseMatch* match = PhraseMatcher::matchAt(matches, words.at(index).index);
    if(match)
        buildActiveWord(pageIndex, words.at(index), match->firstIndex,
                        match->lastIndex, true);
    else
        buildActiveWord(pageIndex, words.at(index), words.at(index).index,
                        words.at(index).index, false);
    return true;
}

bool LuteController::selectRangeAtPage(int pageIndex, qreal startX,
                                       qreal startY, qreal endX, qreal endY)
{
    if(!documentOpen())
        return false;

    const QVector<WordQuad>& words = m_extractor->words(pageIndex);
    if(words.isEmpty())
        return false;

    const int startIndex = wordIndexAt(words, QPointF(startX, startY));
    const int endIndex = wordIndexAt(words, QPointF(endX, endY));
    if(startIndex < 0 || endIndex < 0)
        return false;

    const int firstIndex = std::min(startIndex, endIndex);
    const int lastIndex = std::max(startIndex, endIndex);
    buildActiveWord(pageIndex, words.at(firstIndex), firstIndex, lastIndex,
                    firstIndex != lastIndex);
    return true;
}

void LuteController::closeWordPopup()
{
    if(!m_popupVisible && m_activeWord.isEmpty())
        return;
    m_popupVisible = false;
    m_activeTerm.reset();
    m_aiSuggestion.clear();
    m_aiError.clear();
    m_activeWord.clear();
    emit aiChanged();
    emit wordPopupChanged();
}

void LuteController::refreshActiveTerm()
{
    if(!m_popupVisible)
        return;
    const auto term = m_database.getTermByText(m_activeQuad.word, m_languageId);
    m_activeTerm = term;
    m_activeWord.insert(QStringLiteral("hasTerm"), term.has_value());
    m_activeWord.insert(QStringLiteral("termId"), term ? term->id : 0);
    m_activeWord.insert(QStringLiteral("status"), term ? term->status : 0);
    m_activeWord.insert(QStringLiteral("statusLabel"),
                        statusLabel(term ? term->status : 0));
    m_activeWord.insert(QStringLiteral("statusAbbrev"),
                        statusAbbreviation(term ? term->status : 0));
    m_activeWord.insert(QStringLiteral("translation"),
                        term ? term->translation : QString());
    m_activeWord.insert(QStringLiteral("romanization"),
                        term ? term->romanization : QString());
    emit wordPopupChanged();
}

void LuteController::setActiveStatus(int status)
{
    if(!m_popupVisible || !m_database.isOpen() || m_languageId <= 0)
        return;

    LuteTerm term = m_activeTerm.value_or(LuteTerm{});
    if(term.id <= 0)
    {
        term.languageId = m_languageId;
        term.text = m_activeQuad.word;
        term.tokenCount =
            m_activeIsPhrase ? (m_activeLastIndex - m_activeFirstIndex + 1) : 1;
        term.translation =
            m_activeWord.value(QStringLiteral("translation")).toString();
        term.romanization =
            m_activeWord.value(QStringLiteral("romanization")).toString();
    }
    term.status = status;

    QString error;
    if(!m_database.saveTerm(term, &error))
    {
        setMessage(error, true);
        return;
    }
    m_activeTerm = term;
    if(term.tokenCount > 1)
        m_phraseTerms = m_database.multiWordTerms(m_languageId);

    if(m_parser)
        m_documentWordStatus.insert(m_parser->lowercase(term.text), status);

    invalidateHighlights();
    refreshActiveTerm();
    refreshStats();
    if(m_readerOpen)
        openPage(m_readerPage);
    if(m_termsLoaded)
        refreshTerms(m_termSearch, m_termStatus);
}

void LuteController::setActiveTranslation(const QString& text)
{
    if(!m_popupVisible || !m_database.isOpen() || m_languageId <= 0)
        return;

    LuteTerm term = m_activeTerm.value_or(LuteTerm{});
    if(term.id <= 0)
    {
        term.languageId = m_languageId;
        term.text = m_activeQuad.word;
        term.tokenCount =
            m_activeIsPhrase ? (m_activeLastIndex - m_activeFirstIndex + 1) : 1;
        term.status = m_activeWord.value(QStringLiteral("status")).toInt();
    }
    term.translation = text;

    QString error;
    if(!m_database.saveTerm(term, &error))
    {
        setMessage(error, true);
        return;
    }
    m_activeTerm = term;
    invalidateHighlights();
    refreshActiveTerm();
    refreshStats();
    if(m_termsLoaded)
        refreshTerms(m_termSearch, m_termStatus);
}

void LuteController::setActiveRomanization(const QString& text)
{
    if(!m_popupVisible || !m_database.isOpen() || m_languageId <= 0)
        return;

    LuteTerm term = m_activeTerm.value_or(LuteTerm{});
    if(term.id <= 0)
    {
        term.languageId = m_languageId;
        term.text = m_activeQuad.word;
        term.tokenCount =
            m_activeIsPhrase ? (m_activeLastIndex - m_activeFirstIndex + 1) : 1;
        term.status = m_activeWord.value(QStringLiteral("status")).toInt();
    }
    term.romanization = text;

    QString error;
    if(!m_database.saveTerm(term, &error))
    {
        setMessage(error, true);
        return;
    }
    m_activeTerm = term;
    refreshActiveTerm();
}

void LuteController::deleteActiveTerm()
{
    if(!m_activeTerm || !m_activeTerm->isValid())
        return;
    QString error;
    if(!m_database.deleteTerm(m_activeTerm->id, &error))
    {
        setMessage(error, true);
        return;
    }
    if(m_parser)
        m_documentWordStatus.remove(m_parser->lowercase(m_activeQuad.word));
    m_activeTerm.reset();
    m_phraseTerms = m_database.multiWordTerms(m_languageId);
    invalidateHighlights();
    refreshActiveTerm();
    refreshStats();
}

void LuteController::requestAiSuggestion()
{
    if(!m_popupVisible || !m_ai)
        return;

    m_aiSuggestion.clear();
    m_aiError.clear();
    m_activeWord.insert(QStringLiteral("aiSuggestion"), QString());
    emit aiChanged();
    emit wordPopupChanged();

    // Use the settings-backed configuration (provider/model/url/key).
    QSettings settings;
    LuteAiConfig config = LuteAiConfig::defaultsFor(
        settings.value(QStringLiteral("lute/aiProvider"),
                       QStringLiteral("ollama"))
            .toString());
    config.model =
        settings.value(QStringLiteral("lute/aiModel"), config.model).toString();
    config.baseUrl =
        settings.value(QStringLiteral("lute/aiBaseUrl"), config.baseUrl).toString();
    config.apiKey = settings.value(QStringLiteral("lute/aiApiKey")).toString();
    config.targetLanguage =
        settings
            .value(QStringLiteral("lute/aiTargetLanguage"),
                   QStringLiteral("German"))
            .toString();
    m_ai->setConfig(config);

    m_ai->requestTranslation(m_activeQuad.word, m_activeQuad.sentence,
                             m_activeIsPhrase);
}

void LuteController::applyAiSuggestion()
{
    if(m_aiSuggestion.trimmed().isEmpty())
        return;
    setActiveTranslation(m_aiSuggestion);
}

void LuteController::openStatistics()
{
    refreshStats();
    emit showStatisticsRequested();
}

void LuteController::openSettings()
{
    emit showSettingsRequested();
}

void LuteController::openReader()
{
    emit showReaderRequested();
}

void LuteController::saveAiConfiguration(const QString& provider,
                                         const QString& model,
                                         const QString& baseUrl,
                                         const QString& apiKey,
                                         const QString& targetLanguage)
{
    QSettings settings;
    LuteAiConfig config = LuteAiConfig::defaultsFor(provider);
    config.provider = provider;
    config.model = model.trimmed().isEmpty() ? config.model : model.trimmed();
    config.baseUrl =
        baseUrl.trimmed().isEmpty() ? config.resolvedBaseUrl() : baseUrl.trimmed();
    config.apiKey = apiKey.trimmed();
    config.targetLanguage = targetLanguage.trimmed().isEmpty()
                                ? QStringLiteral("German")
                                : targetLanguage.trimmed();

    settings.setValue(QStringLiteral("lute/aiProvider"), config.provider);
    settings.setValue(QStringLiteral("lute/aiModel"), config.model);
    settings.setValue(QStringLiteral("lute/aiBaseUrl"), config.baseUrl);
    settings.setValue(QStringLiteral("lute/aiApiKey"), config.apiKey);
    settings.setValue(QStringLiteral("lute/aiTargetLanguage"),
                      config.targetLanguage);
    if(m_ai)
        m_ai->setConfig(config);
    emit aiConfigChanged();
    setMessage(QStringLiteral("AI settings saved"));
}

void LuteController::rebuildDocumentVocabulary()
{
    m_documentTokenCount = 0;
    m_documentKnownTokens = 0;
    m_documentCoverageEstimated = false;
    m_documentWordStatus.clear();

    if(!documentOpen() || m_languageId <= 0 || !m_parser)
        return;

    const int pages = m_pageCount;
    const int step = std::max(1, pages / kMaxSampledPages);
    m_documentCoverageEstimated = step > 1;

    QHash<QString, int> frequency;
    int sampledPages = 0;
    for(int page = 0; page < pages; page += step)
    {
        for(const WordQuad& word : m_extractor->words(page))
        {
            const QString key = m_parser->lowercase(word.word);
            if(!key.isEmpty())
                frequency[key] += 1;
        }
        ++sampledPages;
    }

    int tokens = 0;
    QStringList keys;
    keys.reserve(frequency.size());
    for(auto it = frequency.constBegin(); it != frequency.constEnd(); ++it)
    {
        tokens += it.value();
        keys.append(it.key());
    }

    m_documentTokenCount = sampledPages > 0
                               ? int(qint64(tokens) * pages / sampledPages)
                               : tokens;
    m_documentWordStatus = m_database.getTermsForPage(keys, m_languageId);

    int known = 0;
    for(auto it = frequency.constBegin(); it != frequency.constEnd(); ++it)
    {
        if(m_documentWordStatus.value(it.key(), 0) != 0)
            known += it.value();
    }
    m_documentKnownTokens =
        sampledPages > 0 ? int(qint64(known) * pages / sampledPages) : known;
}

void LuteController::refreshStats()
{
    QVariantMap map;
    map.insert(QStringLiteral("languageId"), m_languageId);
    map.insert(QStringLiteral("languageName"), languageName());
    map.insert(QStringLiteral("documentTokenCount"), m_documentTokenCount);
    map.insert(QStringLiteral("documentKnownTokens"), m_documentKnownTokens);
    map.insert(QStringLiteral("coverageEstimated"), m_documentCoverageEstimated);
    map.insert(QStringLiteral("hasDocument"), documentOpen());

    int total = 0;
    int learning = 0;
    int known = 0;
    QVariantList byStatus;
    if(m_database.isOpen() && m_languageId > 0)
    {
        const QHash<int, int> counts = m_database.statusCounts(m_languageId);
        for(auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        {
            total += it.value();
            if(isLearningStatus(it.key()))
                learning += it.value();
            else if(isKnownStatus(it.key()))
                known += it.value();
        }
        for(int status : {1, 2, 3, 4, 5, 99, 98, 0})
        {
            QVariantMap entry = statusMap(status);
            entry.insert(QStringLiteral("count"), counts.value(status, 0));
            byStatus.append(entry);
        }
        map.insert(QStringLiteral("wordsReadTotal"),
                   m_database.wordsReadTotal(m_languageId));
    }
    map.insert(QStringLiteral("totalTerms"), total);
    map.insert(QStringLiteral("learningTerms"), learning);
    map.insert(QStringLiteral("knownTerms"), known);
    map.insert(QStringLiteral("byStatus"), byStatus);

    const double coverage =
        m_documentTokenCount > 0
            ? 100.0 * double(m_documentKnownTokens) / double(m_documentTokenCount)
            : 0.0;
    map.insert(QStringLiteral("coverage"), coverage);

    m_learnedSeries.clear();
    m_createdSeries.clear();
    if(m_database.isOpen() && m_languageId > 0)
    {
        auto toSeries = [](const QVector<DailyCount>& days) {
            QVariantList list;
            for(const DailyCount& day : days)
            {
                QVariantMap entry;
                entry.insert(QStringLiteral("day"), day.date.toString(QStringLiteral("MM-dd")));
                entry.insert(QStringLiteral("count"), day.count);
                list.append(entry);
            }
            return list;
        };
        m_learnedSeries = toSeries(m_database.statusChangesPerDay(m_languageId, 14));
        m_createdSeries = toSeries(m_database.createdPerDay(m_languageId, 14));
    }

    m_stats = map;
    emit statsChanged();
}

bool LuteController::importDocument(LuteWordExtractor& extractor,
                                   const QString& path)
{
    QString title = extractor.title().trimmed();
    if(title.isEmpty())
        title = QFileInfo(path).completeBaseName();

    QString error;
    const qint64 bookId = m_database.findOrCreateBook(title, path, m_languageId,
                                                     &error);
    if(bookId <= 0)
    {
        setMessage(error.isEmpty()
                       ? QStringLiteral("Could not add \"%1\"").arg(title)
                       : error,
                   true);
        return false;
    }

    if(!m_database.findTextForBook(bookId))
    {
        QString text;
        int wordCount = 0;
        for(int page = 0; page < extractor.pageCount(); ++page)
        {
            const QString pageText = extractor.pageText(page).trimmed();
            if(pageText.isEmpty())
                continue;
            text += pageText + QStringLiteral("\n\n");
            wordCount += extractor.words(page).size();
        }

        if(text.trimmed().isEmpty())
        {
            setMessage(QStringLiteral("\"%1\" contains no extractable text")
                           .arg(title),
                       true);
            return false;
        }

        if(m_database.createText(bookId, 1, text, wordCount, &error) <= 0)
        {
            setMessage(error.isEmpty()
                           ? QStringLiteral("Could not add the text")
                           : error,
                       true);
            return false;
        }
    }

    ++m_libraryRevision;
    emit libraryChanged();
    setMessage(QStringLiteral("\"%1\" is now in the Lute library").arg(title));
    return true;
}

void LuteController::addDocumentToLuteLibrary()
{
    if(!documentOpen() || !m_database.isOpen() || m_languageId <= 0)
    {
        setMessage(QStringLiteral("Open a PDF first"), true);
        return;
    }

    if(!importDocument(*m_extractor, m_documentPath))
        return;

    m_documentRegistered = true;
    emit documentChanged();
}

bool LuteController::addPdfToLuteLibrary(const QString& path)
{
    if(!m_database.isOpen() || m_languageId <= 0)
    {
        setMessage(QStringLiteral("Open a lute.db and select a language first"),
                   true);
        return false;
    }

    QString error;
    LuteWordExtractor extractor;
    if(!extractor.open(path, &error))
    {
        setMessage(error, true);
        return false;
    }
    if(const auto language = currentLanguage())
        extractor.setLanguage(*language);

    if(!importDocument(extractor, path))
        return false;

    if(!m_documentPath.isEmpty()
       && QFileInfo(path).absoluteFilePath()
              == QFileInfo(m_documentPath).absoluteFilePath())
    {
        m_documentRegistered = true;
        emit documentChanged();
    }
    return true;
}

void LuteController::messageShown()
{
    m_lastMessage.clear();
    m_lastMessageIsError = false;
    emit lastMessageChanged();
}

void LuteController::setMessage(const QString& message, bool isError)
{
    m_lastMessage = message;
    m_lastMessageIsError = isError;
    emit lastMessageChanged();
}

} // namespace lute
