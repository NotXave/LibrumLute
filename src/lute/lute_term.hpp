#pragma once

#include <QDateTime>
#include <QString>

namespace lute {

/// A row of `lute.db`'s `words` table: one vocabulary term.
///
/// `text` keeps the original casing as it appeared when the term was created,
/// `textLC` is the case-folded key Lute uses for lookups (unique together with
/// the language id), and `tokenCount` is 1 for single words and N for phrases
/// such as "run out of".
struct LuteTerm {
    qint64 id = 0;
    int languageId = 0;
    QString text;
    QString textLC;
    int status = 0;
    QString translation;
    QString romanization;
    int tokenCount = 0; ///< 0 = infer from text, 1 = single word, N = phrase
    QDateTime created;
    QDateTime statusChanged;
    int syncStatus = 0;

    bool isValid() const { return id > 0; }
    bool isPhrase() const { return tokenCount > 1; }
};

} // namespace lute
