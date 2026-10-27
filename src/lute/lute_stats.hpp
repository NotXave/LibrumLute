#pragma once

#include <QDate>
#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

namespace lute
{

/// One point of the "terms learned per day" chart.
struct DailyCount
{
    QDate date;
    int count = 0;
};

/// Aggregated learning metrics read from `lute.db` by LuteStatsEngine.
struct LuteStats
{
    int languageId = 0;
    QString languageName;

    /// Terms per Lute status (keys are `words.WoStatus` values).
    QHash<int, int> byStatus;
    /// Distinct terms in this language (one row per term, phrases count once).
    int totalTerms = 0;
    /// Terms in the learning ladder (1..5).
    int learningTerms = 0;
    /// 98 + 99.
    int knownTerms = 0;
    /// Distinct unknown terms stored with status 0.
    int unknownTerms = 0;

    /// Terms whose status changed to 1..5 during the last 30 days, per day.
    QVector<DailyCount> learnedLast30Days;
    /// Terms added (WoCreated) during the last 30 days, per day.
    QVector<DailyCount> createdLast30Days;

    /// From the `wordsread` table (Lute's reading history).
    int wordsReadTotal = 0;
    int wordsReadLast7Days = 0;

    /// Coverage of the currently open document, computed by ReadingSession.
    int documentTokenCount = 0;
    int documentKnownTokenCount = 0;

    double documentCoverage() const
    {
        return documentTokenCount > 0
                   ? 100.0 * double(documentKnownTokenCount) /
                         double(documentTokenCount)
                   : 0.0;
    }
};

}  // namespace lute
