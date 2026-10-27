#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace lute
{

/// Translation sources shown as tabs in the term panel.
enum class TranslationProvider
{
    Reverso,
    Google,
    Ai,
    MyMemory,
};

/***
 * Asynchronous dictionary/machine-translation lookups for the term panel.
 *
 * request() checks an in-memory cache (key `provider|text|target`) first and
 * answers repeated tab switches synchronously. Results are provider-agnostic
 * maps with `provider`, `label`, `translation`, optional `examples`
 * (source/target pairs) and `attributionUrl`.
 *
 * Reverso has no official API; the HTML parser is best-effort and callers are
 * expected to fall back to the browser link on failure. The AI provider is
 * routed through LuteAiAccess by the controller; aiResult() builds the
 * provider-agnostic map for its suggestion.
 *
 * Privacy: only the word and the target language are sent to Google, MyMemory
 * and Reverso - never the surrounding sentence.
 */
class LuteTranslationAccess : public QObject
{
    Q_OBJECT

public:
    explicit LuteTranslationAccess(QObject* parent = nullptr);
    ~LuteTranslationAccess() override;

    void request(TranslationProvider provider, const QString& text,
                 const QString& sourceLanguage, const QString& targetLanguage,
                 bool isPhrase);
    void cancel();

    bool isBusy() const
    {
        return m_reply != nullptr;
    }

    bool hasCached(TranslationProvider provider, const QString& text,
                   const QString& targetLanguage) const;

    static QString providerKey(TranslationProvider provider);
    static QString providerLabel(TranslationProvider provider);
    static QString cacheKey(TranslationProvider provider, const QString& text,
                            const QString& targetLanguage);
    /// Provider enum for a settings key ("reverso", "google", ...).
    static TranslationProvider providerFromKey(const QString& key,
                                               TranslationProvider fallback);

    /// Lute language name ("Polish") -> ISO code ("pl"); "auto" if unknown.
    static QString languageCode(const QString& languageName);
    /// Lute language name ("Polish") -> Reverso slug ("polish").
    static QString reversoSlug(const QString& languageName);
    /// Canonical names the table covers (one per ISO code, aliases like
    /// german/deutsch deduped), for UI pickers.
    static QStringList supportedLanguages();

    static QUrl googleTranslateUrl(const QString& text,
                                   const QString& sourceCode,
                                   const QString& targetCode);
    static QUrl myMemoryUrl(const QString& text, const QString& sourceCode,
                            const QString& targetCode);
    static QUrl reversoUrl(const QString& word, const QString& sourceSlug,
                           const QString& targetSlug);
    static QVariantMap parseGoogleResponse(const QByteArray& payload,
                                           const QString& text);
    static QVariantMap parseMyMemoryResponse(const QByteArray& payload,
                                             const QString& text);
    /// Best-effort scraper: primary translation plus up to three example
    /// pairs from a context.reverso.net HTML page.
    static QVariantMap parseReversoHtml(const QString& html,
                                        const QString& word,
                                        const QString& sourceSlug,
                                        const QString& targetSlug,
                                        bool isPhrase = false);

    /// Wraps an LuteAiAccess suggestion in the shared result structure.
    static QVariantMap aiResult(const QString& suggestion,
                                const QString& label);

signals:
    void resultReady(const QVariantMap& result);
    void requestFailed(const QString& providerKey, const QString& message);
    void busyChanged();

private:
    void finishWithError(const QString& message);

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    QHash<QString, QVariantMap> m_cache;
    TranslationProvider m_provider = TranslationProvider::Google;
    QString m_providerKey;
    QString m_text;
    QString m_targetLanguage;
};

}  // namespace lute
