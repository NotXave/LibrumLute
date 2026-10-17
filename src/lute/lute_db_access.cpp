#include "lute_db_access.hpp"

#include <QFile>
#include <QTextStream>

#include "lute_text_parser.hpp"
#include "lute_schema.hpp"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaType>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace lute {
namespace {

constexpr int kSqlChunk = 400;

QDateTime parseLuteDateTime(const QVariant& value)
{
    if (!value.isValid() || value.isNull())
        return {};
    if (value.canConvert<QDateTime>())
        return value.toDateTime();
    const QString text = value.toString().trimmed();
    if (text.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODate);
    return dt;
}

QString difficultyToSql(const QString& value)
{
    return value.trimmed().isEmpty() ? QString() : value;
}

} // namespace

struct LuteDbAccess::Impl {
    QSqlDatabase db;
    QString connectionName;
    QString path;
    QString lastError;
    QHash<int, LuteLanguage> languageCache;

    LuteLanguage languageOrDefault(int languageId)
    {
        if (languageCache.contains(languageId))
            return languageCache.value(languageId);
        LuteLanguage language;
        language.id = languageId;
        language.name = QStringLiteral("Generic");
        // Lute's defaults for a language without an explicit row.
        language.characterSubstitutions = QStringLiteral("´='|`='|’='|‘='|...=…|..=‥");
        language.regexpSplitSentences = QStringLiteral(".!?");
        language.regexpWordCharacters.clear();
        language.parserType = QStringLiteral("spacedel");
        return language;
    }
};

LuteDbAccess::LuteDbAccess()
    : m_impl(std::make_unique<Impl>())
{
}

LuteDbAccess::~LuteDbAccess()
{
    close();
}

bool LuteDbAccess::open(const QString& dbPath, QString* error)
{
    close();

    if (!QFileInfo::exists(dbPath)) {
        const QString message = QStringLiteral("Database '%1' does not exist").arg(dbPath);
        m_impl->lastError = message;
        if (error)
            *error = message;
        return false;
    }
    if (!LuteSchema::isLuteDatabase(dbPath)) {
        const QString message =
            QStringLiteral("'%1' is not a Lute v3 database (missing words/languages/_migrations)")
                .arg(dbPath);
        m_impl->lastError = message;
        if (error)
            *error = message;
        return false;
    }

    m_impl->connectionName =
        QStringLiteral("lute-access-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_impl->connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        const QString message =
            QStringLiteral("Cannot open '%1': %2").arg(dbPath, db.lastError().text());
        QSqlDatabase::removeDatabase(m_impl->connectionName);
        m_impl->connectionName.clear();
        m_impl->lastError = message;
        if (error)
            *error = message;
        return false;
    }

    {
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    }

    m_impl->db = db;
    m_impl->path = dbPath;
    m_impl->lastError.clear();
    m_impl->languageCache.clear();
    return true;
}

void LuteDbAccess::close()
{
    if (!m_impl)
        return;
    if (m_impl->db.isValid()) {
        m_impl->db.close();
        m_impl->db = QSqlDatabase();
    }
    if (!m_impl->connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_impl->connectionName);
        m_impl->connectionName.clear();
    }
    m_impl->path.clear();
    m_impl->languageCache.clear();
}

bool LuteDbAccess::isOpen() const
{
    return m_impl && m_impl->db.isValid() && m_impl->db.isOpen();
}

QString LuteDbAccess::path() const
{
    return m_impl->path;
}

QString LuteDbAccess::lastError() const
{
    return m_impl->lastError;
}

QVector<LuteLanguage> LuteDbAccess::languages()
{
    QVector<LuteLanguage> result;
    if (!isOpen())
        return result;

    QSqlQuery query(m_impl->db);
    if (!query.exec(QStringLiteral(
            "SELECT LgID, LgName, LgCharacterSubstitutions, LgRegexpSplitSentences, "
            "LgExceptionsSplitSentences, LgRegexpWordCharacters, LgRightToLeft, "
            "LgShowRomanization, LgParserType FROM languages ORDER BY LgName"))) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        LuteLanguage language;
        language.id = query.value(0).toInt();
        language.name = query.value(1).toString();
        language.characterSubstitutions = query.value(2).toString();
        language.regexpSplitSentences = query.value(3).toString();
        language.exceptionsSplitSentences = query.value(4).toString();
        language.regexpWordCharacters = query.value(5).toString();
        language.rightToLeft = query.value(6).toInt() != 0;
        language.showRomanization = query.value(7).toInt() != 0;
        language.parserType = query.value(8).toString();
        m_impl->languageCache.insert(language.id, language);
        result.append(language);
    }
    return result;
}

std::optional<LuteLanguage> LuteDbAccess::languageById(int languageId)
{
    if (!isOpen() || languageId <= 0)
        return std::nullopt;
    if (m_impl->languageCache.contains(languageId))
        return m_impl->languageCache.value(languageId);

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT LgID, LgName, LgCharacterSubstitutions, LgRegexpSplitSentences, "
        "LgExceptionsSplitSentences, LgRegexpWordCharacters, LgRightToLeft, "
        "LgShowRomanization, LgParserType FROM languages WHERE LgID = ?"));
    query.addBindValue(languageId);
    if (!query.exec() || !query.next())
        return std::nullopt;

    LuteLanguage language;
    language.id = query.value(0).toInt();
    language.name = query.value(1).toString();
    language.characterSubstitutions = query.value(2).toString();
    language.regexpSplitSentences = query.value(3).toString();
    language.exceptionsSplitSentences = query.value(4).toString();
    language.regexpWordCharacters = query.value(5).toString();
    language.rightToLeft = query.value(6).toInt() != 0;
    language.showRomanization = query.value(7).toInt() != 0;
    language.parserType = query.value(8).toString();
    m_impl->languageCache.insert(language.id, language);
    return language;
}

bool LuteDbAccess::importTermsTsv(const QString& path, int languageId,
                                 int* imported, QString* error)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if(error)
            *error = file.errorString();
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    int count = 0;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for(const QString& rawLine : lines)
    {
        const QString line = rawLine.trimmed();
        if(line.isEmpty())
            continue;

        const QStringList columns = line.split(QLatin1Char('\t'));
        if(columns.size() < 1)
            continue;
        if(columns.at(0).compare(QStringLiteral("WoText"), Qt::CaseInsensitive) == 0)
            continue;

        LuteTerm term;
        term.languageId = languageId;
        term.text = columns.at(0).trimmed();
        if(term.text.isEmpty())
            continue;
        term.translation = columns.size() > 1 ? columns.at(1) : QString();
        term.status = columns.size() > 2 ? columns.at(2).toInt() : 0;
        term.tokenCount = columns.size() > 3 ? columns.at(3).toInt() : 1;
        if(term.tokenCount <= 0)
            term.tokenCount = 1;

        QString saveError;
        if(!saveTerm(term, &saveError))
        {
            if(error)
                *error = QStringLiteral("Line %1: %2").arg(count + 1).arg(saveError);
            return false;
        }
        ++count;
    }

    if(imported)
        *imported = count;
    return true;
}

bool LuteDbAccess::backupTo(const QString& path, QString* error)
{
    if(!m_impl->db.isOpen())
        return false;

    // VACUUM INTO writes a consistent copy while the database stays open.
    QFile::remove(path);
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QLatin1String("''"));

    QSqlQuery query(m_impl->db);
    if(!query.exec(QStringLiteral("VACUUM INTO '") + escaped
                   + QLatin1Char('\'')))
    {
        if(error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool LuteDbAccess::exportTermsTsv(const QString& path, int languageId,
                                 QString* error)
{
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if(error)
            *error = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "WoText\tWoTranslation\tWoStatus\tWoTokenCount\n";

    const QVector<LuteTerm> all = terms(languageId, QString(), 0, 100000);
    for(const LuteTerm& term : all)
    {
        QString translation = term.translation;
        translation.replace(QLatin1Char('\t'), QLatin1Char(' '));
        translation.replace(QLatin1Char('\n'), QLatin1Char(' '));
        stream << term.text << '\t' << translation << '\t' << term.status
               << '\t' << term.tokenCount << '\n';
    }
    file.close();
    return true;
}

bool LuteDbAccess::saveLanguage(LuteLanguage& language, QString* error)
{
    if(!m_impl->db.isOpen())
        return false;

    // Lute's columns are NOT NULL; default-constructed QStrings are null and
    // would be bound as SQL NULL.
    auto text = [](QString value) {
        return value.isNull() ? QStringLiteral("") : value;
    };
    language.characterSubstitutions = text(language.characterSubstitutions);
    language.regexpSplitSentences = text(language.regexpSplitSentences);
    language.exceptionsSplitSentences = text(language.exceptionsSplitSentences);
    language.regexpWordCharacters = text(language.regexpWordCharacters);
    language.parserType = text(language.parserType);

    QSqlQuery query(m_impl->db);
    if(language.id > 0)
    {
        query.prepare(QStringLiteral(
            "UPDATE languages SET LgName = ?, LgCharacterSubstitutions = ?, "
            "LgRegexpSplitSentences = ?, LgExceptionsSplitSentences = ?, "
            "LgRegexpWordCharacters = ?, LgRightToLeft = ?, "
            "LgShowRomanization = ?, LgParserType = ? WHERE LgID = ?"));
    }
    else
    {
        query.prepare(QStringLiteral(
            "INSERT INTO languages (LgName, LgCharacterSubstitutions, "
            "LgRegexpSplitSentences, LgExceptionsSplitSentences, "
            "LgRegexpWordCharacters, LgRightToLeft, LgShowRomanization, "
            "LgParserType) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    }

    query.addBindValue(language.name);
    query.addBindValue(language.characterSubstitutions);
    query.addBindValue(language.regexpSplitSentences);
    query.addBindValue(language.exceptionsSplitSentences);
    query.addBindValue(language.regexpWordCharacters);
    query.addBindValue(language.rightToLeft ? 1 : 0);
    query.addBindValue(language.showRomanization ? 1 : 0);
    query.addBindValue(language.parserType);
    if(language.id > 0)
        query.addBindValue(language.id);

    if(!query.exec())
    {
        const QString message = query.lastError().text();
        if(error)
            *error = message;
        m_impl->lastError = message;
        return false;
    }
    if(language.id <= 0)
        language.id = query.lastInsertId().toInt();
    m_impl->languageCache.clear();
    return true;
}

bool LuteDbAccess::deleteLanguage(int languageId, QString* error)
{
    if(!m_impl->db.isOpen())
        return false;

    QSqlQuery usage(m_impl->db);
    usage.prepare(QStringLiteral(
        "SELECT (SELECT COUNT(*) FROM words WHERE WoLgID = ?) + "
        "(SELECT COUNT(*) FROM books WHERE BkLgID = ?)"));
    usage.addBindValue(languageId);
    usage.addBindValue(languageId);
    if(usage.exec() && usage.next() && usage.value(0).toInt() > 0)
    {
        if(error)
            *error = QStringLiteral("The language still has terms or books");
        return false;
    }

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("DELETE FROM languages WHERE LgID = ?"));
    query.addBindValue(languageId);
    if(!query.exec())
    {
        if(error)
            *error = query.lastError().text();
        return false;
    }
    m_impl->languageCache.clear();
    return true;
}

std::optional<LuteLanguage> LuteDbAccess::languageByName(const QString& name)
{
    if (!isOpen())
        return std::nullopt;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT LgID, LgName, LgCharacterSubstitutions, LgRegexpSplitSentences, "
        "LgExceptionsSplitSentences, LgRegexpWordCharacters, LgRightToLeft, "
        "LgShowRomanization, LgParserType FROM languages WHERE LgName = ?"));
    query.addBindValue(name);
    if (!query.exec() || !query.next())
        return std::nullopt;

    LuteLanguage language;
    language.id = query.value(0).toInt();
    language.name = query.value(1).toString();
    language.characterSubstitutions = query.value(2).toString();
    language.regexpSplitSentences = query.value(3).toString();
    language.exceptionsSplitSentences = query.value(4).toString();
    language.regexpWordCharacters = query.value(5).toString();
    language.rightToLeft = query.value(6).toInt() != 0;
    language.showRomanization = query.value(7).toInt() != 0;
    language.parserType = query.value(8).toString();
    m_impl->languageCache.insert(language.id, language);
    return language;
}

namespace {

QString lowerKey(const LuteLanguage& language, const QString& text)
{
    return LuteTextParser(language).lowercase(text);
}

LuteTerm readTermRow(const QSqlQuery& query)
{
    LuteTerm term;
    term.id = query.value(0).toLongLong();
    term.languageId = query.value(1).toInt();
    term.text = query.value(2).toString();
    term.textLC = query.value(3).toString();
    term.status = query.value(4).toInt();
    term.translation = query.value(5).toString();
    term.romanization = query.value(6).toString();
    term.tokenCount = query.value(7).toInt();
    term.created = parseLuteDateTime(query.value(8));
    term.statusChanged = parseLuteDateTime(query.value(9));
    term.syncStatus = query.value(10).toInt();
    return term;
}

const char* kTermColumns =
    "WoID, WoLgID, WoText, WoTextLC, WoStatus, WoTranslation, WoRomanization, "
    "WoTokenCount, WoCreated, WoStatusChanged, WoSyncStatus";

} // namespace

std::optional<LuteTerm> LuteDbAccess::getTermByText(const QString& word, int languageId)
{
    if (!isOpen() || word.isEmpty() || languageId <= 0)
        return std::nullopt;

    const QString key = lowerKey(m_impl->languageOrDefault(languageId), word);
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("SELECT %1 FROM words WHERE WoTextLC = ? AND WoLgID = ?")
                      .arg(QString::fromLatin1(kTermColumns)));
    query.addBindValue(key);
    query.addBindValue(languageId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return readTermRow(query);
}

QHash<QString, int> LuteDbAccess::getTermsForPage(const QStringList& words, int languageId)
{
    QHash<QString, int> result;
    if (!isOpen() || languageId <= 0 || words.isEmpty())
        return result;

    // Deduplicate and case-fold first, so the query stays small.
    QStringList keys;
    keys.reserve(words.size());
    QSet<QString> seen;
    for (const QString& word : words) {
        const QString key = lowerKey(m_impl->languageOrDefault(languageId), word);
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        keys.append(key);
    }

    for (int offset = 0; offset < keys.size(); offset += kSqlChunk) {
        const QStringList chunk = keys.mid(offset, kSqlChunk);
        QStringList placeholders;
        placeholders.reserve(chunk.size());
        for (int i = 0; i < chunk.size(); ++i)
            placeholders << QStringLiteral("?");

        QSqlQuery query(m_impl->db);
        query.prepare(QStringLiteral(
                          "SELECT WoTextLC, WoStatus FROM words WHERE WoLgID = ? AND WoTextLC IN (%1)")
                          .arg(placeholders.join(QLatin1Char(','))));
        query.addBindValue(languageId);
        for (const QString& key : chunk)
            query.addBindValue(key);
        if (!query.exec()) {
            m_impl->lastError = query.lastError().text();
            return result;
        }
        while (query.next())
            result.insert(query.value(0).toString(), query.value(1).toInt());
    }
    return result;
}

QHash<QString, LuteTerm> LuteDbAccess::getTermMap(const QStringList& words, int languageId)
{
    QHash<QString, LuteTerm> result;
    if (!isOpen() || languageId <= 0 || words.isEmpty())
        return result;

    QStringList keys;
    QSet<QString> seen;
    for (const QString& word : words) {
        const QString key = lowerKey(m_impl->languageOrDefault(languageId), word);
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        keys.append(key);
    }

    for (int offset = 0; offset < keys.size(); offset += kSqlChunk) {
        const QStringList chunk = keys.mid(offset, kSqlChunk);
        QStringList placeholders;
        for (int i = 0; i < chunk.size(); ++i)
            placeholders << QStringLiteral("?");

        QSqlQuery query(m_impl->db);
        query.prepare(QStringLiteral("SELECT %1 FROM words WHERE WoLgID = ? AND WoTextLC IN (%2)")
                          .arg(QString::fromLatin1(kTermColumns), placeholders.join(QLatin1Char(','))));
        query.addBindValue(languageId);
        for (const QString& key : chunk)
            query.addBindValue(key);
        if (!query.exec()) {
            m_impl->lastError = query.lastError().text();
            return result;
        }
        while (query.next()) {
            const LuteTerm term = readTermRow(query);
            result.insert(term.textLC, term);
        }
    }
    return result;
}

QVector<LuteTerm> LuteDbAccess::terms(int languageId, const QString& search,
                                     int status, int limit)
{
    QVector<LuteTerm> result;
    if(!m_impl->db.isOpen())
        return result;

    QString sql = QStringLiteral("SELECT %1 FROM words WHERE WoLgID = ?")
                      .arg(QString::fromLatin1(kTermColumns));
    if(!search.trimmed().isEmpty())
        sql += QStringLiteral(" AND WoTextLC LIKE ?");
    if(status > 0)
        sql += QStringLiteral(" AND WoStatus = ?");
    sql += QStringLiteral(" ORDER BY WoStatus, WoTextLC LIMIT ?");

    QSqlQuery query(m_impl->db);
    query.prepare(sql);
    query.addBindValue(languageId);
    if(!search.trimmed().isEmpty())
        query.addBindValue(QStringLiteral("%%1%").arg(search.trimmed().toLower()));
    if(status > 0)
        query.addBindValue(status);
    query.addBindValue(limit > 0 ? limit : 500);

    if(!query.exec())
    {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while(query.next())
        result.append(readTermRow(query));
    return result;
}

std::optional<LuteTerm> LuteDbAccess::termById(qint64 termId)
{
    if(!m_impl->db.isOpen())
        return std::nullopt;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("SELECT %1 FROM words WHERE WoID = ?")
                      .arg(QString::fromLatin1(kTermColumns)));
    query.addBindValue(termId);
    if(!query.exec() || !query.next())
        return std::nullopt;
    return readTermRow(query);
}

QVector<LuteTerm> LuteDbAccess::multiWordTerms(int languageId)
{
    QVector<LuteTerm> result;
    if (!isOpen() || languageId <= 0)
        return result;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
                      "SELECT %1 FROM words WHERE WoLgID = ? AND WoTokenCount > 1 "
                      "ORDER BY WoTokenCount DESC, WoTextLC")
                      .arg(QString::fromLatin1(kTermColumns)));
    query.addBindValue(languageId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next())
        result.append(readTermRow(query));
    return result;
}

QVector<LuteTerm> LuteDbAccess::termsByStatus(int languageId, const QVector<int>& statuses,
                                             int limit)
{
    QVector<LuteTerm> result;
    if (!isOpen() || languageId <= 0 || statuses.isEmpty())
        return result;

    QStringList placeholders;
    for (int i = 0; i < statuses.size(); ++i)
        placeholders << QStringLiteral("?");

    QString sql = QStringLiteral("SELECT %1 FROM words WHERE WoLgID = ? AND WoStatus IN (%2) "
                                "ORDER BY WoStatusChanged DESC")
                      .arg(QString::fromLatin1(kTermColumns), placeholders.join(QLatin1Char(',')));
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1").arg(limit);

    QSqlQuery query(m_impl->db);
    query.prepare(sql);
    query.addBindValue(languageId);
    for (int status : statuses)
        query.addBindValue(status);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next())
        result.append(readTermRow(query));
    return result;
}

bool LuteDbAccess::saveTerm(LuteTerm& term, QString* error)
{
    auto fail = [&](const QString& message) {
        m_impl->lastError = message;
        if (error)
            *error = message;
        return false;
    };

    if (!isOpen())
        return fail(QStringLiteral("Database is not open"));
    term.text = term.text.trimmed();
    if (term.text.isEmpty())
        return fail(QStringLiteral("Cannot save a term with empty text"));
    if (term.languageId <= 0)
        return fail(QStringLiteral("Cannot save a term without a language"));

    const LuteLanguage language = m_impl->languageOrDefault(term.languageId);
    term.textLC = LuteTextParser(language).lowercase(term.text);
    if (term.tokenCount <= 0) {
        term.tokenCount = term.text.split(QLatin1Char(' '), Qt::SkipEmptyParts).size();
        if (term.tokenCount <= 0)
            term.tokenCount = 1;
    }

    QSqlDatabase db = m_impl->db;
    if (!db.transaction())
        return fail(QStringLiteral("Cannot start transaction: %1").arg(db.lastError().text()));

    qint64 termId = term.id;
    if (termId <= 0) {
        QSqlQuery find(db);
        find.prepare(QStringLiteral("SELECT WoID FROM words WHERE WoTextLC = ? AND WoLgID = ?"));
        find.addBindValue(term.textLC);
        find.addBindValue(term.languageId);
        if (!find.exec()) {
            db.rollback();
            return fail(find.lastError().text());
        }
        if (find.next())
            termId = find.value(0).toLongLong();
    }

    const QVariant translation =
        difficultyToSql(term.translation).isEmpty() ? QVariant() : QVariant(term.translation);
    const QVariant romanization =
        term.romanization.trimmed().isEmpty() ? QVariant() : QVariant(term.romanization);

    if (termId > 0) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral(
            "UPDATE words SET WoText = ?, WoStatus = ?, WoTranslation = ?, WoRomanization = ?, "
            "WoTokenCount = ? WHERE WoID = ?"));
        update.addBindValue(term.text);
        update.addBindValue(term.status);
        update.addBindValue(translation);
        update.addBindValue(romanization);
        update.addBindValue(std::max(1, term.tokenCount));
        update.addBindValue(termId);
        if (!update.exec()) {
            db.rollback();
            return fail(update.lastError().text());
        }
    } else {
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT INTO words (WoLgID, WoText, WoTextLC, WoStatus, WoTranslation, "
            "WoRomanization, WoTokenCount, WoCreated, WoStatusChanged, WoSyncStatus) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP, 0)"));
        insert.addBindValue(term.languageId);
        insert.addBindValue(term.text);
        insert.addBindValue(term.textLC);
        insert.addBindValue(term.status);
        insert.addBindValue(translation);
        insert.addBindValue(romanization);
        insert.addBindValue(std::max(1, term.tokenCount));
        if (!insert.exec()) {
            db.rollback();
            return fail(insert.lastError().text());
        }
        termId = insert.lastInsertId().toLongLong();
    }

    QSqlQuery readBack(db);
    readBack.prepare(QStringLiteral("SELECT %1 FROM words WHERE WoID = ?")
                         .arg(QString::fromLatin1(kTermColumns)));
    readBack.addBindValue(termId);
    if (readBack.exec() && readBack.next()) {
        LuteTerm saved = readTermRow(readBack);
        saved.languageId = term.languageId;
        term = saved;
    } else {
        term.id = termId;
    }

    if (!db.commit()) {
        db.rollback();
        return fail(QStringLiteral("Cannot commit: %1").arg(db.lastError().text()));
    }
    m_impl->lastError.clear();
    return true;
}

bool LuteDbAccess::updateStatus(qint64 wordId, int status, QString* error)
{
    if (!isOpen()) {
        if (error)
            *error = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "UPDATE words SET WoStatus = ?, WoStatusChanged = CURRENT_TIMESTAMP WHERE WoID = ?"));
    query.addBindValue(status);
    query.addBindValue(wordId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return false;
    }
    return true;
}

bool LuteDbAccess::updateTranslation(qint64 wordId, const QString& translation, QString* error)
{
    if (!isOpen()) {
        if (error)
            *error = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("UPDATE words SET WoTranslation = ? WHERE WoID = ?"));
    query.addBindValue(translation.trimmed().isEmpty() ? QVariant() : QVariant(translation));
    query.addBindValue(wordId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return false;
    }
    return true;
}

bool LuteDbAccess::updateRomanization(qint64 wordId, const QString& romanization, QString* error)
{
    if (!isOpen()) {
        if (error)
            *error = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("UPDATE words SET WoRomanization = ? WHERE WoID = ?"));
    query.addBindValue(romanization.trimmed().isEmpty() ? QVariant() : QVariant(romanization));
    query.addBindValue(wordId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return false;
    }
    return true;
}

bool LuteDbAccess::deleteTerm(qint64 wordId, QString* error)
{
    if (!isOpen()) {
        if (error)
            *error = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("DELETE FROM words WHERE WoID = ?"));
    query.addBindValue(wordId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return false;
    }
    return true;
}

QVector<LuteTag> LuteDbAccess::tags()
{
    QVector<LuteTag> result;
    if(!m_impl->db.isOpen())
        return result;

    QSqlQuery query(m_impl->db);
    if(!query.exec(QStringLiteral(
            "SELECT TgID, COALESCE(TgText, ''), COALESCE(TgComment, '') "
            "FROM tags ORDER BY TgText")))
    {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while(query.next())
    {
        LuteTag tag;
        tag.id = query.value(0).toLongLong();
        tag.text = query.value(1).toString();
        tag.comment = query.value(2).toString();
        result.append(tag);
    }
    return result;
}

bool LuteDbAccess::saveTag(LuteTag& tag, QString* error)
{
    if(!m_impl->db.isOpen())
        return false;

    // Lute's tag columns are NOT NULL; null QStrings would be bound as NULL.
    if(tag.text.isNull())
        tag.text = QStringLiteral("");
    if(tag.comment.isNull())
        tag.comment = QStringLiteral("");

    QSqlQuery query(m_impl->db);
    if(tag.id > 0)
        query.prepare(QStringLiteral(
            "UPDATE tags SET TgText = ?, TgComment = ? WHERE TgID = ?"));
    else
        query.prepare(QStringLiteral(
            "INSERT INTO tags (TgText, TgComment) VALUES (?, ?)"));
    query.addBindValue(tag.text);
    query.addBindValue(tag.comment);
    if(tag.id > 0)
        query.addBindValue(tag.id);
    if(!query.exec())
    {
        if(error)
            *error = query.lastError().text();
        return false;
    }
    if(tag.id <= 0)
        tag.id = query.lastInsertId().toLongLong();
    return true;
}

QVector<LuteTerm> LuteDbAccess::parentsForTerm(qint64 termId)
{
    QVector<LuteTerm> result;
    if(!m_impl->db.isOpen())
        return result;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT WpParentWoID FROM wordparents WHERE WpWoID = ?"));
    query.addBindValue(termId);
    if(!query.exec())
        return result;
    QVector<qint64> ids;
    while(query.next())
        ids.append(query.value(0).toLongLong());
    for(qint64 id : ids)
    {
        if(const auto parent = termById(id))
            result.append(*parent);
    }
    return result;
}

bool LuteDbAccess::setTermParents(qint64 termId,
                                 const QVector<qint64>& parentIds,
                                 QString* error)
{
    if(!m_impl->db.isOpen())
        return false;
    QSqlQuery clear(m_impl->db);
    clear.prepare(QStringLiteral("DELETE FROM wordparents WHERE WpWoID = ?"));
    clear.addBindValue(termId);
    if(!clear.exec())
    {
        if(error)
            *error = clear.lastError().text();
        return false;
    }
    for(qint64 parentId : parentIds)
    {
        QSqlQuery insert(m_impl->db);
        insert.prepare(QStringLiteral(
            "INSERT INTO wordparents (WpWoID, WpParentWoID) VALUES (?, ?)"));
        insert.addBindValue(termId);
        insert.addBindValue(parentId);
        if(!insert.exec())
        {
            if(error)
                *error = insert.lastError().text();
            return false;
        }
    }
    return true;
}

QVector<LuteTag> LuteDbAccess::tagsForTerm(qint64 termId)
{
    QVector<LuteTag> result;
    if(!m_impl->db.isOpen())
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT t.TgID, COALESCE(t.TgText, ''), COALESCE(t.TgComment, '') "
        "FROM tags t JOIN wordtags wt ON wt.WtTgID = t.TgID "
        "WHERE wt.WtWoID = ? ORDER BY t.TgText"));
    query.addBindValue(termId);
    if(!query.exec())
        return result;
    while(query.next())
    {
        LuteTag tag;
        tag.id = query.value(0).toLongLong();
        tag.text = query.value(1).toString();
        tag.comment = query.value(2).toString();
        result.append(tag);
    }
    return result;
}

bool LuteDbAccess::setTermTags(qint64 termId, const QVector<qint64>& tagIds,
                              QString* error)
{
    if(!m_impl->db.isOpen())
        return false;
    QSqlQuery clear(m_impl->db);
    clear.prepare(QStringLiteral("DELETE FROM wordtags WHERE WtWoID = ?"));
    clear.addBindValue(termId);
    if(!clear.exec())
    {
        if(error)
            *error = clear.lastError().text();
        return false;
    }
    for(qint64 tagId : tagIds)
    {
        QSqlQuery insert(m_impl->db);
        insert.prepare(QStringLiteral(
            "INSERT INTO wordtags (WtWoID, WtTgID) VALUES (?, ?)"));
        insert.addBindValue(termId);
        insert.addBindValue(tagId);
        if(!insert.exec())
        {
            if(error)
                *error = insert.lastError().text();
            return false;
        }
    }
    return true;
}

bool LuteDbAccess::deleteTag(qint64 tagId, QString* error)
{
    if(!m_impl->db.isOpen())
        return false;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("DELETE FROM tags WHERE TgID = ?"));
    query.addBindValue(tagId);
    if(!query.exec())
    {
        if(error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

QHash<int, int> LuteDbAccess::statusCounts(int languageId)
{
    QHash<int, int> result;
    if (!isOpen() || languageId <= 0)
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT WoStatus, COUNT(*) FROM words WHERE WoLgID = ? GROUP BY WoStatus"));
    query.addBindValue(languageId);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next())
        result.insert(query.value(0).toInt(), query.value(1).toInt());
    return result;
}

QVector<DailyCount> LuteDbAccess::statusChangesPerDay(int languageId, int days)
{
    QVector<DailyCount> result;
    if (!isOpen() || languageId <= 0)
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT date(WoStatusChanged) AS d, COUNT(*) FROM words "
        "WHERE WoLgID = ? AND WoStatus BETWEEN 1 AND 5 "
        "AND date(WoStatusChanged) >= date('now', ?) GROUP BY d ORDER BY d"));
    query.addBindValue(languageId);
    query.addBindValue(QStringLiteral("-%1 day").arg(std::max(0, days - 1)));
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        DailyCount count;
        count.date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        count.count = query.value(1).toInt();
        result.append(count);
    }
    return result;
}

QVector<DailyCount> LuteDbAccess::createdPerDay(int languageId, int days)
{
    QVector<DailyCount> result;
    if (!isOpen() || languageId <= 0)
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT date(WoCreated) AS d, COUNT(*) FROM words "
        "WHERE WoLgID = ? AND date(WoCreated) >= date('now', ?) "
        "GROUP BY d ORDER BY d"));
    query.addBindValue(languageId);
    query.addBindValue(QStringLiteral("-%1 day").arg(std::max(0, days - 1)));
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        DailyCount count;
        count.date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        count.count = query.value(1).toInt();
        result.append(count);
    }
    return result;
}

int LuteDbAccess::totalTerms(int languageId)
{
    if (!isOpen() || languageId <= 0)
        return 0;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM words WHERE WoLgID = ?"));
    query.addBindValue(languageId);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

int LuteDbAccess::wordsReadTotal(int languageId)
{
    if (!isOpen() || languageId <= 0)
        return 0;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(WrWordCount), 0) FROM wordsread WHERE WrLgID = ?"));
    query.addBindValue(languageId);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

int LuteDbAccess::wordsReadLast7Days(int languageId)
{
    if (!isOpen() || languageId <= 0)
        return 0;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(WrWordCount), 0) FROM wordsread "
        "WHERE WrLgID = ? AND date(WrReadDate) >= date('now', '-6 day')"));
    query.addBindValue(languageId);
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

bool LuteDbAccess::recordWordsRead(int languageId, int wordCount, qint64 textId)
{
    if (!isOpen() || languageId <= 0 || wordCount <= 0)
        return false;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "INSERT INTO wordsread (WrLgID, WrTxID, WrReadDate, WrWordCount) "
        "VALUES (?, ?, CURRENT_TIMESTAMP, ?)"));
    query.addBindValue(languageId);
    query.addBindValue(textId > 0 ? QVariant(textId) : QVariant());
    query.addBindValue(wordCount);
    if (!query.exec()) {
        m_impl->lastError = query.lastError().text();
        return false;
    }
    return true;
}

QVector<LuteBook> LuteDbAccess::books(int languageId, bool includeArchived)
{
    QVector<LuteBook> result;
    if(!m_impl->db.isOpen())
        return result;

    QSqlQuery query(m_impl->db);
    const QString archivedFilter =
        includeArchived ? QString()
                        : QStringLiteral(" AND COALESCE(b.BkArchived, 0) = 0");
    query.prepare(QStringLiteral(
                      "SELECT b.BkID, b.BkTitle, b.BkLgID, "
                      "COALESCE(b.BkSourceURI, ''), COALESCE(b.BkArchived, 0), "
                      "(SELECT COUNT(*) FROM texts t WHERE t.TxBkID = b.BkID), "
                      "(SELECT t2.TxOrder FROM texts t2 WHERE t2.TxID = b.BkCurrentTxID), "
                      "COALESCE(l.LgName, '') "
                      "FROM books b LEFT JOIN languages l ON l.LgID = b.BkLgID "
                      "WHERE b.BkLgID = ?%1 ORDER BY b.BkID DESC")
                      .arg(archivedFilter));
    query.addBindValue(languageId);
    if(!query.exec())
    {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while(query.next())
    {
        LuteBook book;
        book.id = query.value(0).toLongLong();
        book.title = query.value(1).toString();
        book.languageId = query.value(2).toInt();
        book.sourceUri = query.value(3).toString();
        book.archived = query.value(4).toInt() != 0;
        book.pageCount = query.value(5).toInt();
        book.currentPage = query.value(6).toInt();
        if(book.currentPage <= 0)
            book.currentPage = 1;
        book.languageName = query.value(7).toString();
        result.append(book);
    }
    return result;
}

std::optional<LuteBook> LuteDbAccess::book(qint64 bookId)
{
    if(!m_impl->db.isOpen())
        return std::nullopt;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT b.BkID, b.BkTitle, b.BkLgID, COALESCE(b.BkSourceURI, ''), "
        "COALESCE(b.BkArchived, 0), "
        "(SELECT COUNT(*) FROM texts t WHERE t.TxBkID = b.BkID), "
        "(SELECT t2.TxOrder FROM texts t2 WHERE t2.TxID = b.BkCurrentTxID), "
        "COALESCE(l.LgName, '') "
        "FROM books b LEFT JOIN languages l ON l.LgID = b.BkLgID WHERE b.BkID = ?"));
    query.addBindValue(bookId);
    if(!query.exec() || !query.next())
        return std::nullopt;

    LuteBook result;
    result.id = query.value(0).toLongLong();
    result.title = query.value(1).toString();
    result.languageId = query.value(2).toInt();
    result.sourceUri = query.value(3).toString();
    result.archived = query.value(4).toInt() != 0;
    result.pageCount = query.value(5).toInt();
    result.currentPage = query.value(6).toInt();
    if(result.currentPage <= 0)
        result.currentPage = 1;
    result.languageName = query.value(7).toString();
    return result;
}

QVector<LuteText> LuteDbAccess::texts(qint64 bookId)
{
    QVector<LuteText> result;
    if(!m_impl->db.isOpen())
        return result;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT TxID, TxBkID, TxOrder, COALESCE(TxWordCount, 0), "
        "CASE WHEN TxReadDate IS NULL THEN 0 ELSE 1 END "
        "FROM texts WHERE TxBkID = ? ORDER BY TxOrder"));
    query.addBindValue(bookId);
    if(!query.exec())
    {
        m_impl->lastError = query.lastError().text();
        return result;
    }
    while(query.next())
    {
        LuteText text;
        text.id = query.value(0).toLongLong();
        text.bookId = query.value(1).toLongLong();
        text.order = query.value(2).toInt();
        text.wordCount = query.value(3).toInt();
        text.read = query.value(4).toInt() != 0;
        result.append(text);
    }
    return result;
}

QVector<LuteDictionary> LuteDbAccess::dictionaries(int languageId)
{
    QVector<LuteDictionary> result;
    if(!m_impl->db.isOpen())
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT LdID, LdLgID, LdUseFor, LdType, LdDictURI, LdIsActive, "
        "LdSortOrder FROM languagedicts WHERE LdLgID = ? "
        "ORDER BY LdSortOrder, LdID"));
    query.addBindValue(languageId);
    if(!query.exec())
        return result;
    while(query.next())
    {
        LuteDictionary dictionary;
        dictionary.id = query.value(0).toLongLong();
        dictionary.languageId = query.value(1).toInt();
        dictionary.useFor = query.value(2).toString();
        dictionary.type = query.value(3).toString();
        dictionary.uri = query.value(4).toString();
        dictionary.active = query.value(5).toInt() != 0;
        dictionary.sortOrder = query.value(6).toInt();
        result.append(dictionary);
    }
    return result;
}

QVector<LuteBookmark> LuteDbAccess::bookmarks(qint64 bookId)
{
    QVector<LuteBookmark> result;
    if(!m_impl->db.isOpen())
        return result;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT b.TbID, b.TbTxID, t.TxOrder, COALESCE(b.TbTitle, '') "
        "FROM textbookmarks b JOIN texts t ON t.TxID = b.TbTxID "
        "WHERE t.TxBkID = ? ORDER BY t.TxOrder"));
    query.addBindValue(bookId);
    if(!query.exec())
        return result;
    while(query.next())
    {
        LuteBookmark bookmark;
        bookmark.id = query.value(0).toLongLong();
        bookmark.textId = query.value(1).toLongLong();
        bookmark.page = query.value(2).toInt();
        bookmark.title = query.value(3).toString();
        result.append(bookmark);
    }
    return result;
}

std::optional<LuteBookmark> LuteDbAccess::bookmarkForText(qint64 textId)
{
    if(!m_impl->db.isOpen())
        return std::nullopt;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT b.TbID, b.TbTxID, t.TxOrder, COALESCE(b.TbTitle, '') "
        "FROM textbookmarks b JOIN texts t ON t.TxID = b.TbTxID "
        "WHERE b.TbTxID = ?"));
    query.addBindValue(textId);
    if(!query.exec() || !query.next())
        return std::nullopt;
    LuteBookmark bookmark;
    bookmark.id = query.value(0).toLongLong();
    bookmark.textId = query.value(1).toLongLong();
    bookmark.page = query.value(2).toInt();
    bookmark.title = query.value(3).toString();
    return bookmark;
}

qint64 LuteDbAccess::addBookmark(qint64 textId, const QString& title,
                                QString* error)
{
    if(!m_impl->db.isOpen())
        return 0;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "INSERT INTO textbookmarks (TbTxID, TbTitle) VALUES (?, ?)"));
    query.addBindValue(textId);
    query.addBindValue(title.isNull() ? QStringLiteral("") : title);
    if(!query.exec())
    {
        if(error)
            *error = query.lastError().text();
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

bool LuteDbAccess::deleteBookmark(qint64 bookmarkId, QString* error)
{
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("DELETE FROM textbookmarks WHERE TbID = ?"));
    query.addBindValue(bookmarkId);
    if(!query.exec())
    {
        if(error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

std::optional<LuteText> LuteDbAccess::text(qint64 bookId, int order)
{
    if(!m_impl->db.isOpen())
        return std::nullopt;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "SELECT TxID, TxBkID, TxOrder, COALESCE(TxWordCount, 0), "
        "CASE WHEN TxReadDate IS NULL THEN 0 ELSE 1 END, COALESCE(TxText, '') "
        "FROM texts WHERE TxBkID = ? AND TxOrder = ?"));
    query.addBindValue(bookId);
    query.addBindValue(order);
    if(!query.exec() || !query.next())
        return std::nullopt;

    LuteText result;
    result.id = query.value(0).toLongLong();
    result.bookId = query.value(1).toLongLong();
    result.order = query.value(2).toInt();
    result.wordCount = query.value(3).toInt();
    result.read = query.value(4).toInt() != 0;
    result.text = query.value(5).toString();
    return result;
}

bool LuteDbAccess::setBookCurrentPage(qint64 bookId, int page)
{
    if(!m_impl->db.isOpen())
        return false;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "UPDATE books SET BkCurrentTxID = "
        "(SELECT TxID FROM texts WHERE TxBkID = ? AND TxOrder = ?) "
        "WHERE BkID = ?"));
    query.addBindValue(bookId);
    query.addBindValue(page);
    query.addBindValue(bookId);
    if(!query.exec())
    {
        m_impl->lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool LuteDbAccess::markTextRead(qint64 textId)
{
    if(!m_impl->db.isOpen())
        return false;

    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral(
        "UPDATE texts SET TxReadDate = datetime('now') WHERE TxID = ?"));
    query.addBindValue(textId);
    if(!query.exec())
    {
        m_impl->lastError = query.lastError().text();
        return false;
    }
    return true;
}

std::optional<qint64> LuteDbAccess::findBookBySource(const QString& sourceUri, int languageId)
{
    if (!isOpen() || languageId <= 0 || sourceUri.isEmpty())
        return std::nullopt;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("SELECT BkID FROM books WHERE BkSourceURI = ? AND BkLgID = ?"));
    query.addBindValue(sourceUri);
    query.addBindValue(languageId);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return query.value(0).toLongLong();
}

qint64 LuteDbAccess::findOrCreateBook(const QString& title, const QString& sourceUri,
                                      int languageId, QString* error)
{
    if (!isOpen() || languageId <= 0) {
        if (error)
            *error = QStringLiteral("Database is not open or no language selected");
        return 0;
    }
    if (const auto existing = findBookBySource(sourceUri, languageId))
        return *existing;

    QSqlQuery insert(m_impl->db);
    insert.prepare(QStringLiteral(
        "INSERT INTO books (BkLgID, BkTitle, BkSourceURI, BkArchived, BkCurrentTxID) "
        "VALUES (?, ?, ?, 0, 0)"));
    insert.addBindValue(languageId);
    insert.addBindValue(title);
    insert.addBindValue(sourceUri.isEmpty() ? QVariant() : QVariant(sourceUri));
    if (!insert.exec()) {
        m_impl->lastError = insert.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return 0;
    }
    return insert.lastInsertId().toLongLong();
}

std::optional<qint64> LuteDbAccess::findTextForBook(qint64 bookId)
{
    if (!isOpen() || bookId <= 0)
        return std::nullopt;
    QSqlQuery query(m_impl->db);
    query.prepare(QStringLiteral("SELECT TxID FROM texts WHERE TxBkID = ? ORDER BY TxOrder LIMIT 1"));
    query.addBindValue(bookId);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return query.value(0).toLongLong();
}

qint64 LuteDbAccess::createText(qint64 bookId, int order, const QString& text, int wordCount,
                                QString* error)
{
    if (!isOpen() || bookId <= 0) {
        if (error)
            *error = QStringLiteral("Database is not open or book is invalid");
        return 0;
    }
    QSqlQuery insert(m_impl->db);
    insert.prepare(QStringLiteral(
        "INSERT INTO texts (TxBkID, TxOrder, TxText, TxWordCount, TxStartDate) "
        "VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)"));
    insert.addBindValue(bookId);
    insert.addBindValue(order);
    insert.addBindValue(text);
    insert.addBindValue(wordCount);
    if (!insert.exec()) {
        m_impl->lastError = insert.lastError().text();
        if (error)
            *error = m_impl->lastError;
        return 0;
    }
    return insert.lastInsertId().toLongLong();
}

} // namespace lute
