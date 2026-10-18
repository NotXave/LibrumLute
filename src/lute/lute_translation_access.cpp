#include "lute_translation_access.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

namespace lute
{
namespace
{

struct LanguageNames
{
    const char* name;
    const char* iso;
    const char* reversoSlug;
};

/// Lute language display names -> ISO 639 code and context.reverso.net slug.
const LanguageNames kLanguages[] = {
    {"german", "de", "german"},
    {"deutsch", "de", "german"},
    {"english", "en", "english"},
    {"polish", "pl", "polish"},
    {"french", "fr", "french"},
    {"spanish", "es", "spanish"},
    {"italian", "it", "italian"},
    {"portuguese", "pt", "portuguese"},
    {"russian", "ru", "russian"},
    {"ukrainian", "uk", "ukrainian"},
    {"czech", "cs", "czech"},
    {"dutch", "nl", "dutch"},
    {"swedish", "sv", "swedish"},
    {"norwegian", "no", "norwegian"},
    {"danish", "da", "danish"},
    {"finnish", "fi", "finnish"},
    {"turkish", "tr", "turkish"},
    {"greek", "el", "greek"},
    {"hungarian", "hu", "hungarian"},
    {"romanian", "ro", "romanian"},
    {"bulgarian", "bg", "bulgarian"},
    {"serbian", "sr", "serbian"},
    {"croatian", "hr", "croatian"},
    {"slovak", "sk", "slovak"},
    {"slovene", "sl", "slovenian"},
    {"slovenian", "sl", "slovenian"},
    {"lithuanian", "lt", "lithuanian"},
    {"latvian", "lv", "latvian"},
    {"estonian", "et", "estonian"},
    {"albanian", "sq", "albanian"},
    {"macedonian", "mk", "macedonian"},
    {"arabic", "ar", "arabic"},
    {"hebrew", "he", "hebrew"},
    {"persian", "fa", "persian"},
    {"farsi", "fa", "persian"},
    {"hindi", "hi", "hindi"},
    {"korean", "ko", "korean"},
    {"japanese", "ja", "japanese"},
    {"chinese", "zh-CN", "chinese"},
    {"mandarin", "zh-CN", "chinese"},
    {"thai", "th", "thai"},
    {"vietnamese", "vi", "vietnamese"},
    {"indonesian", "id", "indonesian"},
};

const LanguageNames* lookup(const QString& languageName)
{
    const QString name = languageName.trimmed().toLower();
    for(const LanguageNames& entry : kLanguages)
    {
        if(name == QLatin1String(entry.name))
            return &entry;
    }
    return nullptr;
}

QString decodeEntities(const QString& text)
{
    static const QHash<QString, QString> named = {
        {QStringLiteral("nbsp"), QStringLiteral(" ")},
        {QStringLiteral("amp"), QStringLiteral("&")},
        {QStringLiteral("lt"), QStringLiteral("<")},
        {QStringLiteral("gt"), QStringLiteral(">")},
        {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},
        {QStringLiteral("aacute"), QStringLiteral("á")},
        {QStringLiteral("eacute"), QStringLiteral("é")},
        {QStringLiteral("iacute"), QStringLiteral("í")},
        {QStringLiteral("oacute"), QStringLiteral("ó")},
        {QStringLiteral("uacute"), QStringLiteral("ú")},
        {QStringLiteral("ntilde"), QStringLiteral("ñ")},
        {QStringLiteral("auml"), QStringLiteral("ä")},
        {QStringLiteral("ouml"), QStringLiteral("ö")},
        {QStringLiteral("uuml"), QStringLiteral("ü")},
        {QStringLiteral("szlig"), QStringLiteral("ß")},
        {QStringLiteral("ccedil"), QStringLiteral("ç")},
        {QStringLiteral("agrave"), QStringLiteral("à")},
        {QStringLiteral("egrave"), QStringLiteral("è")},
        {QStringLiteral("igrave"), QStringLiteral("ì")},
        {QStringLiteral("ograve"), QStringLiteral("ò")},
        {QStringLiteral("ugrave"), QStringLiteral("ù")},
        {QStringLiteral("acirc"), QStringLiteral("â")},
        {QStringLiteral("ecirc"), QStringLiteral("ê")},
        {QStringLiteral("icirc"), QStringLiteral("î")},
        {QStringLiteral("ocirc"), QStringLiteral("ô")},
        {QStringLiteral("ucirc"), QStringLiteral("û")},
    };
    static const QRegularExpression entity(
        QStringLiteral("&#(\\d+);|&([a-zA-Z]+);"));

    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = entity.globalMatch(text);
    while(it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        result += text.mid(last, match.capturedStart() - last);
        const QString numeric = match.captured(1);
        if(!numeric.isEmpty())
            result += QChar(numeric.toUShort());
        else
        {
            const QString mapped = named.value(match.captured(2));
            result += mapped.isEmpty() ? match.captured(0) : mapped;
        }
        last = match.capturedEnd();
    }
    result += text.mid(last);
    return result;
}

/// Strips tags, decodes entities and collapses whitespace.
QString cleanHtmlText(const QString& html)
{
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    QString text = html;
    text.remove(tags);
    text = decodeEntities(text);
    static const QRegularExpression collapse(QStringLiteral("\\s+"));
    text.replace(collapse, QLatin1String(" "));
    return text.trimmed();
}

QVariantMap baseResult(const QString& providerKey, const QString& label,
                       const QString& text)
{
    QVariantMap result;
    result.insert(QStringLiteral("provider"), providerKey);
    result.insert(QStringLiteral("label"), label);
    result.insert(QStringLiteral("text"), text);
    return result;
}

QVariantList takeExamples(const QString& html, int limit)
{
    QVariantList examples;
    static const QRegularExpression blockRegex(
        QStringLiteral("<div[^>]*class=\"[^\"]*example[^\"]*\"[^>]*>(.*?)</div>"),
        QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = blockRegex.globalMatch(html);
    while(it.hasNext() && examples.size() < limit)
    {
        const QString block = it.next().captured(1);
        static const QRegularExpression srcRegex(
            QStringLiteral("<p[^>]*class=\"[^\"]*src[^\"]*\"[^>]*>(.*?)</p>"),
            QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression trgRegex(
            QStringLiteral("<p[^>]*class=\"[^\"]*trg[^\"]*\"[^>]*>(.*?)</p>"),
            QRegularExpression::DotMatchesEverythingOption);
        const QString source = cleanHtmlText(srcRegex.match(block).captured(1));
        const QString target = cleanHtmlText(trgRegex.match(block).captured(1));
        if(source.isEmpty() || target.isEmpty())
            continue;
        QVariantMap pair;
        pair.insert(QStringLiteral("source"), source);
        pair.insert(QStringLiteral("target"), target);
        examples.append(pair);
    }
    return examples;
}

} // namespace

LuteTranslationAccess::LuteTranslationAccess(QObject* parent) :
    QObject(parent),
    m_network(new QNetworkAccessManager(this))
{
}

LuteTranslationAccess::~LuteTranslationAccess() = default;

QString LuteTranslationAccess::providerKey(TranslationProvider provider)
{
    switch(provider)
    {
    case TranslationProvider::Reverso:
        return QStringLiteral("reverso");
    case TranslationProvider::Google:
        return QStringLiteral("google");
    case TranslationProvider::Ai:
        return QStringLiteral("ai");
    case TranslationProvider::MyMemory:
        return QStringLiteral("mymemory");
    }
    return QStringLiteral("google");
}

QString LuteTranslationAccess::providerLabel(TranslationProvider provider)
{
    switch(provider)
    {
    case TranslationProvider::Reverso:
        return QStringLiteral("Reverso Context");
    case TranslationProvider::Google:
        return QStringLiteral("Google Translate");
    case TranslationProvider::Ai:
        return QStringLiteral("KI-Übersetzung");
    case TranslationProvider::MyMemory:
        return QStringLiteral("MyMemory");
    }
    return QStringLiteral("Google Translate");
}

TranslationProvider LuteTranslationAccess::providerFromKey(
    const QString& key, TranslationProvider fallback)
{
    const QString normalized = key.trimmed().toLower();
    if(normalized == QStringLiteral("reverso"))
        return TranslationProvider::Reverso;
    if(normalized == QStringLiteral("google"))
        return TranslationProvider::Google;
    if(normalized == QStringLiteral("ai"))
        return TranslationProvider::Ai;
    if(normalized == QStringLiteral("mymemory"))
        return TranslationProvider::MyMemory;
    return fallback;
}

QString LuteTranslationAccess::cacheKey(TranslationProvider provider,
                                        const QString& text,
                                        const QString& targetLanguage)
{
    return providerKey(provider) + QLatin1Char('|') + text + QLatin1Char('|')
        + targetLanguage.trimmed().toLower();
}

bool LuteTranslationAccess::hasCached(TranslationProvider provider,
                                      const QString& text,
                                      const QString& targetLanguage) const
{
    return m_cache.contains(cacheKey(provider, text, targetLanguage));
}

QString LuteTranslationAccess::languageCode(const QString& languageName)
{
    const LanguageNames* entry = lookup(languageName);
    return entry ? QString::fromLatin1(entry->iso) : QStringLiteral("auto");
}

QString LuteTranslationAccess::reversoSlug(const QString& languageName)
{
    const LanguageNames* entry = lookup(languageName);
    return entry ? QString::fromLatin1(entry->reversoSlug)
                 : QStringLiteral("english");
}

QUrl LuteTranslationAccess::googleTranslateUrl(const QString& text,
                                               const QString& sourceCode,
                                               const QString& targetCode)
{
    QUrl url(QStringLiteral("https://translate.googleapis.com/translate_a/single"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("gtx"));
    query.addQueryItem(QStringLiteral("sl"), sourceCode);
    query.addQueryItem(QStringLiteral("tl"), targetCode);
    query.addQueryItem(QStringLiteral("dt"), QStringLiteral("t"));
    query.addQueryItem(QStringLiteral("q"), text);
    url.setQuery(query);
    return url;
}

QUrl LuteTranslationAccess::myMemoryUrl(const QString& text,
                                        const QString& sourceCode,
                                        const QString& targetCode)
{
    QUrl url(QStringLiteral("https://api.mymemory.translated.net/get"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), text);
    query.addQueryItem(QStringLiteral("langpair"),
                       sourceCode + QLatin1Char('|') + targetCode);
    url.setQuery(query);
    return url;
}

QUrl LuteTranslationAccess::reversoUrl(const QString& word,
                                       const QString& sourceSlug,
                                       const QString& targetSlug)
{
    const QString encoded =
        QString::fromUtf8(QUrl::toPercentEncoding(word.trimmed()));
    return QUrl(QStringLiteral("https://context.reverso.net/translation/%1-%2/%3")
                    .arg(sourceSlug, targetSlug, encoded));
}

QVariantMap LuteTranslationAccess::parseGoogleResponse(const QByteArray& payload,
                                                       const QString& text)
{
    QVariantMap result = baseResult(QStringLiteral("google"),
                                    QStringLiteral("Google Translate"), text);
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if(!document.isArray())
        return result;

    const QJsonArray root = document.array();
    if(root.isEmpty() || !root.first().isArray())
        return result;
    const QJsonArray sentences = root.first().toArray();
    QString translation;
    for(const QJsonValue& value : sentences)
    {
        if(!value.isArray())
            continue;
        const QJsonArray parts = value.toArray();
        if(!parts.isEmpty() && parts.first().isString())
            translation += parts.first().toString();
    }

    result.insert(QStringLiteral("translation"), translation.trimmed());
    result.insert(QStringLiteral("attributionUrl"),
                  QStringLiteral("https://translate.google.com/"));
    return result;
}

QVariantMap LuteTranslationAccess::parseMyMemoryResponse(const QByteArray& payload,
                                                         const QString& text)
{
    QVariantMap result = baseResult(QStringLiteral("mymemory"),
                                    QStringLiteral("MyMemory"), text);
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if(!document.isObject())
        return result;

    const QJsonObject response =
        document.object().value(QStringLiteral("responseData")).toObject();
    result.insert(QStringLiteral("translation"),
                  response.value(QStringLiteral("translatedText")).toString().trimmed());
    result.insert(QStringLiteral("attributionUrl"),
                  QStringLiteral("https://mymemory.translated.net/"));
    return result;
}

QVariantMap LuteTranslationAccess::parseReversoHtml(const QString& html,
                                                    const QString& word,
                                                    const QString& sourceSlug,
                                                    const QString& targetSlug,
                                                    bool isPhrase)
{
    QVariantMap result = baseResult(
        QStringLiteral("reverso"), QStringLiteral("Reverso Context"), word);
    result.insert(QStringLiteral("attributionUrl"),
                  reversoUrl(word, sourceSlug, targetSlug).toString());

    const QVariantList examples = takeExamples(html, 3);
    if(!examples.isEmpty())
        result.insert(QStringLiteral("examples"), examples);

    // Primary translation for single words: the dictionary block links its
    // entries with class="translation ..." and dictionary.reverso.net hrefs.
    if(!isPhrase)
    {
        static const QRegularExpression translationLink(
            QStringLiteral("<a[^>]*(?:class=\"[^\"]*translation[^\"]*\""
                           "|href=\"[^\"]*dictionary\\.reverso\\.net[^\"]*\")"
                           "[^>]*>(.*?)</a>"),
            QRegularExpression::DotMatchesEverythingOption);
        const QString primary =
            cleanHtmlText(translationLink.match(html).captured(1));
        if(!primary.isEmpty())
        {
            result.insert(QStringLiteral("translation"), primary);
            return result;
        }
    }

    // Fallback: the plain text between "Translation of" and "Show more".
    static const QRegularExpression fallbackBlock(
        QStringLiteral("Translation of(.+?)(?:Show more|</section|</div)"),
        QRegularExpression::DotMatchesEverythingOption);
    QString primary = cleanHtmlText(fallbackBlock.match(html).captured(1));
    if(!primary.isEmpty())
    {
        // The block starts with the word itself; drop it if present.
        if(primary.startsWith(word, Qt::CaseInsensitive))
            primary = primary.mid(word.length()).trimmed();
        if(!primary.isEmpty())
        {
            result.insert(QStringLiteral("translation"), primary);
            return result;
        }
    }

    // Phrases have no dictionary entries; the first context pair is the
    // closest thing to a primary translation.
    if(!examples.isEmpty())
    {
        const QVariantMap first = examples.first().toMap();
        result.insert(QStringLiteral("translation"),
                      first.value(QStringLiteral("target")).toString());
        return result;
    }

    result.insert(QStringLiteral("translation"), QString());
    return result;
}

QVariantMap LuteTranslationAccess::aiResult(const QString& suggestion,
                                            const QString& label)
{
    QVariantMap result = baseResult(QStringLiteral("ai"), label, QString());
    result.insert(QStringLiteral("translation"), suggestion.trimmed());
    return result;
}

void LuteTranslationAccess::request(TranslationProvider provider,
                                    const QString& text,
                                    const QString& sourceLanguage,
                                    const QString& targetLanguage,
                                    bool isPhrase)
{
    cancel();
    if(text.trimmed().isEmpty())
        return;

    m_provider = provider;
    m_providerKey = providerKey(provider);
    m_text = text;
    m_targetLanguage = targetLanguage;

    const QString key = cacheKey(provider, text, targetLanguage);
    const QVariantMap cached = m_cache.value(key);
    if(cached.contains(QStringLiteral("translation")))
    {
        emit resultReady(cached);
        return;
    }

    QUrl url;
    QString acceptLanguage;
    bool isReverso = provider == TranslationProvider::Reverso;
    switch(provider)
    {
    case TranslationProvider::Reverso:
        url = reversoUrl(text, reversoSlug(sourceLanguage),
                         reversoSlug(targetLanguage));
        break;
    case TranslationProvider::Google:
        url = googleTranslateUrl(text, languageCode(sourceLanguage),
                                 languageCode(targetLanguage));
        break;
    case TranslationProvider::MyMemory:
        url = myMemoryUrl(text, languageCode(sourceLanguage),
                          languageCode(targetLanguage));
        break;
    case TranslationProvider::Ai:
        // Routed through LuteAiAccess by the controller.
        emit requestFailed(m_providerKey,
                           QStringLiteral("AI requests are handled by the AI backend"));
        return;
    }

    QNetworkRequest networkRequest(url);
    if(isReverso)
    {
        // Best effort to look like a browser; Cloudflare may still block.
        networkRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                 QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                                "AppleWebKit/537.36 (KHTML, like Gecko) "
                                                "Chrome/124.0 Safari/537.36"));
        acceptLanguage = QStringLiteral("en-US,en;q=0.9");
        networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                    QByteArrayLiteral("text/html,application/xhtml+xml"));
    }
    else
    {
        networkRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                 QStringLiteral("LibrumLute/0.1 (translation panel)"));
    }
    if(!acceptLanguage.isEmpty())
        networkRequest.setRawHeader(QByteArrayLiteral("Accept-Language"),
                                    acceptLanguage.toUtf8());
    networkRequest.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
    m_reply = m_network->get(networkRequest);
    emit busyChanged();

    const QString sourceSlug = reversoSlug(sourceLanguage);
    const QString targetSlug = reversoSlug(targetLanguage);
    connect(m_reply, &QNetworkReply::finished, this,
            [this, key, isPhrase, isReverso, sourceSlug, targetSlug]() {
        QNetworkReply* reply = m_reply;
        if(!reply)
            return;
        m_reply = nullptr;
        reply->deleteLater();

        if(reply->error() != QNetworkReply::NoError)
        {
            finishWithError(reply->errorString());
            return;
        }

        const QByteArray payload = reply->readAll();
        QVariantMap result;
        if(m_provider == TranslationProvider::Google)
            result = parseGoogleResponse(payload, m_text);
        else if(m_provider == TranslationProvider::MyMemory)
            result = parseMyMemoryResponse(payload, m_text);
        else
            result = parseReversoHtml(QString::fromUtf8(payload), m_text,
                                      sourceSlug, targetSlug, isPhrase);

        if(result.value(QStringLiteral("translation")).toString().isEmpty())
        {
            finishWithError(isReverso
                                ? QStringLiteral(
                                      "No results parsed (page layout may have changed)")
                                : QStringLiteral("No translation returned"));
            return;
        }

        m_cache.insert(key, result);
        emit busyChanged();
        emit resultReady(result);
    });
}

void LuteTranslationAccess::cancel()
{
    if(!m_reply)
        return;
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
    emit busyChanged();
}

void LuteTranslationAccess::finishWithError(const QString& message)
{
    emit busyChanged();
    emit requestFailed(m_providerKey, message);
}

} // namespace lute
