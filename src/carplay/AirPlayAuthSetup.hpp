#pragma once

#include <QByteArray>

class LocalMfiAuth;

namespace AirPlayAuthSetup {
QByteArray handle(const QByteArray &body, LocalMfiAuth &mfi);
}
