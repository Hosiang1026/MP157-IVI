#pragma once

#include <QByteArray>
#include <QVariant>

namespace AirPlayBplist {
QByteArray encode(const QVariantMap &root);
QVariant decode(const QByteArray &bytes);
}
