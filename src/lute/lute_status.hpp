#pragma once

#include <QColor>
#include <QString>

namespace lute {

/// The eight term states used by Lute v3.
///
/// The numeric values are part of the `lute.db` contract (`words.WoStatus`) and
/// must never be renumbered: 1..5 are the learning ladder, 98 means "ignored",
/// 99 means "well known". 0 is used by Lute for terms that were created without
/// a status (and is what an unknown word implicitly maps to).
enum TermStatus : int {
    StatusUnknown = 0,
    StatusNew1 = 1,
    StatusNew2 = 2,
    StatusLearning3 = 3,
    StatusLearning4 = 4,
    StatusLearned5 = 5,
    StatusIgnored = 98,
    StatusWellKnown = 99,
};

/// Single letter used by Lute in its term badges ("1".."5", "W", "I", "?").
QString statusAbbreviation(int status);

/// Human readable status name, matching Lute's `statuses.StText` column.
QString statusLabel(int status);

/// True for the learning ladder 1..5.
bool isLearningStatus(int status);

/// True for 98 (ignored) and 99 (well known).
bool isKnownStatus(int status);

/// Background colour used by Lute for a term of this status.
///
/// These are Lute's own CSS custom properties (`--status-*-color`), so a page
/// highlighted by this application looks like a Lute text.
QColor statusColor(int status);

/// Statuses ordered the way Lute shows them in its term lists.
QList<int> allStatuses();

} // namespace lute
