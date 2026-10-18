#include <gtest/gtest.h>

#include <QMetaObject>
#include <QSettings>
#include <QString>

#include "lute_controller.hpp"

// Regression tests: QML invokes controller methods through the metaobject, so
// anything the QML layer calls must be Q_INVOKABLE or a slot. The settings
// combos and the term panel tabs call setTranslationProvider,
// setReaderPresentation and setReaderPanelSide via QML; before the
// Q_INVOKABLE fix those calls failed silently and the values never changed.
namespace {

// Switches <propertyName> through the metaobject (the exact mechanism QML
// uses), asserts property + persisted value, then restores the previous state
// so the test does not pollute the user's settings.
void expectMetaObjectSwitchWorks(const char* method, const char* propertyName,
                                 const char* settingsKey,
                                 const QString& optionA, const QString& optionB)
{
    lute::LuteController controller;
    const QString previous = controller.property(propertyName).toString();
    const QString previousPersisted = QSettings().value(settingsKey).toString();

    const QString target = previous == optionA ? optionB : optionA;

    ASSERT_TRUE(QMetaObject::invokeMethod(&controller, method,
                                          Q_ARG(QString, target)));
    EXPECT_EQ(controller.property(propertyName).toString(), target);
    EXPECT_EQ(QSettings().value(settingsKey).toString(), target);

    // Restore the user's settings so the test does not pollute the registry.
    ASSERT_TRUE(QMetaObject::invokeMethod(&controller, method,
                                          Q_ARG(QString, previous)));
    if(previousPersisted.isEmpty())
        QSettings().remove(settingsKey);
    else
        QSettings().setValue(settingsKey, previousPersisted);
}

} // namespace

TEST(LuteController, SetTranslationProviderCallableViaMetaObject)
{
    expectMetaObjectSwitchWorks("setTranslationProvider", "translationProvider",
                                "lute/translationProvider",
                                QStringLiteral("ai"), QStringLiteral("google"));
}

TEST(LuteController, SetReaderPresentationCallableViaMetaObject)
{
    expectMetaObjectSwitchWorks("setReaderPresentation", "readerPresentation",
                                "lute/readerPresentation",
                                QStringLiteral("panel"), QStringLiteral("popup"));
}

TEST(LuteController, SetReaderPanelSideCallableViaMetaObject)
{
    expectMetaObjectSwitchWorks("setReaderPanelSide", "readerPanelSide",
                                "lute/readerPanelSide",
                                QStringLiteral("left"), QStringLiteral("right"));
}

// Regression test: a config saved with another provider's default URL/model
// (e.g. switching the provider combo to Gemini while Ollama's localhost URL
// stayed in the fields) must heal to the selected provider's defaults at
// load time; the API key is preserved.
TEST(LuteController, AiConfigSelfHealsForeignProviderDefaults)
{
    QSettings settings;
    const QList<QPair<QString, QVariant>> keys = {
        {"lute/aiProvider", settings.value("lute/aiProvider")},
        {"lute/aiModel", settings.value("lute/aiModel")},
        {"lute/aiBaseUrl", settings.value("lute/aiBaseUrl")},
        {"lute/aiApiKey", settings.value("lute/aiApiKey")},
    };
    const auto restore = [&keys]() {
        for(const auto& [key, value] : keys) {
            if(value.isValid() && !value.toString().isEmpty())
                QSettings().setValue(key, value);
            else
                QSettings().remove(key);
        }
    };

    // Broken state from the field: Gemini selected, Ollama's defaults kept.
    QSettings().setValue("lute/aiProvider", QStringLiteral("gemini"));
    QSettings().setValue("lute/aiModel", QStringLiteral("qwen2.5:7b"));
    QSettings().setValue("lute/aiBaseUrl", QStringLiteral("http://localhost:11434"));
    QSettings().setValue("lute/aiApiKey", QStringLiteral("test-key"));
    {
        lute::LuteController controller;
        const QVariantMap config = controller.aiConfig();
        EXPECT_EQ(config.value("provider").toString(), QStringLiteral("gemini"));
        EXPECT_EQ(config.value("baseUrl").toString(),
                  QStringLiteral("https://generativelanguage.googleapis.com/v1beta"));
        EXPECT_EQ(config.value("model").toString(),
                  QStringLiteral("gemini-3.8-flash"));
        EXPECT_EQ(config.value("apiKey").toString(), QStringLiteral("test-key"));
    }

    // Inverse: Ollama selected with Gemini's default URL and model kept.
    // Note: ollama's default model in C++ is "qwen2.5:7b" (defaultModel in
    // lute_ai_access.cpp), which both seeded values are foreign to.
    QSettings().setValue("lute/aiProvider", QStringLiteral("ollama"));
    QSettings().setValue("lute/aiModel", QStringLiteral("gemini-2.0-flash"));
    QSettings().setValue("lute/aiBaseUrl",
                         QStringLiteral("https://generativelanguage.googleapis.com/v1beta"));
    {
        lute::LuteController controller;
        const QVariantMap config = controller.aiConfig();
        EXPECT_EQ(config.value("baseUrl").toString(),
                  QStringLiteral("http://localhost:11434"));
        EXPECT_EQ(config.value("model").toString(),
                  QStringLiteral("qwen2.5:7b"));
    }

    restore();
}
