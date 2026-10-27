#pragma once
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace application::core::utils::tools
{

// mutool is shipped next to the application binary. Resolving it via the
// binary directory is required on Linux, where QProcess only searches $PATH.
static QString mutoolBinaryPath()
{
    return QCoreApplication::applicationDirPath() + "/mutool";
}

void merge(QProcess* process, const QString& dest,
           const QList<QString>& filePaths)
{
    QString tool = mutoolBinaryPath();
    QString subCommand = "merge";

    auto args = QStringList { subCommand, "-o", dest } << filePaths;
    process->start(tool, args);
}

void extract(QProcess* process, const QString& dest, const QString& filePath,
             const QString& separator)
{
    QString tool = mutoolBinaryPath();
    QString subCommand = "clean";

    auto args = QStringList { subCommand, "-g", filePath, dest, separator };
    process->start(tool, args);
}

void convert(QProcess* process, const QString& dest, const QString& filePath)
{
    QString tool = mutoolBinaryPath();
    QString subCommand = "convert";

    auto args = QStringList { subCommand, "-o", dest, filePath };
    process->start(tool, args);
}

}  // namespace application::core::utils::tools