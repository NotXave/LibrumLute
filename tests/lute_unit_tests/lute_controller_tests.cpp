#include <gtest/gtest.h>

#include <QMetaObject>
#include <QSettings>
#include <QString>

#include "lute_controller.hpp"

// Regression test: QML invokes controller methods through the metaobject, so
// anything the QML layer calls must be Q_INVOKABLE or a slot. The settings
// dropdown and the term panel tabs call setTranslationProvider via QML; before
// the Q_INVOKABLE fix that call failed silently and the provider never
// changed.
TEST(LuteController, SetTranslationProviderCallableViaMetaObject)
{
    lute::LuteController controller;
    const QString previous = controller.translationProvider();
    const QString previousPersisted =
        QSettings().value("lute/translationProvider").toString();

    const QString target = previous == QStringLiteral("ai")
                               ? QStringLiteral("google")
                               : QStringLiteral("ai");

    ASSERT_TRUE(QMetaObject::invokeMethod(&controller, "setTranslationProvider",
                                          Q_ARG(QString, target)));
    EXPECT_EQ(controller.translationProvider(), target);
    EXPECT_EQ(QSettings().value("lute/translationProvider").toString(), target);

    // Restore the user's settings so the test does not pollute the registry.
    ASSERT_TRUE(QMetaObject::invokeMethod(&controller, "setTranslationProvider",
                                          Q_ARG(QString, previous)));
    if(previousPersisted.isEmpty())
        QSettings().remove("lute/translationProvider");
    else
        QSettings().setValue("lute/translationProvider", previousPersisted);
}
