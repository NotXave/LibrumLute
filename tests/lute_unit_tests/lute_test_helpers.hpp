#pragma once

#include "lute_language.hpp"
#include "word_quad.hpp"

#include <QRectF>
#include <QString>

namespace lute_test
{

/// A compact English language row with the fields Lute uses for tokenising.
inline lute::LuteLanguage englishLanguage()
{
    lute::LuteLanguage language;
    language.id = 21;
    language.name = QStringLiteral("English");
    language.regexpWordCharacters = QStringLiteral("a-zA-Z0-9'");
    language.regexpSplitSentences = QStringLiteral(".!?");
    language.characterSubstitutions = QStringLiteral("\u2019='");
    language.parserType = QStringLiteral("spacedel");
    return language;
}

/// A page word with a synthetic box, handy for phrase matcher tests.
inline lute::WordQuad makeWord(const QString& text, int index, qreal x = 0.0)
{
    lute::WordQuad word;
    word.word = text;
    word.sentence = QStringLiteral("Ellen had run out of patience.");
    word.rect = QRectF(x, 0.1, 0.05, 0.02);
    word.page = 0;
    word.index = index;
    return word;
}

} // namespace lute_test
