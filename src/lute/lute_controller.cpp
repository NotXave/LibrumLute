#include "lute_controller.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPointF>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#include "lute_ai_access.hpp"
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

    const int wanted =
        settings.value(QStringLiteral("lute/languageId"), 0).toInt();
    bool found = false;
    for(const LuteLanguage& language : m_languages)
    {
        if(language.id == wanted)
        {
            found = true;
            break;
        }
    }
    if(found)
        selectLanguage(wanted);
    else
        selectLanguageByName(QStringLiteral("English"));

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
    const int previousLanguage = m_languageId;
    loadLanguages();
    if(m_languages.isEmpty())
    {
        setMessage(QStringLiteral("No languages found in the database"), true);
        emit databaseChanged();
        return false;
    }

    m_languageId = 0;
    bool found = false;
    for(const LuteLanguage& language : m_languages)
    {
        if(language.id == previousLanguage)
        {
            found = true;
            break;
        }
    }
    if(found)
        selectLanguage(previousLanguage);
    else
        selectLanguageByName(QStringLiteral("English"));

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

    if(m_extractor)
        m_extractor->setLanguage(*language);
    m_parser = std::make_unique<LuteTextParser>(*language);
    m_phraseTerms = m_database.multiWordTerms(languageId);

    invalidateHighlights();
    emit languageChanged();
    rebuildDocumentVocabulary();
    refreshStats();
}

void LuteController::selectLanguageByName(const QString& name)
{
    for(const LuteLanguage& language : m_languages)
    {
        if(language.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            selectLanguage(language.id);
            return;
        }
    }
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
    map.insert(QStringLiteral("showRomanization"),
               m_extractor ? m_extractor->language().showRomanization : false);
    map.insert(QStringLiteral("aiSuggestion"), QString());

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

    m_stats = map;
    emit statsChanged();
}

void LuteController::addDocumentToLuteLibrary()
{
    if(!documentOpen() || !m_database.isOpen() || m_languageId <= 0)
    {
        setMessage(QStringLiteral("Open a lute.db and a PDF first"), true);
        return;
    }

    QString error;
    const qint64 bookId = m_database.findOrCreateBook(
        documentTitle(), m_documentPath, m_languageId, &error);
    if(bookId <= 0)
    {
        setMessage(error.isEmpty() ? QStringLiteral("Could not add the book")
                                   : error,
                   true);
        return;
    }
    if(!m_database.findTextForBook(bookId))
    {
        QString text;
        for(int page = 0; page < m_pageCount; ++page)
        {
            const QString pageText = m_extractor->pageText(page).trimmed();
            if(!pageText.isEmpty())
                text += pageText + QStringLiteral("\n\n");
        }
        const qint64 textId = m_database.createText(
            bookId, 1, text, m_documentTokenCount, &error);
        if(textId <= 0)
        {
            setMessage(error.isEmpty() ? QStringLiteral("Could not add the text")
                                       : error,
                       true);
            return;
        }
    }
    m_documentRegistered = true;
    emit documentChanged();
    setMessage(QStringLiteral("\"%1\" is now part of the Lute library")
                   .arg(documentTitle()));
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
