#pragma once

#include <QString>
#include <QVariantList>

namespace BluetoothDevices {

QVariantList listDevices();
QString localAdapterAddress();
bool isPaired(const QString &address);
bool authenticate(const QString &address, QString *error = nullptr);

} // namespace BluetoothDevices
