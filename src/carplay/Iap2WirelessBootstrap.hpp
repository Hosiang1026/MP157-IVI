#pragma once

#include "Iap2Protocol.hpp"
#include "LocalMfiAuth.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

class Iap2LinkSession;

class Iap2WirelessBootstrap {
public:
    enum class Status {
        Ok,
        TimedOut,
        Failed,
        Rejected,
        AuthFailed,
        Dead,
    };

    struct Result {
        Status status = Status::Failed;
        QString error;
        int wifiConfigsSent = 0;
        int startSessionsSent = 0;
        bool transportNotified = false;
    };

    struct Endpoint {
        QString ssid;
        QString passphrase;
        quint8 channel = 0;
        quint8 securityType = 2;
        QByteArray bssid;
        QStringList ipAddresses;
        quint32 airPlayPort = 7000;
        QString deviceIdentifier;
        QString publicKey;
        QString sourceVersion;
    };

    using ProgressFn = std::function<void(QString)>;
    using KeepAliveFn = std::function<bool()>;

    Result run(Iap2LinkSession &session,
               LocalMfiAuth &mfi,
               const Iap2Protocol::WirelessIdentity &identity,
               const Endpoint &endpoint,
               int timeoutMs = 60000,
               const ProgressFn &onProgress = ProgressFn(),
               const KeepAliveFn &keepAlive = KeepAliveFn());

private:
    static quint16 messageIdOf(const QByteArray &csm);
    static qint64 nowMs();
    static int remainingMs(qint64 deadlineMs);
};
