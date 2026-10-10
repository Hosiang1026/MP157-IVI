#pragma once

#include <QString>
#include <QVariantList>

namespace BluetoothDevices {

QVariantList listDevices();
QString localAdapterAddress();
bool isPaired(const QString &address);
bool authenticate(const QString &address, QString *error = nullptr);
bool connectDevice(const QString &address, QString *error = nullptr);
bool disconnectDevice(const QString &address, QString *error = nullptr);
bool setAudioEnabled(const QString &address, bool enabled, QString *error = nullptr);
bool connectAudioProfile(const QString &address, QString *error = nullptr);
bool disconnectAudioProfile(const QString &address, QString *error = nullptr);

} // namespace BluetoothDevices
