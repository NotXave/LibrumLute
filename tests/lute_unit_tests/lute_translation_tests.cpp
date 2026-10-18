#include <gtest/gtest.h>

#include <QUrlQuery>

#include "lute_translation_access.hpp"

using namespace lute;

TEST(LuteTranslation, GoogleUrlCarriesLanguagesAndText)
{
    const QUrl url = LuteTranslationAccess::googleTranslateUrl(
        QStringLiteral("Biblioteczny przestrzeń"), QStringLiteral("pl"),
        QStringLiteral("de"));
    EXPECT_TRUE(url.toString().startsWith(
        QStringLiteral("https://translate.googleapis.com/translate_a/single")));
    const QUrlQuery query(url);
    EXPECT_EQ(query.queryItemValue(QStringLiteral("sl")), QStringLiteral("pl"));
    EXPECT_EQ(query.queryItemValue(QStringLiteral("tl")), QStringLiteral("de"));
    EXPECT_EQ(query.queryItemValue(QStringLiteral("q")),
              QStringLiteral("Biblioteczny przestrzeń"));
    EXPECT_TRUE(query.hasQueryItem(QStringLiteral("client")));
    EXPECT_TRUE(query.hasQueryItem(QStringLiteral("dt")));
}

TEST(LuteTranslation, MapsLanguageNamesToIsoCodes)
{
    EXPECT_EQ(LuteTranslationAccess::languageCode(QStringLiteral("Polish")),
              QStringLiteral("pl"));
    EXPECT_EQ(LuteTranslationAccess::languageCode(QStringLiteral("German")),
              QStringLiteral("de"));
    EXPECT_EQ(LuteTranslationAccess::languageCode(QStringLiteral("English")),
              QStringLiteral("en"));
    // Unknown languages fall back to auto-detection.
    EXPECT_EQ(LuteTranslationAccess::languageCode(QStringLiteral("Klingon")),
              QStringLiteral("auto"));
}

TEST(LuteTranslation, MapsLanguageNamesToReversoSlugs)
{
    EXPECT_EQ(LuteTranslationAccess::reversoSlug(QStringLiteral("Polish")),
              QStringLiteral("polish"));
    EXPECT_EQ(LuteTranslationAccess::reversoSlug(QStringLiteral("German")),
              QStringLiteral("german"));
    // Unknown languages fall back to english.
    EXPECT_EQ(LuteTranslationAccess::reversoSlug(QStringLiteral("Klingon")),
              QStringLiteral("english"));
}

TEST(LuteTranslation, ReversoUrlEncodesWordAndSlugs)
{
    const QUrl url = LuteTranslationAccess::reversoUrl(
        QStringLiteral("Biblioteka publiczna"), QStringLiteral("polish"),
        QStringLiteral("german"));
    EXPECT_EQ(url.host(), QStringLiteral("context.reverso.net"));
    EXPECT_TRUE(url.path().startsWith(QStringLiteral("/translation/polish-german/")));
    EXPECT_TRUE(QString::fromUtf8(url.toEncoded()).endsWith(
        QStringLiteral("Biblioteka%20publiczna")));
}

TEST(LuteTranslation, ParsesGoogleResponse)
{
    const QByteArray payload =
        R"([[["Bibliothek","Biblioteczny",null,null,11]],null,"pl"])";
    const QVariantMap result = LuteTranslationAccess::parseGoogleResponse(
        payload, QStringLiteral("Biblioteczny"));

    EXPECT_EQ(result.value(QStringLiteral("provider")).toString(),
              QStringLiteral("google"));
    EXPECT_EQ(result.value(QStringLiteral("translation")).toString(),
              QStringLiteral("Bibliothek"));
    EXPECT_FALSE(result.value(QStringLiteral("attributionUrl")).toString().isEmpty());
}

TEST(LuteTranslation, ParsesMyMemoryResponse)
{
    const QByteArray payload =
        R"({"responseData":{"translatedText":"Bibliothek","match":0.85}})";
    const QVariantMap result = LuteTranslationAccess::parseMyMemoryResponse(
        payload, QStringLiteral("Biblioteczny"));

    EXPECT_EQ(result.value(QStringLiteral("provider")).toString(),
              QStringLiteral("mymemory"));
    EXPECT_EQ(result.value(QStringLiteral("translation")).toString(),
              QStringLiteral("Bibliothek"));
}

TEST(LuteTranslation, ParsesReversoDictionaryAndExamples)
{
    const QString html = QStringLiteral(
        "<html><body>"
        "<div id=\"dictionaryentries\">"
        "<a class=\"translation dict\" "
        "href=\"https://dictionary.reverso.net/spanish-english/library\">library"
        "</a>"
        "</div>"
        "<section id=\"examples-content\">"
        "<div class=\"example\"><p class=\"src ltr\">"
        "<span class=\"text\">Voy a la &amp;librer&iacute;a.</span></p>"
        "<p class=\"trg ltr\"><span class=\"text\">"
        "I'm going to the bookstore.</span></p></div>"
        "<div class=\"example\"><p class=\"src ltr\">"
        "<span class=\"text\">La librer&iacute;a est&aacute; abierta.</span></p>"
        "<p class=\"trg ltr\"><span class=\"text\">"
        "The bookstore is open.</span></p></div>"
        "</section></body></html>");
    const QVariantMap result = LuteTranslationAccess::parseReversoHtml(
        html, QStringLiteral("librería"), QStringLiteral("spanish"),
        QStringLiteral("english"));

    EXPECT_EQ(result.value(QStringLiteral("provider")).toString(),
              QStringLiteral("reverso"));
    EXPECT_EQ(result.value(QStringLiteral("translation")).toString(),
              QStringLiteral("library"));
    const QVariantList examples = result.value(QStringLiteral("examples")).toList();
    ASSERT_EQ(examples.size(), 2);
    const QVariantMap first = examples.first().toMap();
    EXPECT_EQ(first.value(QStringLiteral("source")).toString(),
              QStringLiteral("Voy a la &librería."));
    EXPECT_EQ(first.value(QStringLiteral("target")).toString(),
              QStringLiteral("I'm going to the bookstore."));
    EXPECT_TRUE(result.value(QStringLiteral("attributionUrl"))
                    .toString()
                    .contains(QStringLiteral("spanish-english")));
}

TEST(LuteTranslation, ReversoFallsBackToHeaderText)
{
    const QString html = QStringLiteral(
        "<div><h2>Translation of librería</h2>"
        "<div class=\"translate-all\">bookshop</div></div>"
        "<div>Show more</div>");
    const QVariantMap result = LuteTranslationAccess::parseReversoHtml(
        html, QStringLiteral("librería"), QStringLiteral("spanish"),
        QStringLiteral("english"));

    // The word itself is dropped, the next token becomes the translation.
    EXPECT_EQ(result.value(QStringLiteral("translation")).toString(),
              QStringLiteral("bookshop"));
}

TEST(LuteTranslation, ReversoPhraseUsesFirstExample)
{
    const QString html = QStringLiteral(
        "<section id=\"examples-content\">"
        "<div class=\"example\"><p class=\"src ltr\">voy a la librería</p>"
        "<p class=\"trg ltr\">I'm going to the bookstore</p></div>"
        "</section>");
    const QVariantMap result = LuteTranslationAccess::parseReversoHtml(
        html, QStringLiteral("voy a la librería"), QStringLiteral("spanish"),
        QStringLiteral("english"), true);

    EXPECT_EQ(result.value(QStringLiteral("translation")).toString(),
              QStringLiteral("I'm going to the bookstore"));
    EXPECT_EQ(result.value(QStringLiteral("examples")).toList().size(), 1);
}

TEST(LuteTranslation, BrokenPayloadsYieldEmptyTranslations)
{
    EXPECT_TRUE(LuteTranslationAccess::parseGoogleResponse("{", QStringLiteral("x"))
                    .value(QStringLiteral("translation"))
                    .toString()
                    .isEmpty());
    EXPECT_TRUE(LuteTranslationAccess::parseMyMemoryResponse("{", QStringLiteral("x"))
                    .value(QStringLiteral("translation"))
                    .toString()
                    .isEmpty());
    EXPECT_TRUE(LuteTranslationAccess::parseMyMemoryResponse(
                    QByteArrayLiteral("gateway timeout"),
                    QStringLiteral("x"))
                    .value(QStringLiteral("translation"))
                    .toString()
                    .isEmpty());
}

TEST(LuteTranslation, ReversoGarbageHtmlYieldsEmptyButValidResult)
{
    const QVariantMap result = LuteTranslationAccess::parseReversoHtml(
        QStringLiteral(""), QStringLiteral("libro"), QStringLiteral("spanish"),
        QStringLiteral("english"));
    EXPECT_TRUE(result.value(QStringLiteral("translation")).toString().isEmpty());
    EXPECT_FALSE(result.value(QStringLiteral("attributionUrl")).toString().isEmpty());

    const QVariantMap blocked = LuteTranslationAccess::parseReversoHtml(
        QStringLiteral("<html><body>Access denied</body></html>"),
        QStringLiteral("libro"), QStringLiteral("spanish"),
        QStringLiteral("english"));
    EXPECT_TRUE(blocked.value(QStringLiteral("translation")).toString().isEmpty());
}

TEST(LuteTranslation, ProviderKeysAndLabelsRoundTrip)
{
    EXPECT_EQ(LuteTranslationAccess::providerKey(TranslationProvider::Reverso),
              QStringLiteral("reverso"));
    EXPECT_EQ(LuteTranslationAccess::providerKey(TranslationProvider::Ai),
              QStringLiteral("ai"));
    EXPECT_EQ(LuteTranslationAccess::providerFromKey(
                  QStringLiteral("mymemory"), TranslationProvider::Google),
              TranslationProvider::MyMemory);
    EXPECT_EQ(LuteTranslationAccess::providerFromKey(
                  QStringLiteral("nonsense"), TranslationProvider::Google),
              TranslationProvider::Google);

    const QVariantMap ai = LuteTranslationAccess::aiResult(
        QStringLiteral("  Bibliothek; Aufbewahrungsort  "),
        QStringLiteral("Ollama"));
    EXPECT_EQ(ai.value(QStringLiteral("provider")).toString(),
              QStringLiteral("ai"));
    EXPECT_EQ(ai.value(QStringLiteral("label")).toString(),
              QStringLiteral("Ollama"));
    EXPECT_EQ(ai.value(QStringLiteral("translation")).toString(),
              QStringLiteral("Bibliothek; Aufbewahrungsort"));

    EXPECT_EQ(LuteTranslationAccess::cacheKey(
                  TranslationProvider::Google, QStringLiteral(" Haus "),
                  QStringLiteral("German")),
              QStringLiteral("google| Haus |german"));
}
