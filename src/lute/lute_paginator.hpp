#pragma once

#include <QString>
#include <QVector>

namespace lute
{

class LuteTextParser;

/// Splits an imported text into Lute-style pages.
///
/// Lute groups about 250 tokens per page, can split at paragraph or sentence
/// boundaries and always breaks at lines containing only `---`. The same rules
/// are applied here so a text imported in Librum looks like one imported in
/// Lute itself.
class LutePaginator
{
public:
    static QVector<QString> paginate(const QString& text,
                                     const LuteTextParser& parser,
                                     int threshold = 250,
                                     bool splitBySentences = false);
};

} // namespace lute
