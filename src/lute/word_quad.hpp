#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace lute {

/// One clickable word on a rendered PDF page.
///
/// `rect` is normalised to the page size (0..1 on both axes, origin at the
/// top-left corner), so the presentation layer can draw it at any zoom level
/// without knowing the page dimensions. `sentence` carries the surrounding
/// sentence, which is what gets sent to the AI translation service.
struct WordQuad {
    QString word;
    QString sentence;
    QRectF rect;
    int page = -1;
    int index = -1;

    bool isValid() const { return !word.isEmpty() && rect.isValid(); }
};

/// A contiguous run of words forming one display/highlight unit. For a normal
/// term this holds exactly one word; for a multi-word term (phrase) it holds
/// the run of tokens that is stored in `words` with `WoTokenCount = N`.
struct WordSpan {
    QVector<WordQuad> words;
    QRectF boundingRect;
    /// Sentence of the first token (the popup's context sentence).
    QString sentence;

    bool isEmpty() const { return words.isEmpty(); }
    int count() const { return words.size(); }
    QString text() const;
    int firstIndex() const { return words.isEmpty() ? -1 : words.first().index; }
    int lastIndex() const { return words.isEmpty() ? -1 : words.last().index; }
};

} // namespace lute
