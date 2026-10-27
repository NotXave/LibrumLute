#pragma once

#include <QString>

namespace lute
{

/// Helpers for producing and recognising Lute v3 databases.
///
/// The application never re-invents the schema: a brand new library is an exact
/// copy of an empty `lute.db` generated from Lute's own baseline schema, with
/// all of Lute's shipped languages seeded. That means every database created
/// here can be opened by Lute v3 itself (and vice versa).
class LuteSchema
{
public:
    /// Official Lute baseline schema (embedded resource).
    static QString baselineSql();
    /// Generated language/dictionary seed (embedded resource).
    static QString languageSeedSql();

    /// Copies the embedded empty lute.db to `targetPath`.
    /// Fails if a file already exists, unless `overwrite` is true.
    static bool createDatabase(const QString& targetPath,
                               QString* error = nullptr,
                               bool overwrite = false);

    /// True when `path` looks like a Lute v3 database (has words/languages and
    /// the migration table).
    static bool isLuteDatabase(const QString& path);

    /// Directory holding the user's libraries (created on demand).
    static QString defaultLibraryDirectory();
};

}  // namespace lute
