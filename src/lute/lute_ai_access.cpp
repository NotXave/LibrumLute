#include "lute_ai_access.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace lute {
namespace {

QString defaultBaseUrl(const QString& provider)
{
    if (provider == QLatin1String("openai"))
        return QStringLiteral("https://api.openai.com/v1");
    if (provider == QLatin1String("gemini"))
        return QStringLiteral("https://generativelanguage.googleapis.com/v1beta");
    return QStringLiteral("http://localhost:11434");
}

QString defaultModel(const QString& provider)
{
    if (provider == QLatin1String("openai"))
        return QStringLiteral("gpt-4o-mini");
    if (provider == QLatin1String("gemini"))
        return QStringLiteral("gemini-2.0-flash");
    return QStringLiteral("qwen2.5:7b");
}

/// Strips the noise models like to add around a one-line answer.
QString cleanSuggestion(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1String("```"))) {
        const int firstNewline = text.indexOf(QLatin1Char('\n'));
        if (firstNewline > 0)
            text = text.mid(firstNewline + 1);
        const int closing = text.lastIndexOf(QLatin1String("```"));
        if (closing >= 0)
            text = text.left(closing);
        text = text.trimmed();
    }
    if (text.size() >= 2) {
        const QChar first = text.front();
        const QChar last = text.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"'))
            || (first == QChar(0x201C) && last == QChar(0x201D)))
            text = text.mid(1, text.size() - 2).trimmed();
    }
    return text;
}

} // namespace

LuteAiConfig LuteAiConfig::defaultsFor(const QString& provider)
{
    LuteAiConfig config;
    config.provider = provider;
    config.baseUrl = defaultBaseUrl(provider);
    config.model = defaultModel(provider);
    return config;
}

QString LuteAiConfig::resolvedBaseUrl() const
{
    const QString trimmed = baseUrl.trimmed();
    if (!trimmed.isEmpty())
        return trimmed;
    return defaultBaseUrl(provider);
}

QString LuteAiConfig::providerLabel() const
{
    if (provider == QLatin1String("openai"))
        return QStringLiteral("OpenAI compatible");
    if (provider == QLatin1String("gemini"))
        return QStringLiteral("Google Gemini");
    return QStringLiteral("Ollama (local)");
}

LuteAiAccess::LuteAiAccess(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_config = LuteAiConfig::defaultsFor(QStringLiteral("ollama"));
}

LuteAiAccess::~LuteAiAccess()
{
    cancel();
}

void LuteAiAccess::setConfig(const LuteAiConfig& config)
{
    m_config = config;
    if (m_config.model.trimmed().isEmpty())
        m_config.model = defaultModel(m_config.provider);
}

QString LuteAiAccess::buildPrompt(const QString& word, const QString& sentence,
                                         const QString& targetLanguage, bool isPhrase)
{
    const QString kind = isPhrase ? QStringLiteral("expression") : QStringLiteral("word");
    return QStringLiteral(
               "You are a bilingual dictionary for language learners.\n"
               "Give the meaning of the %1 \"%2\" as it is used in the sentence below, "
               "translated into %3.\n"
               "Rules:\n"
               "- Answer with a short gloss only: 1 to 5 words.\n"
               "- If it is a verb, add the infinitive in parentheses.\n"
               "- Do not quote the answer, do not explain, do not add a sentence.\n"
               "\n"
               "Sentence: %4\n"
               "Answer:")
        .arg(kind, word, targetLanguage, sentence);
}

std::optional<QString> LuteAiAccess::parseResponse(const QByteArray& payload,
                                                          const QString& provider)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject root = document.object();

    QString answer;
    if (provider == QLatin1String("gemini")) {
        const QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
        if (!candidates.isEmpty()) {
            const QJsonObject content = candidates.first().toObject().value(QStringLiteral("content")).toObject();
            const QJsonArray parts = content.value(QStringLiteral("parts")).toArray();
            if (!parts.isEmpty())
                answer = parts.first().toObject().value(QStringLiteral("text")).toString();
        }
    } else if (root.contains(QStringLiteral("choices"))) {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
            const QJsonObject message =
                choices.first().toObject().value(QStringLiteral("message")).toObject();
            answer = message.value(QStringLiteral("content")).toString();
        }
    } else if (root.contains(QStringLiteral("response"))) {
        answer = root.value(QStringLiteral("response")).toString();
    }

    if (answer.trimmed().isEmpty())
        return std::nullopt;
    return cleanSuggestion(answer);
}

void LuteAiAccess::cancel()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
    emit busyChanged();
}

void LuteAiAccess::finishWithError(const QString& error)
{
    emit busyChanged();
    emit requestFailed(error);
}

void LuteAiAccess::requestTranslation(const QString& word, const QString& sentence,
                                             bool isPhrase)
{
    if (isBusy())
        cancel();

    const QString trimmedWord = word.trimmed();
    if (trimmedWord.isEmpty()) {
        emit requestFailed(QStringLiteral("No word selected"));
        return;
    }
    if (m_config.model.trimmed().isEmpty()) {
        emit requestFailed(QStringLiteral("No AI model configured"));
        return;
    }

    const QString prompt = buildPrompt(trimmedWord, sentence.trimmed(),
                                       m_config.targetLanguage, isPhrase);

    const QString baseUrl = m_config.resolvedBaseUrl();
    QNetworkRequest request;
    QByteArray body;
    const QString provider = m_config.provider;

    if (provider == QLatin1String("gemini")) {
        QUrl url(baseUrl + QStringLiteral("/models/%1:generateContent").arg(m_config.model));
        if (!m_config.apiKey.trimmed().isEmpty())
            url.setQuery(QStringLiteral("key=%1").arg(m_config.apiKey.trimmed()));
        request.setUrl(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QJsonObject part{{QStringLiteral("text"), prompt}};
        QJsonObject content{{QStringLiteral("parts"), QJsonArray{part}}};
        QJsonObject payload{{QStringLiteral("contents"), QJsonArray{content}}};
        QJsonObject generation{{QStringLiteral("temperature"), 0.2},
                               {QStringLiteral("maxOutputTokens"), 64}};
        payload.insert(QStringLiteral("generationConfig"), generation);
        body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    } else if (provider == QLatin1String("openai")) {
        request.setUrl(QUrl(baseUrl + QStringLiteral("/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        if (!m_config.apiKey.trimmed().isEmpty()) {
            request.setRawHeader("Authorization",
                                 QByteArray("Bearer ") + m_config.apiKey.trimmed().toUtf8());
        }

        QJsonObject systemMessage{
            {QStringLiteral("role"), QStringLiteral("system")},
            {QStringLiteral("content"),
             QStringLiteral("You are a concise bilingual dictionary. Answer with a short gloss only.")}};
        QJsonObject userMessage{{QStringLiteral("role"), QStringLiteral("user")},
                                {QStringLiteral("content"), prompt}};
        QJsonObject payload{
            {QStringLiteral("model"), m_config.model},
            {QStringLiteral("messages"), QJsonArray{systemMessage, userMessage}},
            {QStringLiteral("temperature"), 0.2},
            {QStringLiteral("max_tokens"), 64}};
        body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    } else {
        request.setUrl(QUrl(baseUrl + QStringLiteral("/api/generate")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        QJsonObject payload{
            {QStringLiteral("model"), m_config.model},
            {QStringLiteral("prompt"), prompt},
            {QStringLiteral("stream"), false},
            {QStringLiteral("options"),
             QJsonObject{{QStringLiteral("temperature"), 0.2},
                         {QStringLiteral("num_predict"), 64}}}};
        body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    }

    request.setTransferTimeout(qMax(1000, m_config.timeoutMs));

    m_reply = m_network->post(request, body);
    emit busyChanged();

    connect(m_reply, &QNetworkReply::finished, this, [this, provider]() {
        QNetworkReply* reply = m_reply;
        if (!reply)
            return;
        m_reply = nullptr;
        reply->deleteLater();

        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString error =
                QStringLiteral("AI request failed: %1").arg(reply->errorString());
            finishWithError(error);
            return;
        }
        const auto suggestion = parseResponse(payload, provider);
        emit busyChanged();
        if (!suggestion) {
            emit requestFailed(QStringLiteral("The AI answer could not be read: %1")
                                   .arg(QString::fromUtf8(payload.left(200))));
            return;
        }
        emit suggestionReady(*suggestion);
    });
}

} // namespace lute
