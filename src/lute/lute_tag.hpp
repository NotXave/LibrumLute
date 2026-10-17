#pragma once
#include <QString>
namespace lute {
struct LuteTag { qint64 id = 0; QString text; QString comment; bool isValid() const { return id > 0; } };
}
