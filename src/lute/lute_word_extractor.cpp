#include "lute_word_extractor.hpp"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>

#include <mupdf/fitz.h>

#include "lute_text_parser.hpp"

namespace lute
{
namespace
{

QRectF quadToRect(const fz_quad& quad)
{
    const float minX = std::min({quad.ul.x, quad.ur.x, quad.ll.x, quad.lr.x});
    const float maxX = std::max({quad.ul.x, quad.ur.x, quad.ll.x, quad.lr.x});
    const float minY = std::min({quad.ul.y, quad.ur.y, quad.ll.y, quad.lr.y});
    const float maxY = std::max({quad.ul.y, quad.ur.y, quad.ll.y, quad.lr.y});
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

} // namespace

struct LuteWordExtractor::Impl
{
    fz_context* context = nullptr;
    fz_document* document = nullptr;
    QByteArray buffer;
    QString path;
    QString title;

    LuteLanguage language;
    std::unique_ptr<LuteTextParser> parser;

    QHash<int, QVector<WordQuad>> wordCache;
    QHash<int, QString> textCache;
    QHash<int, QSizeF> sizeCache;

    ~Impl() { teardown(); }

    void teardown()
    {
        if(document)
        {
            fz_drop_document(context, document);
            document = nullptr;
        }
        if(context)
        {
            fz_drop_context(context);
            context = nullptr;
        }
        wordCache.clear();
        textCache.clear();
        sizeCache.clear();
        buffer.clear();
        title.clear();
        path.clear();
    }

    LuteTextParser* ensureParser()
    {
        if(!parser)
            parser = std::make_unique<LuteTextParser>(language);
        return parser.get();
    }
};

LuteWordExtractor::LuteWordExtractor() :
    m_impl(std::make_unique<Impl>())
{
    m_impl->language.name = QStringLiteral("Generic");
    m_impl->language.regexpSplitSentences = QStringLiteral(".!?");
    m_impl->language.characterSubstitutions =
        QStringLiteral("´='|`='|’='|‘='|...=…|..=‥");
    m_impl->language.parserType = QStringLiteral("spacedel");
}

LuteWordExtractor::~LuteWordExtractor() = default;

bool LuteWordExtractor::open(const QString& filePath, QString* error)
{
    close();

    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        if(error)
            *error = QStringLiteral("Cannot open '%1': %2").arg(filePath,
                                                                file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    if(data.isEmpty())
    {
        if(error)
            *error = QStringLiteral("'%1' is empty").arg(filePath);
        return false;
    }

    fz_context* context = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
    if(!context)
    {
        if(error)
            *error = QStringLiteral("Could not create the MuPDF context");
        return false;
    }

    fz_register_document_handlers(context);

    fz_stream* stream = nullptr;
    fz_document* document = nullptr;
    fz_try(context)
    {
        stream = fz_open_memory(context,
                                reinterpret_cast<const unsigned char*>(
                                    data.constData()),
                                size_t(data.size()));
        // MuPDF supports more than PDFs: the user's EPUBs/MOBIs must be
        // opened with the matching magic or text extraction and the Lute
        // overlay stay empty for them.
        QString mimeType = QStringLiteral("application/pdf");
        const QString suffix = QFileInfo(filePath).suffix().toLower();
        if(suffix == QStringLiteral("epub"))
            mimeType = QStringLiteral("application/epub+zip");
        else if(suffix == QStringLiteral("mobi"))
            mimeType = QStringLiteral("application/x-mobipocket-ebook");
        else if(suffix == QStringLiteral("fb2"))
            mimeType = QStringLiteral("application/x-fictionbook+xml");
        else if(suffix == QStringLiteral("cbz"))
            mimeType = QStringLiteral("application/vnd.comicbook+zip");
        else if(suffix == QStringLiteral("html") || suffix == QStringLiteral("xhtml"))
            mimeType = QStringLiteral("text/html");
        document = fz_open_document_with_stream(
            context, mimeType.toUtf8().constData(), stream);
    }
    fz_always(context)
    {
        if(stream)
            fz_drop_stream(context, stream);
    }
    fz_catch(context)
    {
        const QString message =
            QStringLiteral("MuPDF could not open '%1': %2")
                .arg(filePath, QString::fromUtf8(fz_caught_message(context)));
        fz_drop_context(context);
        if(error)
            *error = message;
        return false;
    }

    m_impl->context = context;
    m_impl->document = document;
    m_impl->buffer = data;
    m_impl->path = filePath;

    // The PDF title, if it has one.
    char titleBuffer[512] = {0};
    fz_try(context)
    {
        fz_lookup_metadata(context, document, FZ_META_INFO_TITLE, titleBuffer,
                           sizeof(titleBuffer) - 1);
    }
    fz_catch(context)
    {
        titleBuffer[0] = '\0';
    }
    m_impl->title = QString::fromUtf8(titleBuffer).trimmed();

    if(pageCount() <= 0)
    {
        if(error)
            *error = QStringLiteral("'%1' contains no pages").arg(filePath);
        close();
        return false;
    }
    return true;
}

void LuteWordExtractor::close()
{
    m_impl->teardown();
}

bool LuteWordExtractor::isOpen() const
{
    return m_impl->document != nullptr;
}

QString LuteWordExtractor::filePath() const
{
    return m_impl->path;
}

QString LuteWordExtractor::title() const
{
    return m_impl->title;
}

int LuteWordExtractor::pageCount() const
{
    if(!m_impl->document)
        return 0;
    int count = 0;
    fz_try(m_impl->context)
    {
        count = fz_count_pages(m_impl->context, m_impl->document);
    }
    fz_catch(m_impl->context)
    {
        count = 0;
    }
    return count;
}

QSizeF LuteWordExtractor::pageSize(int pageIndex)
{
    if(!m_impl->document)
        return {};
    const auto cached = m_impl->sizeCache.constFind(pageIndex);
    if(cached != m_impl->sizeCache.constEnd())
        return *cached;

    QSizeF size;
    fz_page* page = nullptr;
    fz_try(m_impl->context)
    {
        page = fz_load_page(m_impl->context, m_impl->document, pageIndex);
        const fz_rect bounds = fz_bound_page(m_impl->context, page);
        size = QSizeF(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
    }
    fz_catch(m_impl->context)
    {
        size = QSizeF();
    }
    if(page)
        fz_drop_page(m_impl->context, page);

    if(size.isEmpty())
        return {};
    m_impl->sizeCache.insert(pageIndex, size);
    return size;
}

void LuteWordExtractor::setLanguage(const LuteLanguage& language)
{
    if(m_impl->language.id == language.id
       && m_impl->language.regexpWordCharacters == language.regexpWordCharacters
       && m_impl->language.characterSubstitutions
              == language.characterSubstitutions)
        return;

    m_impl->language = language;
    m_impl->parser = std::make_unique<LuteTextParser>(language);
    clearCache();
}

const LuteLanguage& LuteWordExtractor::language() const
{
    return m_impl->language;
}

void LuteWordExtractor::clearCache()
{
    m_impl->wordCache.clear();
    m_impl->textCache.clear();
}

QString LuteWordExtractor::pageText(int pageIndex)
{
    words(pageIndex);
    return m_impl->textCache.value(pageIndex);
}

const QVector<WordQuad>& LuteWordExtractor::words(int pageIndex)
{
    static const QVector<WordQuad> empty;

    const auto cached = m_impl->wordCache.constFind(pageIndex);
    if(cached != m_impl->wordCache.constEnd())
        return *cached;

    if(!m_impl->document || pageIndex < 0 || pageIndex >= pageCount())
        return empty;

    // Rebuild the page as running text while remembering the glyph rectangle
    // of every character, so a token range can be turned back into a box.
    QString text;
    QVector<QRectF> charRects;
    QSizeF pageSize;

    fz_page* page = nullptr;
    fz_stext_page* stext = nullptr;
    fz_try(m_impl->context)
    {
        page = fz_load_page(m_impl->context, m_impl->document, pageIndex);
        const fz_rect bounds = fz_bound_page(m_impl->context, page);
        pageSize = QSizeF(bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);

        fz_stext_options options = {0, 0};
        stext = fz_new_stext_page_from_page(m_impl->context, page, &options);
        for(fz_stext_block* block = stext->first_block; block;
            block = block->next)
        {
            if(block->type != FZ_STEXT_BLOCK_TEXT)
                continue;

            if(!text.isEmpty())
            {
                text.append(QLatin1Char('\n'));
                charRects.append(QRectF());
                text.append(QLatin1Char('\n'));
                charRects.append(QRectF());
            }

            for(fz_stext_line* line = block->u.t.first_line; line;
                line = line->next)
            {
                for(fz_stext_char* chr = line->first_char; chr; chr = chr->next)
                {
                    const unsigned int code = unsigned(chr->c);
                    const char32_t point = char32_t(code);
                    const QString characters = QString::fromUcs4(&point, 1);
                    const QRectF rect = quadToRect(chr->quad);
                    for(const QChar character : characters)
                    {
                        text.append(character);
                        charRects.append(rect);
                    }
                }
                text.append(QLatin1Char('\n'));
                charRects.append(QRectF());
            }
        }
    }
    fz_always(m_impl->context)
    {
        if(stext)
            fz_drop_stext_page(m_impl->context, stext);
        if(page)
            fz_drop_page(m_impl->context, page);
    }
    fz_catch(m_impl->context)
    {
        m_impl->wordCache.insert(pageIndex, QVector<WordQuad>());
        m_impl->textCache.insert(pageIndex, QString());
        return m_impl->wordCache.value(pageIndex);
    }

    if(pageSize.isEmpty())
        pageSize = QSizeF(1, 1);

    // Normalise all character rectangles to the page.
    for(QRectF& rect : charRects)
    {
        if(rect.isNull())
            continue;
        rect = QRectF(rect.left() / pageSize.width(), rect.top() / pageSize.height(),
                      rect.width() / pageSize.width(),
                      rect.height() / pageSize.height());
    }

    LuteTextParser* parser = m_impl->ensureParser();
    const QVector<ParsedWord> tokens = parser->words(text);

    QVector<WordQuad> words;
    words.reserve(tokens.size());
    int index = 0;
    for(const ParsedWord& token : tokens)
    {
        QRectF box;
        for(int i = token.start; i < token.end && i < charRects.size(); ++i)
        {
            const QRectF& rect = charRects.at(i);
            if(rect.isNull() || rect.width() <= 0 || rect.height() <= 0)
                continue;
            box = box.isNull() ? rect : box.united(rect);
        }
        if(box.isNull() || box.width() <= 0 || box.height() <= 0)
            continue;

        WordQuad word;
        word.word = token.text;
        word.sentence = parser->sentenceAt(text, token.start);
        word.page = pageIndex;
        word.index = index++;
        word.rect = box.intersected(QRectF(0, 0, 1, 1));
        if(!word.rect.isValid())
            continue;
        words.append(word);
    }

    m_impl->textCache.insert(pageIndex, text);
    return *m_impl->wordCache.insert(pageIndex, words);
}

} // namespace lute
