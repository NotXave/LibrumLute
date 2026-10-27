#include "lute_status.hpp"
#include <QList>

namespace lute
{

QString statusAbbreviation(int status)
{
    switch(status)
    {
    case StatusUnknown:
        return QStringLiteral("?");
    case StatusNew1:
        return QStringLiteral("1");
    case StatusNew2:
        return QStringLiteral("2");
    case StatusLearning3:
        return QStringLiteral("3");
    case StatusLearning4:
        return QStringLiteral("4");
    case StatusLearned5:
        return QStringLiteral("5");
    case StatusIgnored:
        return QStringLiteral("I");
    case StatusWellKnown:
        return QStringLiteral("W");
    default:
        return QString::number(status);
    }
}

QString statusLabel(int status)
{
    switch(status)
    {
    case StatusUnknown:
        return QStringLiteral("Unknown");
    case StatusNew1:
        return QStringLiteral("New (1)");
    case StatusNew2:
        return QStringLiteral("New (2)");
    case StatusLearning3:
        return QStringLiteral("Learning (3)");
    case StatusLearning4:
        return QStringLiteral("Learning (4)");
    case StatusLearned5:
        return QStringLiteral("Learned");
    case StatusIgnored:
        return QStringLiteral("Ignored");
    case StatusWellKnown:
        return QStringLiteral("Well Known");
    default:
        return QStringLiteral("Status %1").arg(status);
    }
}

bool isLearningStatus(int status)
{
    return status >= StatusNew1 && status <= StatusLearned5;
}

bool isKnownStatus(int status)
{
    return status == StatusIgnored || status == StatusWellKnown;
}

QColor statusColor(int status)
{
    switch(status)
    {
    case StatusUnknown:
        return QColor(QStringLiteral("#D5FFFF"));
    case StatusNew1:
        return QColor(QStringLiteral("#F5B8A9"));
    case StatusNew2:
        return QColor(QStringLiteral("#F5CCA9"));
    case StatusLearning3:
        return QColor(QStringLiteral("#F5E1A9"));
    case StatusLearning4:
        return QColor(QStringLiteral("#F5F3A9"));
    case StatusLearned5:
        return QColor(QStringLiteral("#DDFFDD"));
    case StatusIgnored:
        return QColor(QStringLiteral("#ee8577"));
    case StatusWellKnown:
        return QColor(QStringLiteral("#72da88"));
    default:
        return QColor(QStringLiteral("#D5FFFF"));
    }
}

QColor highlightColor(int status)
{
    switch(status)
    {
    case StatusUnknown:
        return QColor(QStringLiteral("#D9534F"));
    case StatusNew1:
        return QColor(QStringLiteral("#E06C75"));
    case StatusNew2:
        return QColor(QStringLiteral("#E5A15C"));
    case StatusLearning3:
        return QColor(QStringLiteral("#E3C95A"));
    case StatusLearning4:
        return QColor(QStringLiteral("#B48EAD"));
    case StatusLearned5:
        return QColor(QStringLiteral("#6FA8DC"));
    default:
        return QColor();
    }
}

QList<int> allStatuses()
{
    return { StatusNew1,     StatusNew2,      StatusLearning3, StatusLearning4,
             StatusLearned5, StatusWellKnown, StatusIgnored };
}

}  // namespace lute
