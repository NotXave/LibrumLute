#include "word_quad.hpp"

namespace lute
{

QString WordSpan::text() const
{
    QStringList parts;
    parts.reserve(words.size());
    for(const auto& w : words)
        parts << w.word;
    return parts.join(QLatin1Char(' '));
}

}  // namespace lute
