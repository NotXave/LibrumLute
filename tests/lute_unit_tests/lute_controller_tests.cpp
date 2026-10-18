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
