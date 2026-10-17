#include "lute_schema.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace lute {
namespace {

void ensureEmbeddedResources()
{
    static const bool initialised = []() {
        Q_INIT_RESOURCE(lute_data);
        return true;
    }();
    Q_UNUSED(initialised);
}

QByteArray readResource(const QString& path)
{
    ensureEmbeddedResources();
    QFile file(QStringLiteral(":/") + path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

} // namespace

QString LuteSchema::baselineSql()
{
    return QString::fromUtf8(readResource(QStringLiteral("lute/lute_baseline.sql")));
}

QString LuteSchema::languageSeedSql()
{
    return QString::fromUtf8(readResource(QStringLiteral("lute/lute_languages.sql")));
}

bool LuteSchema::createDatabase(const QString& targetPath, QString* error, bool overwrite)
{
    ensureEmbeddedResources();

    const QFileInfo info(targetPath);
    QDir parent = info.absoluteDir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("Cannot create directory '%1'").arg(parent.absolutePath());
        return false;
    }

    if (QFile::exists(targetPath)) {
        if (!overwrite) {
            if (error)
                *error = QStringLiteral("'%1' already exists").arg(targetPath);
            return false;
        }
        if (!QFile::remove(targetPath)) {
            if (error)
                *error = QStringLiteral("Cannot replace '%1'").arg(targetPath);
            return false;
        }
    }

    QFile templateFile(QStringLiteral(":/lute/lute_template.db"));
    if (!templateFile.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Embedded lute.db template is missing");
        return false;
    }
    const QByteArray bytes = templateFile.readAll();
    templateFile.close();

    QSaveFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot write '%1': %2").arg(targetPath, out.errorString());
        return false;
    }
    out.write(bytes);
    if (!out.commit()) {
        if (error)
            *error = QStringLiteral("Cannot write '%1': %2").arg(targetPath, out.errorString());
        return false;
    }

    if (!isLuteDatabase(targetPath)) {
        if (error)
            *error = QStringLiteral("New database '%1' failed validation").arg(targetPath);
        return false;
    }
    return true;
}

bool LuteSchema::isLuteDatabase(const QString& path)
{
    if (!QFileInfo::exists(path))
        return false;

    const QString connectionName =
        QStringLiteral("lute-validate-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral(
                    "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
                    "('words','languages','_migrations')"))) {
                if (query.next())
                    ok = query.value(0).toInt() == 3;
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

QString LuteSchema::defaultLibraryDirectory()
{
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString base = documents.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : documents + QStringLiteral("/Librum Lute");
    QDir().mkpath(base);
    return base;
}

} // namespace lute
