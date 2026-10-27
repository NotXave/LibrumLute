#pragma once

#include <QHash>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <memory>
#include "lute_language.hpp"
#include "word_quad.hpp"

namespace mupdf
{
struct FzDocument;
}

namespace lute
{

class LuteTextParser;

/***
 * Extracts every word of a PDF page together with its normalised bounding box
 * and the full sentence it appears in.
 *
 * The heavy lifting is done by MuPDF's structured text device (the same engine
 * Librum renders the pages with), so word boxes and the rendered page always
 * line up. Extracted pages are cached per page number.
 */
class LuteWordExtractor
{
public:
    LuteWordExtractor();
    ~LuteWordExtractor();

    LuteWordExtractor(const LuteWordExtractor&) = delete;
    LuteWordExtractor& operator=(const LuteWordExtractor&) = delete;

    bool open(const QString& filePath, QString* error = nullptr);
    /// Share an already opened MuPDF document (same context, same layout) so
    /// page numbers and word coordinates match what Librum renders.
    bool openShared(mupdf::FzDocument* document, const QString& filePath);
    void close();
    bool isOpen() const;
    QString filePath() const;
    QString title() const;

    int pageCount() const;
    QSizeF pageSize(int pageIndex);

    void setLanguage(const LuteLanguage& language);
    const LuteLanguage& language() const;

    /// All words of `pageIndex`, in reading order (cached).
    const QVector<WordQuad>& words(int pageIndex);
    QString pageText(int pageIndex);

    void clearCache();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace lute
