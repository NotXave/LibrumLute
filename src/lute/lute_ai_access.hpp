#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace lute {

/// Configuration of the contextual AI translation backend (Loop 4).
struct LuteAiConfig {
    /// "ollama" (local, default), "openai" (OpenAI compatible) or "gemini".
    QString provider = QStringLiteral("ollama");
    QString baseUrl;
    QString model;
    QString apiKey;
    /// Language the suggestion is written in, e.g. "German".
    QString targetLanguage = QStringLiteral("German");
    int timeoutMs = 15000;

    bool isValid() const { return !model.trimmed().isEmpty(); }

    static LuteAiConfig defaultsFor(const QString& provider);
    QString resolvedBaseUrl() const;
    /// Name of the provider as shown in the UI.
    QString providerLabel() const;
};

/// Asynchronous, context-aware word/phrase translation.
///
/// The word plus the sentence it appears in are sent to a local Ollama server,
/// an OpenAI compatible endpoint or Google Gemini; the answer is a short
/// dictionary-style gloss that can be applied to `words.WoTranslation` with one
/// click. Nothing is cached or sent anywhere else.
class LuteAiAccess : public QObject
{
    Q_OBJECT

public:
    explicit LuteAiAccess(QObject* parent = nullptr);
    ~LuteAiAccess() override;

    void setConfig(const LuteAiConfig& config);
    const LuteAiConfig& config() const { return m_config; }

    /// True while a request is running.
    bool isBusy() const { return m_reply != nullptr; }

    void requestTranslation(const QString& word, const QString& sentence, bool isPhrase);
    void cancel();

    /// Prompt shown to the model (public for unit tests).
    static QString buildPrompt(const QString& word, const QString& sentence,
                               const QString& targetLanguage, bool isPhrase);

    /// Extract the answer from a provider JSON payload (public for unit tests).
    static std::optional<QString> parseResponse(const QByteArray& payload, const QString& provider);

signals:
    void suggestionReady(const QString& suggestion);
    void requestFailed(const QString& error);
    void busyChanged();

private:
    void finishWithError(const QString& error);

    LuteAiConfig m_config;
    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
};

} // namespace lute
