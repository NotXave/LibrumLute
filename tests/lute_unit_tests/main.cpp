#include <gtest/gtest.h>

#include <QCoreApplication>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    // Mirror the application's QSettings identity (src/main.cpp) so tests read
    // and write the same registry location as the app.
    QCoreApplication::setOrganizationName("Librum-Reader");
    QCoreApplication::setApplicationName("Librum");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
