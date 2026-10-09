#include "Iap2WirelessBootstrap.hpp"

#include "Iap2Body.hpp"
#include "Iap2LinkSession.hpp"
#include "Iap2Wire.hpp"

#include <QVector>
#include <chrono>

namespace {

void progress(const Iap2WirelessBootstrap::ProgressFn &fn, const QString &msg)
{
    if (fn)
        fn(msg);
}

} // namespace

quint16 Iap2WirelessBootstrap::messageIdOf(const QByteArray &csm)
{
    if (csm.size() < Iap2Wire::kCsmHeaderBytes)
        return 0;
    return quint16((quint8(csm[4]) << 8) | quint8(csm[5]));
}

qint64 Iap2WirelessBootstrap::nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int Iap2WirelessBootstrap::remainingMs(qint64 deadlineMs)
{
    const qint64 left = deadlineMs - nowMs();
    if (left <= 0)
        return 0;
    if (left > 300000)
        return 300000;
    return int(left);
}

Iap2WirelessBootstrap::Result Iap2WirelessBootstrap::run(
    Iap2LinkSession &session,
    LocalMfiAuth &mfi,
    const Iap2Protocol::WirelessIdentity &identity,
    const Endpoint &endpoint,
    int timeoutMs,
    const ProgressFn &onProgress,
    const KeepAliveFn &keepAlive)
{
    Result result;
    if (timeoutMs < 1)
        timeoutMs = 1;
    const qint64 deadline = nowMs() + timeoutMs;

    auto fail = [&](Status st, const QString &err) -> Result {
        result.status = st;
        result.error = err;
        return result;
    };

    if (!mfi.isReady())
        return fail(Status::Failed, mfi.errorString().isEmpty()
                                        ? QStringLiteral("MFi not ready")
                                        : mfi.errorString());

    progress(onProgress, QStringLiteral("await ready"));
    if (!session.awaitReady(remainingMs(deadline))) {
        if (session.isDead())
            return fail(Status::Dead, session.deadReason());
        return fail(Status::TimedOut, QStringLiteral("awaitReady timeout"));
    }

    progress(onProgress, QStringLiteral("identification"));
    while (true) {
        const int left = remainingMs(deadline);
        if (left == 0)
            return fail(Status::TimedOut, QStringLiteral("identification timeout"));
        const QByteArray raw = session.recvControl(left);
        if (raw.isEmpty()) {
            if (session.isDead())
                return fail(Status::Dead, session.deadReason());
            return fail(Status::TimedOut, QStringLiteral("identification recv timeout"));
        }
        const quint16 mid = messageIdOf(raw);
        if (mid == Iap2Protocol::kStartIdentification) {
            if (!session.sendControl(Iap2Protocol::buildIdentificationWireless(identity)))
                return fail(Status::Failed, QStringLiteral("send identification failed"));
            progress(onProgress, QStringLiteral("tx=0x1d01"));
        } else if (mid == Iap2Protocol::kIdentificationAccepted) {
            progress(onProgress, QStringLiteral("identification accepted"));
            break;
        } else if (mid == Iap2Protocol::kIdentificationRejected) {
            return fail(Status::Rejected, QStringLiteral("identification rejected"));
        } else {
            return fail(Status::Failed,
                        QStringLiteral("unexpected identification 0x%1").arg(mid, 4, 16, QLatin1Char('0')));
        }
    }

    progress(onProgress, QStringLiteral("mfi"));
    const QByteArray certFrame = Iap2Protocol::buildAuthCertificate(mfi.certificate());
    while (true) {
        const int left = remainingMs(deadline);
        if (left == 0)
            return fail(Status::TimedOut, QStringLiteral("mfi timeout"));
        const QByteArray raw = session.recvControl(left);
        if (raw.isEmpty()) {
            if (session.isDead())
                return fail(Status::Dead, session.deadReason());
            return fail(Status::TimedOut, QStringLiteral("mfi recv timeout"));
        }
        const quint16 mid = messageIdOf(raw);
        if (mid == Iap2Protocol::kRequestCertificate) {
            if (!session.sendControl(certFrame))
                return fail(Status::Failed, QStringLiteral("send certificate failed"));
            progress(onProgress, QStringLiteral("tx=0xaa01"));
        } else if (mid == Iap2Protocol::kRequestChallenge) {
            const QByteArray challenge = Iap2Protocol::parseChallenge(raw);
            if (challenge.isEmpty() || challenge.size() > 128)
                return fail(Status::AuthFailed, QStringLiteral("invalid challenge"));
            const QByteArray sig = mfi.signChallenge(challenge);
            if (sig.isEmpty())
                return fail(Status::AuthFailed, mfi.errorString().isEmpty()
                                                    ? QStringLiteral("sign failed")
                                                    : mfi.errorString());
            if (!session.sendControl(Iap2Protocol::buildAuthResponse(sig)))
                return fail(Status::Failed, QStringLiteral("send auth response failed"));
            progress(onProgress, QStringLiteral("tx=0xaa03"));
        } else if (mid == Iap2Protocol::kAuthenticationSucceeded) {
            progress(onProgress, QStringLiteral("mfi accepted"));
            break;
        } else if (mid == Iap2Protocol::kAuthenticationFailed) {
            return fail(Status::AuthFailed, QStringLiteral("AuthenticationFailed"));
        } else {
            return fail(Status::AuthFailed,
                        QStringLiteral("unexpected mfi 0x%1").arg(mid, 4, 16, QLatin1Char('0')));
        }
    }

    for (const QByteArray &sub : Iap2Protocol::buildMinimalSubscriptions()) {
        if (!session.sendControl(sub))
            return fail(Status::Failed, QStringLiteral("send subscription failed"));
    }
    progress(onProgress, QStringLiteral("subscriptions sent"));

    Iap2Protocol::WirelessStartSession start;
    start.ssid = endpoint.ssid;
    start.passphrase = endpoint.passphrase;
    start.channel = endpoint.channel;
    start.securityType = endpoint.securityType;
    start.ipAddresses = endpoint.ipAddresses;
    start.airPlayPort = endpoint.airPlayPort;
    start.deviceIdentifier = endpoint.deviceIdentifier;
    start.publicKey = endpoint.publicKey;
    start.sourceVersion = endpoint.sourceVersion;

    int preTransportWifi = 0;
    int postTransportWifi = 0;
    constexpr int kMaxPre = 5;
    constexpr int kMaxPost = 2;

    auto sendWifi = [&](bool post) -> bool {
        const int &count = post ? postTransportWifi : preTransportWifi;
        const int limit = post ? kMaxPost : kMaxPre;
        if (count >= limit) {
            progress(onProgress, QStringLiteral("0x5703 limit reached"));
            return true;
        }
        if (!session.sendControl(Iap2Protocol::buildAccessoryWifiConfig(
                endpoint.ssid, endpoint.passphrase, endpoint.channel, endpoint.securityType,
                endpoint.bssid))) {
            return false;
        }
        if (post)
            ++postTransportWifi;
        else
            ++preTransportWifi;
        ++result.wifiConfigsSent;
        progress(onProgress, post ? QStringLiteral("tx=0x5703 post-transport")
                                  : QStringLiteral("tx=0x5703"));
        return true;
    };

    while (true) {
        int left = remainingMs(deadline);
        if (left == 0) {
            if (result.status == Status::Ok) {
                if (keepAlive && keepAlive()) {
                    left = 1000;
                } else {
                    progress(onProgress, QStringLiteral("control keep-alive end"));
                    return result;
                }
            } else {
                result.status = Status::TimedOut;
                result.error = QStringLiteral("control loop timeout");
                return result;
            }
        }
        const QByteArray raw = session.recvControl(left);
        if (raw.isEmpty()) {
            if (session.isDead()) {
                if (result.status == Status::Ok)
                    return result;
                return fail(Status::Dead, session.deadReason());
            }
            if (result.status == Status::Ok) {
                if (keepAlive && keepAlive())
                    continue;
                return result;
            }
            result.status = Status::TimedOut;
            result.error = QStringLiteral("control recv timeout");
            return result;
        }
        const quint16 mid = messageIdOf(raw);
        if (mid == Iap2Protocol::kRequestAccessoryWifiConfiguration) {
            progress(onProgress, QStringLiteral("rx=0x5702"));
            if (!sendWifi(result.transportNotified))
                return fail(Status::Failed, QStringLiteral("send wifi config failed"));
        } else if (mid == Iap2Protocol::kCarPlayAvailability) {
            progress(onProgress, QStringLiteral("rx=0x4300"));
            const QByteArray startFrame = Iap2Protocol::buildCarPlayStartSessionWireless(start);
            if (!session.sendControl(startFrame))
                return fail(Status::Failed, QStringLiteral("send start session failed"));
            ++result.startSessionsSent;
            progress(onProgress,
                     QStringLiteral("tx=0x4301 hex=%1").arg(QString::fromLatin1(startFrame.toHex())));
            result.status = Status::Ok;
            if (!(keepAlive && keepAlive()))
                return result;
        } else if (mid == Iap2Protocol::kWirelessCarPlayUpdate) {
            quint8 avail = 0xff;
            QVector<QPair<quint16, QByteArray>> params;
            if (Iap2Body::parseParams(raw.mid(Iap2Wire::kCsmHeaderBytes), &params)) {
                for (const auto &p : params) {
                    if (p.first == 0 && !p.second.isEmpty())
                        avail = quint8(p.second[0]);
                }
            }
            progress(onProgress, QStringLiteral("rx=0x4e0d available=%1").arg(avail));
        } else if (mid == Iap2Protocol::kDeviceTransportIdentifier) {
            result.transportNotified = true;
            progress(onProgress, QStringLiteral("rx=0x4e0e"));
            if (!sendWifi(true))
                return fail(Status::Failed, QStringLiteral("send post-transport wifi failed"));
        } else {
            progress(onProgress,
                     QStringLiteral("rx=0x%1").arg(mid, 4, 16, QLatin1Char('0')));
        }
        if (result.status == Status::Ok && keepAlive && !keepAlive())
            return result;
    }
}
