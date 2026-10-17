#pragma once

#include <QString>

namespace lute
{

/// One row of Lute's `books` table (a book groups the text pages).
struct LuteBook
{
    qint64 id = 0;
    int languageId = 0;
    QString title;
    QString sourceUri;
    bool archived = false;
    int pageCount = 0;
    int currentPage = 1;
    QString languageName;

    bool isValid() const { return id > 0; }
};

/// One row of Lute's `texts` table: a single readable page of a book.
struct LuteDictionary {
    qint64 id = 0;
    int languageId = 0;
    QString useFor;
    QString type;
    QString uri;
    bool active = true;
    int sortOrder = 0;
};

struct LuteBookmark {
    qint64 id = 0;
    qint64 textId = 0;
    int page = 0;
    QString title;
};

struct LuteText
{
    qint64 id = 0;
    qint64 bookId = 0;
    int order = 1;
    int wordCount = 0;
    bool read = false;
    QString text;

    bool isValid() const { return id > 0; }
};

} // namespace lute
