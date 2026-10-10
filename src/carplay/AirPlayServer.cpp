#include "AirPlayServer.hpp"

#include "AirPlayAuthSetup.hpp"
#include "AirPlayBplist.hpp"
#include "AirPlayCrypto.hpp"
#include "AirPlayH264Decoder.hpp"
#include "LocalMfiAuth.hpp"

#include <QAbstractSocket>
#include <QBuffer>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QImage>
#include <QMap>
#include <QMetaObject>
#include <QCoreApplication>
#include <QDir>
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QtConcurrent>
#include <QVariant>

#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

namespace {

constexpr int kTlvIdentifier = 0x01;
constexpr int kTlvSalt = 0x02;
constexpr int kTlvPublicKey = 0x03;
constexpr int kTlvProof = 0x04;
constexpr int kTlvEncryptedData = 0x05;
constexpr int kTlvState = 0x06;
constexpr int kTlvError = 0x07;
constexpr int kTlvSignature = 0x0a;
constexpr int kErrorAuth = 2;
constexpr char kSetupUser[] = "Pair-Setup";
constexpr char kSetupPin[] = "3939";

using TlvMap = QHash<int, QByteArray>;

QByteArray tlvEncode(const QList<QPair<int, QByteArray>> &items)
{
    QByteArray out;
    int previousType = -1;
    for (const auto &item : items) {
        if (previousType == item.first)
            out.append("\xff\x00", 2);
        int offset = 0;
        do {
            const int length = qMin(255, item.second.size() - offset);
            out.append(char(item.first));
            out.append(char(length));
            out.append(item.second.mid(offset, length));
            offset += length;
        } while (offset < item.second.size());
        previousType = item.first;
    }
    return out;
}

TlvMap tlvDecode(const QByteArray &buffer)
{
    TlvMap output;
    int position = 0;
    int lastType = -1;
    int lastLength = 0;
    while (position + 2 <= buffer.size()) {
        const int type = uint8_t(buffer[position]);
        const int length = uint8_t(buffer[position + 1]);
        position += 2;
        if (position + length > buffer.size())
            break;
        const QByteArray value = buffer.mid(position, length);
        position += length;
        if (type == lastType && lastLength == 255)
            output[type] = output.value(type) + value;
        else
            output[type] = value;
        lastType = type;
        lastLength = length;
    }
    return output;
}

QByteArray tlvErr(int state)
{
    return tlvEncode({{kTlvState, QByteArray(1, char(state))},
                      {kTlvError, QByteArray(1, char(kErrorAuth))}});
}

constexpr int kNBytes = 384;
constexpr int kLimbs = 96;

struct BigInt {
    uint32_t d[kLimbs]{};
};

const char kNHex[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74"
    "020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437"
    "4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF05"
    "98DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
    "9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
    "3995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D04507A33"
    "A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
    "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6BF12FFA06D98A0864"
    "D87602733EC86A64521F2B18177B200CBBE117577A615D6C770988C0BAD946E2"
    "08E24FA074E5AB3143DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF";

int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

BigInt fromHex(const char *hex)
{
    QByteArray bytes;
    bytes.reserve(kNBytes);
    for (int i = 0; hex[i] && hex[i + 1]; i += 2)
        bytes.append(char((hexVal(hex[i]) << 4) | hexVal(hex[i + 1])));
    BigInt out{};
    const int n = qMin(kNBytes, bytes.size());
    for (int i = 0; i < n; ++i) {
        const int limb = i / 4;
        const int shift = (i % 4) * 8;
        out.d[limb] |= uint32_t(uint8_t(bytes[bytes.size() - 1 - i])) << shift;
    }
    return out;
}

BigInt fromBytesBe(const QByteArray &bytes)
{
    BigInt out{};
    const int n = qMin(kNBytes, bytes.size());
    for (int i = 0; i < n; ++i) {
        const int limb = i / 4;
        const int shift = (i % 4) * 8;
        out.d[limb] |= uint32_t(uint8_t(bytes[bytes.size() - 1 - i])) << shift;
    }
    return out;
}

QByteArray toBytesBe(const BigInt &v, int size = kNBytes)
{
    QByteArray out(size, 0);
    for (int i = 0; i < size; ++i) {
        const int limb = i / 4;
        const int shift = (i % 4) * 8;
        out[size - 1 - i] = char((v.d[limb] >> shift) & 0xff);
    }
    return out;
}

QByteArray toBytesBeMinimal(const BigInt &v)
{
    QByteArray padded = toBytesBe(v, kNBytes);
    int start = 0;
    while (start + 1 < padded.size() && padded[start] == 0)
        ++start;
    return padded.mid(start);
}

int cmp(const BigInt &a, const BigInt &b)
{
    for (int i = kLimbs - 1; i >= 0; --i) {
        if (a.d[i] < b.d[i])
            return -1;
        if (a.d[i] > b.d[i])
            return 1;
    }
    return 0;
}

bool isZero(const BigInt &a)
{
    for (int i = 0; i < kLimbs; ++i) {
        if (a.d[i])
            return false;
    }
    return true;
}

BigInt sub(const BigInt &a, const BigInt &b)
{
    BigInt out{};
    int64_t borrow = 0;
    for (int i = 0; i < kLimbs; ++i) {
        const int64_t t = int64_t(a.d[i]) - int64_t(b.d[i]) - borrow;
        out.d[i] = uint32_t(t);
        borrow = t < 0 ? 1 : 0;
    }
    return out;
}

BigInt modN(BigInt a, const BigInt &n)
{
    while (cmp(a, n) >= 0)
        a = sub(a, n);
    return a;
}

BigInt addMod(const BigInt &a, const BigInt &b, const BigInt &n)
{
    BigInt out{};
    uint64_t carry = 0;
    for (int i = 0; i < kLimbs; ++i) {
        const uint64_t t = uint64_t(a.d[i]) + uint64_t(b.d[i]) + carry;
        out.d[i] = uint32_t(t);
        carry = t >> 32;
    }
    if (carry)
        out = sub(out, n);
    if (cmp(out, n) >= 0)
        out = sub(out, n);
    return out;
}

BigInt dblMod(BigInt a, const BigInt &n)
{
    uint32_t carry = 0;
    for (int i = 0; i < kLimbs; ++i) {
        const uint64_t t = (uint64_t(a.d[i]) << 1) | carry;
        a.d[i] = uint32_t(t);
        carry = uint32_t(t >> 32);
    }
    if (carry)
        a = sub(a, n);
    if (cmp(a, n) >= 0)
        a = sub(a, n);
    return a;
}

BigInt mulMod(const BigInt &a, const BigInt &b, const BigInt &n)
{
    BigInt result{};
    for (int i = kLimbs - 1; i >= 0; --i) {
        for (int bit = 31; bit >= 0; --bit) {
            result = dblMod(result, n);
            if ((a.d[i] >> bit) & 1u)
                result = addMod(result, b, n);
        }
    }
    return result;
}

BigInt modPow(BigInt base, const BigInt &exp, const BigInt &n)
{
    BigInt result{};
    result.d[0] = 1;
    BigInt e = exp;
    base = modN(base, n);
    while (!isZero(e)) {
        if (e.d[0] & 1u)
            result = mulMod(result, base, n);
        base = mulMod(base, base, n);
        uint32_t carry = 0;
        for (int i = kLimbs - 1; i >= 0; --i) {
            const uint32_t next = e.d[i] & 1u;
            e.d[i] = (e.d[i] >> 1) | (carry << 31);
            carry = next;
        }
    }
    return result;
}

BigInt gConst()
{
    BigInt g{};
    g.d[0] = 5;
    return g;
}

struct SrpSession {
    QByteArray salt;
    BigInt publicB;
    QByteArray identifier;
    BigInt verifier;
    BigInt privateB;
};

BigInt padToBig(const QByteArray &bytes)
{
    QByteArray padded(kNBytes, 0);
    const int n = qMin(kNBytes, bytes.size());
    std::memcpy(padded.data() + kNBytes - n, bytes.constData() + bytes.size() - n, size_t(n));
    return fromBytesBe(padded);
}

QByteArray padBytes(const BigInt &v)
{
    return toBytesBe(v, kNBytes);
}

SrpSession srpStart(const QByteArray &username, const QByteArray &password)
{
    const BigInt n = fromHex(kNHex);
    const BigInt g = gConst();
    const QByteArray salt = AirPlayCrypto::randomBytes(16);
    const QByteArray inner = AirPlayCrypto::sha512(QList<QByteArray>{username, QByteArray(1, ':'), password});
    const BigInt x = fromBytesBe(AirPlayCrypto::sha512(salt, inner));
    const BigInt verifier = modPow(g, x, n);
    const BigInt k = fromBytesBe(AirPlayCrypto::sha512(toBytesBeMinimal(n), padBytes(g)));
    const BigInt privateB = fromBytesBe(AirPlayCrypto::randomBytes(32));
    const BigInt gb = modPow(g, privateB, n);
    const BigInt publicB = addMod(mulMod(k, verifier, n), gb, n);

    SrpSession s;
    s.salt = salt;
    s.publicB = publicB;
    s.identifier = username;
    s.verifier = verifier;
    s.privateB = privateB;
    return s;
}

struct SrpResult {
    bool ok = false;
    QByteArray sessionKey;
    QByteArray serverProof;
};

SrpResult srpVerify(const SrpSession &session, const QByteArray &publicA, const QByteArray &clientProof)
{
    const BigInt n = fromHex(kNHex);
    const BigInt a = fromBytesBe(publicA);
    if (isZero(modN(a, n)))
        return {};

    const BigInt u = fromBytesBe(AirPlayCrypto::sha512(padBytes(a), padBytes(session.publicB)));
    const BigInt vu = modPow(session.verifier, u, n);
    const BigInt base = mulMod(a, vu, n);
    const BigInt s = modPow(base, session.privateB, n);
    const QByteArray sessionKey = AirPlayCrypto::sha512(toBytesBeMinimal(s));

    const QByteArray hashN = AirPlayCrypto::sha512(toBytesBeMinimal(n));
    const QByteArray hashG = AirPlayCrypto::sha512(toBytesBeMinimal(gConst()));
    QByteArray hashXor(hashN.size(), 0);
    for (int i = 0; i < hashN.size(); ++i)
        hashXor[i] = char(uint8_t(hashN[i]) ^ uint8_t(hashG[i]));

    const QByteArray expected = AirPlayCrypto::sha512(QList<QByteArray>{
        hashXor, AirPlayCrypto::sha512(session.identifier), session.salt, padBytes(a),
        padBytes(session.publicB), sessionKey});
    if (expected != clientProof)
        return {};

    SrpResult r;
    r.ok = true;
    r.sessionKey = sessionKey;
    r.serverProof = AirPlayCrypto::sha512(QList<QByteArray>{padBytes(a), clientProof, sessionKey});
    return r;
}

class PairSetup {
public:
    explicit PairSetup(AirPlayIdentity *identity, AirPlayServer::PairingStore *store)
        : m_identity(identity)
        , m_store(store)
    {
    }

    QByteArray handle(const QByteArray &body)
    {
        const TlvMap tlv = tlvDecode(body);
        const int state = tlv.contains(kTlvState) ? uint8_t(tlv.value(kTlvState).at(0)) : 0;
        if (state == 1)
            return m2();
        if (state == 3)
            return m4(tlv);
        if (state == 5)
            return m6(tlv);
        return tlvErr(state);
    }

    bool complete = false;

private:
    QByteArray m2()
    {
        m_srp = srpStart(QByteArray(kSetupUser), QByteArray(kSetupPin));
        return tlvEncode({{kTlvState, QByteArray(1, char(2))},
                          {kTlvPublicKey, padBytes(m_srp.publicB)},
                          {kTlvSalt, m_srp.salt}});
    }

    QByteArray m4(const TlvMap &tlv)
    {
        const QByteArray publicA = tlv.value(kTlvPublicKey);
        const QByteArray proof = tlv.value(kTlvProof);
        if (publicA.isEmpty() || proof.isEmpty())
            return tlvErr(4);
        const SrpResult result = srpVerify(m_srp, publicA, proof);
        if (!result.ok)
            return tlvErr(4);
        m_sessionKey = result.sessionKey;
        return tlvEncode({{kTlvState, QByteArray(1, char(4))}, {kTlvProof, result.serverProof}});
    }

    QByteArray m6(const TlvMap &tlv)
    {
        const QByteArray encrypted = tlv.value(kTlvEncryptedData);
        if (m_sessionKey.isEmpty() || encrypted.isEmpty())
            return tlvErr(6);
        const QByteArray encryptionKey = AirPlayCrypto::hkdfSha512(
            m_sessionKey, QByteArray("Pair-Setup-Encrypt-Salt"), QByteArray("Pair-Setup-Encrypt-Info"));
        const QByteArray opened =
            AirPlayCrypto::chachaOpen(encryptionKey, AirPlayCrypto::nonceLabel("PS-Msg05"), encrypted);
        if (opened.isEmpty())
            return tlvErr(6);
        const TlvMap sub = tlvDecode(opened);
        const QByteArray controllerId = sub.value(kTlvIdentifier);
        const QByteArray controllerLtpk = sub.value(kTlvPublicKey);
        const QByteArray controllerSignature = sub.value(kTlvSignature);
        if (controllerId.isEmpty() || controllerLtpk.isEmpty() || controllerSignature.isEmpty())
            return tlvErr(6);

        const QByteArray controllerSignKey = AirPlayCrypto::hkdfSha512(
            m_sessionKey, QByteArray("Pair-Setup-Controller-Sign-Salt"),
            QByteArray("Pair-Setup-Controller-Sign-Info"));
        const QByteArray controllerSignData =
            AirPlayCrypto::concat({controllerSignKey, controllerId, controllerLtpk});
        if (!AirPlayCrypto::ed25519Verify(controllerLtpk, controllerSignData, controllerSignature))
            return tlvErr(6);
        m_store->save(QString::fromUtf8(controllerId), controllerLtpk);

        const QByteArray accessoryId = m_identity->pairingId().toUtf8();
        const QByteArray accessorySignKey = AirPlayCrypto::hkdfSha512(
            m_sessionKey, QByteArray("Pair-Setup-Accessory-Sign-Salt"),
            QByteArray("Pair-Setup-Accessory-Sign-Info"));
        const QByteArray accessorySignData =
            AirPlayCrypto::concat({accessorySignKey, accessoryId, m_identity->publicKey()});
        const QByteArray accessorySignature =
            AirPlayCrypto::ed25519Sign(m_identity->privateKey(), accessorySignData);
        const QByteArray subResponse = tlvEncode({{kTlvIdentifier, accessoryId},
                                                  {kTlvPublicKey, m_identity->publicKey()},
                                                  {kTlvSignature, accessorySignature}});
        const QByteArray sealed =
            AirPlayCrypto::chachaSeal(encryptionKey, AirPlayCrypto::nonceLabel("PS-Msg06"), subResponse);
        complete = true;
        return tlvEncode({{kTlvState, QByteArray(1, char(6))}, {kTlvEncryptedData, sealed}});
    }

    AirPlayIdentity *m_identity = nullptr;
    AirPlayServer::PairingStore *m_store = nullptr;
    SrpSession m_srp;
    QByteArray m_sessionKey;
};

class PairVerify {
public:
    explicit PairVerify(AirPlayIdentity *identity, AirPlayServer::PairingStore *store)
        : m_identity(identity)
        , m_store(store)
    {
    }

    QByteArray handle(const QByteArray &body)
    {
        const TlvMap tlv = tlvDecode(body);
        const int state = tlv.contains(kTlvState) ? uint8_t(tlv.value(kTlvState).at(0)) : 0;
        if (state == 1)
            return m2(tlv);
        if (state == 3)
            return m4(tlv);
        return tlvErr(state);
    }

    bool verified = false;
    QByteArray sharedSecret;
    QByteArray controlReadKey;
    QByteArray controlWriteKey;

private:
    QByteArray m2(const TlvMap &tlv)
    {
        const QByteArray clientEphemeral = tlv.value(kTlvPublicKey);
        if (clientEphemeral.isEmpty())
            return tlvErr(2);
        const auto pair = AirPlayCrypto::x25519Generate();
        m_ephemeralPublic = pair.publicKey;
        m_clientEphemeral = clientEphemeral;
        m_shared = AirPlayCrypto::x25519Shared(pair.privateKey, clientEphemeral);
        m_encryptionKey = AirPlayCrypto::hkdfSha512(m_shared, QByteArray("Pair-Verify-Encrypt-Salt"),
                                                    QByteArray("Pair-Verify-Encrypt-Info"));

        const QByteArray accessoryId = m_identity->pairingId().toUtf8();
        const QByteArray signature = AirPlayCrypto::ed25519Sign(
            m_identity->privateKey(),
            AirPlayCrypto::concat({pair.publicKey, accessoryId, clientEphemeral}));
        const QByteArray sub =
            tlvEncode({{kTlvIdentifier, accessoryId}, {kTlvSignature, signature}});
        const QByteArray sealed =
            AirPlayCrypto::chachaSeal(m_encryptionKey, AirPlayCrypto::nonceLabel("PV-Msg02"), sub);
        return tlvEncode({{kTlvState, QByteArray(1, char(2))},
                          {kTlvPublicKey, pair.publicKey},
                          {kTlvEncryptedData, sealed}});
    }

    QByteArray m4(const TlvMap &tlv)
    {
        const QByteArray encrypted = tlv.value(kTlvEncryptedData);
        if (m_encryptionKey.isEmpty() || m_shared.isEmpty() || m_ephemeralPublic.isEmpty() ||
            m_clientEphemeral.isEmpty() || encrypted.isEmpty())
            return tlvErr(4);
        const QByteArray opened =
            AirPlayCrypto::chachaOpen(m_encryptionKey, AirPlayCrypto::nonceLabel("PV-Msg03"), encrypted);
        if (opened.isNull())
            return tlvErr(4);
        const TlvMap sub = tlvDecode(opened);
        const QByteArray controllerId = sub.value(kTlvIdentifier);
        const QByteArray controllerSignature = sub.value(kTlvSignature);
        if (controllerId.isEmpty() || controllerSignature.isEmpty())
            return tlvErr(4);
        const QByteArray controllerLtpk = m_store->get(QString::fromUtf8(controllerId));
        if (controllerLtpk.isEmpty())
            return tlvErr(4);
        const QByteArray signatureData =
            AirPlayCrypto::concat({m_clientEphemeral, controllerId, m_ephemeralPublic});
        if (!AirPlayCrypto::ed25519Verify(controllerLtpk, signatureData, controllerSignature))
            return tlvErr(4);
        sharedSecret = m_shared;
        controlReadKey = AirPlayCrypto::hkdfSha512(m_shared, QByteArray("Control-Salt"),
                                                   QByteArray("Control-Write-Encryption-Key"));
        controlWriteKey = AirPlayCrypto::hkdfSha512(m_shared, QByteArray("Control-Salt"),
                                                    QByteArray("Control-Read-Encryption-Key"));
        verified = true;
        return tlvEncode({{kTlvState, QByteArray(1, char(4))}});
    }

    AirPlayIdentity *m_identity = nullptr;
    AirPlayServer::PairingStore *m_store = nullptr;
    QByteArray m_ephemeralPublic;
    QByteArray m_clientEphemeral;
    QByteArray m_shared;
    QByteArray m_encryptionKey;
};

QString unsignedPlistDecimal(const QVariant &value)
{
    return QString::number(quint64(value.toLongLong()));
}

bool isLocalPeer(const QHostAddress &peer)
{
    if (peer.isNull() || peer.isLoopback())
        return true;
    const QHostAddress v4 = peer.toIPv4Address() ? QHostAddress(peer.toIPv4Address()) : peer;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip() == peer || entry.ip() == v4)
                return true;
        }
    }
    return false;
}

}

struct AirPlayServer::ClientState {
    QTcpSocket *socket = nullptr;
    QByteArray buffer;
    QByteArray plain;
    std::unique_ptr<PairSetup> pairSetup;
    std::unique_ptr<PairVerify> pairVerify;
    std::unique_ptr<AirPlayControlCipher> cipher;
    bool enableCipherAfterResponse = false;
    bool announced = false;
};

AirPlayServer::AirPlayServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &AirPlayServer::onNewConnection);
    connect(&m_eventServer, &QTcpServer::newConnection, this, &AirPlayServer::onEventNewConnection);
    m_timingTimer.setInterval(1000);
    connect(&m_timingTimer, &QTimer::timeout, this, &AirPlayServer::onTimingTick);
}

AirPlayServer::~AirPlayServer()
{
    stop();
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    delete static_cast<AirPlayH264Decoder *>(m_mfDecoder);
    m_mfDecoder = nullptr;
#endif
}

bool AirPlayServer::start(AirPlayIdentity *identity, LocalMfiAuth *mfi, const QString &deviceName,
                          const QString &deviceId, const QString &model, const QHostAddress &bindAddress,
                          const QString &sourceVersion, const QString &bluetoothMac)
{
    stop();
    if (!identity) {
        emit failed(QStringLiteral("identity is null"));
        return false;
    }
    m_identity = identity;
    m_mfi = mfi;
    m_deviceName = deviceName;
    m_deviceId = deviceId;
    m_model = model;
    m_bindAddress = bindAddress;
    m_sourceVersion = sourceVersion.isEmpty() ? QStringLiteral("950.7.1") : sourceVersion;
    m_bluetoothMac = bluetoothMac.isEmpty() ? deviceId : bluetoothMac;
    m_pairings = std::make_unique<PairingStore>();

    quint16 port = 7000;
    bool ok = false;
    for (; port <= 7010; ++port) {
        if (m_server.listen(bindAddress, port)) {
            ok = true;
            break;
        }
    }
    if (!ok) {
        emit failed(QStringLiteral("TCP listen 7000-7010 failed: %1").arg(m_server.errorString()));
        return false;
    }
    m_port = m_server.serverPort();
    m_timingPort = ensureTimingUdp();
    m_eventPort = ensureEventTcp();
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (!m_mfDecoder)
        m_mfDecoder = new AirPlayH264Decoder;
    auto *decoder = static_cast<AirPlayH264Decoder *>(m_mfDecoder);
    (void)QtConcurrent::run([decoder]() {
        if (decoder)
            decoder->preload();
    });
#endif
    emit listening(m_port);
    emit log(QStringLiteral("AirPlay listening on %1 bind=%2 event=%3")
                 .arg(m_port)
                 .arg(bindAddress.toString())
                 .arg(m_eventPort));
    return true;
}

void AirPlayServer::stop()
{
    const auto sockets = m_clients.keys();
    for (QTcpSocket *sock : sockets) {
        sock->disconnect(this);
        sock->close();
        delete m_clients.take(sock);
        sock->deleteLater();
    }
    m_server.close();
    if (m_eventSock) {
        m_eventSock->disconnect(this);
        m_eventSock->abort();
        m_eventSock->deleteLater();
        m_eventSock = nullptr;
    }
    m_eventServer.close();
    m_eventCipher.reset();
    m_eventBuf.clear();
    m_eventPlain.clear();
    m_eventOut.clear();
    m_touchDown = false;
    m_lastTouchSendMs = 0;
    m_lastTouchPx = -1;
    m_lastTouchPy = -1;
    m_timingTimer.stop();
    m_timingPeer = QHostAddress();
    m_timingPeerPort = 0;
    m_timingSock.reset();
    resetNtpClock();
    m_keepAliveSock.reset();
    m_screen.reset();
    m_audio.reset();
    m_buffered.reset();
    if (m_pcm)
        m_pcm->stop();
    m_pcm.reset();
    m_sharedSecret.clear();
    m_sps.clear();
    m_pps.clear();
    m_videoFrame = QImage();
    emit videoFrameChanged();
    m_timingPort = 0;
    m_eventPort = 0;
    m_keepAlivePort = 0;
    m_eventCseq = 0;
    m_decodeBusy.store(false);
    m_waitIdr.store(false);
    {
        QMutexLocker lock(&m_decodeMutex);
        m_decodeQueue.clear();
    }
    m_port = 0;
    m_identity = nullptr;
    m_mfi = nullptr;
    m_pairings.reset();
}

namespace {

constexpr double kNtpTwo32 = 4294967296.0;
constexpr double kNtpStepThresholdSec = 0.128;
constexpr double kNtpSlewGain = 1.0 / 8.0;
constexpr int kNtpDelayWindow = 8;
constexpr int kNtpPickCount = 2;

quint64 ntp64FromUnixMs(qint64 ms)
{
    const quint64 seconds = quint64(ms / 1000) + 2208988800ULL;
    const quint64 frac = (quint64(ms % 1000) << 32) / 1000ULL;
    return (seconds << 32) | frac;
}

quint64 ntp64FromNanos(qint64 ns)
{
    const qint64 sec = ns / 1000000000LL;
    qint64 rem = ns % 1000000000LL;
    if (rem < 0) {
        rem += 1000000000LL;
    }
    const quint64 frac = (quint64(rem) << 32) / 1000000000ULL;
    return (quint64(sec) << 32) | frac;
}

qint64 ntp64ToNanos(quint64 ntp)
{
    const qint64 sec = qint64(ntp >> 32);
    const qint64 fracNs = qint64(((ntp & 0xffffffffULL) * 1000000000ULL) >> 32);
    return sec * 1000000000LL + fracNs;
}

QByteArray ntp64ToBytes(quint64 ntp)
{
    QByteArray out(8, 0);
    for (int i = 0; i < 8; ++i)
        out[i] = char((ntp >> (8 * (7 - i))) & 0xff);
    return out;
}

quint64 ntp64FromBytes(const QByteArray &msg, int off)
{
    quint64 ntp = 0;
    for (int i = 0; i < 8; ++i)
        ntp = (ntp << 8) | quint8(msg.at(off + i));
    return ntp;
}

} // namespace

void AirPlayServer::resetNtpClock()
{
    m_ntpMono.invalidate();
    m_ntpClockOffsetNs = 0;
    m_ntpSynced = false;
    m_ntpPendingT1 = 0;
    m_ntpHasPendingT1 = false;
    m_ntpDelayIndex = 0;
    m_ntpPickCount = kNtpPickCount;
    m_ntpPickRtt = std::numeric_limits<double>::infinity();
    m_ntpPickOffset = 0;
    for (int i = 0; i < kNtpDelayWindow; ++i)
        m_ntpDelays[i] = std::numeric_limits<double>::infinity();
}

quint64 AirPlayServer::syncedNtp64() const
{
    if (!m_ntpMono.isValid())
        return ntp64FromUnixMs(QDateTime::currentMSecsSinceEpoch());
    return ntp64FromNanos(m_ntpMono.nsecsElapsed() + m_ntpClockOffsetNs);
}

QByteArray AirPlayServer::ntpNowBytes() const
{
    return ntp64ToBytes(syncedNtp64());
}

void AirPlayServer::handleTimingResponse(const QByteArray &msg)
{
    const quint64 t4 = syncedNtp64();
    const quint64 t1 = ntp64FromBytes(msg, 8);
    const quint64 t2 = ntp64FromBytes(msg, 16);
    const quint64 t3 = ntp64FromBytes(msg, 24);
    if (!m_ntpHasPendingT1 || m_ntpPendingT1 != t1)
        return;
    m_ntpHasPendingT1 = false;

    const double offset = 0.5 * (double(qint64(t2 - t1)) + double(qint64(t3 - t4))) / kNtpTwo32;
    const double rtt = (double(qint64(t4 - t1)) - double(qint64(t3 - t2))) / kNtpTwo32;
    if (rtt < 0.0)
        return;

    if (rtt < m_ntpPickRtt) {
        m_ntpPickRtt = rtt;
        m_ntpPickOffset = offset;
    }
    if (--m_ntpPickCount > 0)
        return;

    const double selectedRtt = m_ntpPickRtt;
    const double selectedOffset = m_ntpPickOffset;
    m_ntpPickCount = kNtpPickCount;
    m_ntpPickRtt = std::numeric_limits<double>::infinity();

    bool useSample = true;
    for (int i = 0; i < kNtpDelayWindow; ++i) {
        if (selectedRtt > m_ntpDelays[i]) {
            useSample = false;
            break;
        }
    }
    m_ntpDelays[m_ntpDelayIndex] = selectedRtt;
    m_ntpDelayIndex = (m_ntpDelayIndex + 1) % kNtpDelayWindow;
    if (!useSample)
        return;

    const bool stepping = !m_ntpSynced || std::fabs(selectedOffset) > kNtpStepThresholdSec;
    const double applied = stepping ? selectedOffset : selectedOffset * kNtpSlewGain;
    m_ntpClockOffsetNs += qint64(std::llround(applied * 1e9));
    if (stepping) {
        for (int i = 0; i < kNtpDelayWindow; ++i)
            m_ntpDelays[i] = std::numeric_limits<double>::infinity();
        m_ntpDelayIndex = 0;
        m_ntpHasPendingT1 = false;
        m_ntpSynced = true;
        emit log(QStringLiteral("timing synced offsetMs=%1 rttMs=%2")
                     .arg(selectedOffset * 1000.0, 0, 'f', 2)
                     .arg(selectedRtt * 1000.0, 0, 'f', 2));
    }
}

quint16 AirPlayServer::ensureTimingUdp()
{
    m_timingSock = std::make_unique<QUdpSocket>(this);
    if (!m_timingSock->bind(QHostAddress::AnyIPv4, 0))
        return 0;
    connect(m_timingSock.get(), &QUdpSocket::readyRead, this, &AirPlayServer::onTimingReadyRead);
    return m_timingSock->localPort();
}

void AirPlayServer::startTimingPeer(const QHostAddress &peer, quint16 peerPort)
{
    m_timingPeer = peer;
    m_timingPeerPort = peerPort;
    resetNtpClock();
    m_ntpMono.start();
    m_ntpClockOffsetNs = ntp64ToNanos(ntp64FromUnixMs(QDateTime::currentMSecsSinceEpoch()));
    if (peerPort > 0 && !peer.isNull()) {
        onTimingTick();
        m_timingTimer.start();
    }
}

void AirPlayServer::onTimingReadyRead()
{
    if (!m_timingSock)
        return;
    while (m_timingSock->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_timingSock->receiveDatagram();
        const QByteArray msg = dg.data();
        if (msg.size() < 32)
            continue;
        const int pt = quint8(msg[1]);
        if (pt != 210 && pt != 211)
            continue;
        if (pt == 211) {
            handleTimingResponse(msg);
            continue;
        }
        QByteArray response(32, 0);
        response[0] = char(0x80);
        response[1] = char(211);
        response[2] = 0;
        response[3] = 7;
        response.replace(8, 8, msg.mid(24, 8));
        const QByteArray now = ntpNowBytes();
        response.replace(16, 8, now);
        response.replace(24, 8, now);
        m_timingSock->writeDatagram(response, dg.senderAddress(), quint16(dg.senderPort()));
    }
}

void AirPlayServer::onTimingTick()
{
    if (!m_timingSock || m_timingPeerPort == 0 || m_timingPeer.isNull())
        return;
    if (!m_ntpMono.isValid()) {
        m_ntpMono.start();
        m_ntpClockOffsetNs = ntp64ToNanos(ntp64FromUnixMs(QDateTime::currentMSecsSinceEpoch()));
    }
    const quint64 t1 = syncedNtp64();
    m_ntpPendingT1 = t1;
    m_ntpHasPendingT1 = true;
    QByteArray req(32, 0);
    req[0] = char(0x80);
    req[1] = char(210);
    req[2] = 0;
    req[3] = 7;
    req.replace(24, 8, ntp64ToBytes(t1));
    static int txCount = 0;
    const qint64 n = m_timingSock->writeDatagram(req, m_timingPeer, m_timingPeerPort);
    if (txCount < 5) {
        ++txCount;
        emit log(QStringLiteral("timing tx 210 -> %1:%2 n=%3")
                     .arg(m_timingPeer.toString())
                     .arg(m_timingPeerPort)
                     .arg(n));
    }
}

quint16 AirPlayServer::ensureEventTcp()
{
    if (!m_eventServer.listen(m_bindAddress, 0))
        return 0;
    return m_eventServer.serverPort();
}

quint16 AirPlayServer::ensureKeepAliveUdp()
{
    m_keepAliveSock = std::make_unique<QUdpSocket>(this);
    if (!m_keepAliveSock->bind(QHostAddress::AnyIPv4, 0))
        return 0;
    connect(m_keepAliveSock.get(), &QUdpSocket::readyRead, this, [this] {
        if (!m_keepAliveSock)
            return;
        while (m_keepAliveSock->hasPendingDatagrams()) {
            const QNetworkDatagram dg = m_keepAliveSock->receiveDatagram();
            emit log(QStringLiteral("keepalive rx %1 bytes from %2:%3")
                         .arg(dg.data().size())
                         .arg(dg.senderAddress().toString())
                         .arg(dg.senderPort()));
        }
    });
    return m_keepAliveSock->localPort();
}

void AirPlayServer::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *sock = m_server.nextPendingConnection();
        auto *state = new ClientState;
        state->socket = sock;
        state->pairSetup = std::make_unique<PairSetup>(m_identity, m_pairings.get());
        state->pairVerify = std::make_unique<PairVerify>(m_identity, m_pairings.get());
        m_clients.insert(sock, state);
        connect(sock, &QTcpSocket::readyRead, this, &AirPlayServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &AirPlayServer::onClientDisconnected);
        emit log(QStringLiteral("tcp accept %1 local=%2")
                     .arg(sock->peerAddress().toString())
                     .arg(isLocalPeer(sock->peerAddress()) ? QStringLiteral("1") : QStringLiteral("0")));
    }
}

void AirPlayServer::onClientDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    delete m_clients.take(sock);
    sock->deleteLater();
}

void AirPlayServer::onClientReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !m_clients.contains(sock))
        return;
    ClientState *client = m_clients.value(sock);
    QByteArray chunk = sock->readAll();
    if (client->cipher) {
        client->buffer.append(chunk);
        QByteArray rest;
        const QByteArray plain = client->cipher->decrypt(client->buffer, &rest);
        client->buffer = rest;
        if (plain.isNull())
            return;
        client->plain.append(plain);
    } else {
        client->plain.append(chunk);
    }
    processClientBuffer(client);
}

void AirPlayServer::processClientBuffer(ClientState *client)
{
    const QByteArray headerEnd("\r\n\r\n");
    while (true) {
        const int headerEndIndex = client->plain.indexOf(headerEnd);
        if (headerEndIndex < 0)
            break;
        const QByteArray headerText = client->plain.left(headerEndIndex);
        const QList<QByteArray> lines = headerText.split('\n');
        if (lines.isEmpty()) {
            client->plain.remove(0, headerEndIndex + 4);
            continue;
        }
        QByteArray requestLine = lines.first();
        if (requestLine.endsWith('\r'))
            requestLine.chop(1);
        const QList<QByteArray> parts = requestLine.split(' ');
        const QByteArray method = parts.value(0);
        const QString path = QString::fromLatin1(parts.value(1));
        const QString protocol = QString::fromLatin1(parts.value(2, "RTSP/1.0"));

        QHash<QString, QString> headers;
        for (int i = 1; i < lines.size(); ++i) {
            QByteArray line = lines[i];
            if (line.endsWith('\r'))
                line.chop(1);
            const int sep = line.indexOf(':');
            if (sep < 0)
                continue;
            headers.insert(QString::fromLatin1(line.left(sep)).trimmed().toLower(),
                           QString::fromLatin1(line.mid(sep + 1)).trimmed());
        }
        const int contentLength = headers.value(QStringLiteral("content-length")).toInt();
        const int bodyStart = headerEndIndex + 4;
        if (client->plain.size() < bodyStart + contentLength)
            break;
        const QByteArray body = client->plain.mid(bodyStart, contentLength);
        client->plain.remove(0, bodyStart + contentLength);
        handleRequest(client, method, path, protocol, headers, body);
    }
}

void AirPlayServer::sendResponse(ClientState *client, const QString &protocol, int status,
                                 const QHash<QString, QString> &headers, const QByteArray &body,
                                 const QString &cseq)
{
    static const QHash<int, QString> statusText = {
        {200, QStringLiteral("OK")},
        {400, QStringLiteral("Bad Request")},
        {404, QStringLiteral("Not Found")},
        {500, QStringLiteral("Internal Server Error")},
    };
    QByteArray head;
    head.append(protocol.toLatin1());
    head.append(' ');
    head.append(QByteArray::number(status));
    head.append(' ');
    head.append(statusText.value(status, QStringLiteral("OK")).toLatin1());
    head.append("\r\n");
    QHash<QString, QString> out = headers;
    if (!cseq.isEmpty())
        out.insert(QStringLiteral("CSeq"), cseq);
    out.insert(QStringLiteral("Content-Length"), QString::number(body.size()));
    for (auto it = out.constBegin(); it != out.constEnd(); ++it) {
        head.append(it.key().toLatin1());
        head.append(": ");
        head.append(it.value().toLatin1());
        head.append("\r\n");
    }
    head.append("\r\n");
    QByteArray wire = head;
    if (!body.isEmpty())
        wire.append(body);
    if (client->cipher)
        wire = client->cipher->encrypt(wire);
    client->socket->write(wire);
    if (client->enableCipherAfterResponse && client->pairVerify && client->pairVerify->verified) {
        client->cipher = std::make_unique<AirPlayControlCipher>(client->pairVerify->controlReadKey,
                                                                client->pairVerify->controlWriteKey);
        m_sharedSecret = client->pairVerify->sharedSecret;
        client->enableCipherAfterResponse = false;
        emit log(QStringLiteral("control encryption enabled"));
    }
}

static QByteArray touchHidDescriptor(int xMax, int yMax)
{
    auto finger = [&]() {
        QByteArray f = QByteArray::fromHex("050d0922a1020938750895018102150025010933750195018102950781030501");
        f.append(char(0x26));
        f.append(char(xMax & 0xff));
        f.append(char((xMax >> 8) & 0xff));
        f.append(QByteArray::fromHex("0930751095018102"));
        f.append(char(0x26));
        f.append(char(yMax & 0xff));
        f.append(char((yMax >> 8) & 0xff));
        f.append(QByteArray::fromHex("09318102c0"));
        return f;
    };
    QByteArray out = QByteArray::fromHex("050d0904a101");
    out.append(finger());
    out.append(finger());
    out.append(char(0xc0));
    return out;
}

void AirPlayServer::setDisplaySize(int width, int height)
{
    if (width < 320)
        width = 320;
    if (height < 240)
        height = 240;
    if (width > 3840)
        width = 3840;
    if (height > 2160)
        height = 2160;
    m_displayW = width;
    m_displayH = height;
}

void AirPlayServer::setSafeAreaInsets(int top, int bottom, int left, int right)
{
    m_safeTop = qMax(0, top);
    m_safeBottom = qMax(0, bottom);
    m_safeLeft = qMax(0, left);
    m_safeRight = qMax(0, right);
}

QByteArray AirPlayServer::buildMediaReport(quint8 usageIndex)
{
    QByteArray report(1, char(usageIndex));
    return report;
}

bool AirPlayServer::sendHidReport(const QString &uuid, const QByteArray &report)
{
    QVariantMap cmd;
    cmd.insert(QStringLiteral("type"), QStringLiteral("hidSendReport"));
    cmd.insert(QStringLiteral("uuid"), uuid);
    cmd.insert(QStringLiteral("hidReport"), report);
    return sendEventCommand(cmd);
}

bool AirPlayServer::setNightMode(bool night)
{
    m_nightMode = night;
    QVariantMap cmd;
    cmd.insert(QStringLiteral("type"), QStringLiteral("appearanceUpdate"));
    cmd.insert(QStringLiteral("appearanceModes"), night ? 1 : 0);
    if (!sendEventCommand(cmd))
        return false;
    QVariantMap nightCmd;
    nightCmd.insert(QStringLiteral("type"), QStringLiteral("nightModeUpdate"));
    nightCmd.insert(QStringLiteral("nightMode"), night);
    sendEventCommand(nightCmd);
    emit log(QStringLiteral("nightMode %1").arg(int(night)));
    return true;
}

QString AirPlayServer::mapManeuverTurn(const QVariant &maneuver)
{
    const QString s = maneuver.toString().toLower();
    if (s.contains(QStringLiteral("left")) || s.contains(QStringLiteral("左")))
        return QStringLiteral("left");
    if (s.contains(QStringLiteral("right")) || s.contains(QStringLiteral("右")))
        return QStringLiteral("right");
    if (s.contains(QStringLiteral("arrive")) || s.contains(QStringLiteral("到达"))
        || s.contains(QStringLiteral("dest")))
        return QStringLiteral("arrive");
    bool ok = false;
    const int code = maneuver.toInt(&ok);
    if (ok) {
        // Common CarPlay maneuver enums (subset).
        if (code == 2 || code == 3 || code == 4)
            return QStringLiteral("left");
        if (code == 5 || code == 6 || code == 7)
            return QStringLiteral("right");
        if (code == 16 || code == 17)
            return QStringLiteral("arrive");
    }
    return QStringLiteral("straight");
}

void AirPlayServer::handleIncomingCommand(const QVariantMap &cmd)
{
    const QString type = cmd.value(QStringLiteral("type")).toString();
    QVariantMap params = cmd.value(QStringLiteral("params")).toMap();
    if (params.isEmpty())
        params = cmd;

    if (type == QLatin1String("requestUI")) {
        const QString url = params.value(QStringLiteral("url")).toString();
        if (!url.startsWith(QLatin1String("videoplayback:"))) {
            emit log(QStringLiteral("host UI requested (car icon)"));
            emit hostUiRequested();
        }
        return;
    }

    if (type.contains(QStringLiteral("nowPlaying"), Qt::CaseInsensitive)
        || params.contains(QStringLiteral("mediaItem"))
        || params.contains(QStringLiteral("playbackStatus"))) {
        QVariantMap item = params.value(QStringLiteral("mediaItem")).toMap();
        if (item.isEmpty())
            item = params;
        const QString title = item.value(QStringLiteral("title"),
                                         item.value(QStringLiteral("songTitle"))).toString();
        QString artist = item.value(QStringLiteral("artist")).toString();
        if (artist.isEmpty())
            artist = item.value(QStringLiteral("albumArtist")).toString();
        int durationMs = item.value(QStringLiteral("playbackDurationInMilliseconds"),
                                    item.value(QStringLiteral("durationInMilliseconds")))
                             .toInt();
        if (durationMs <= 0)
            durationMs = int(item.value(QStringLiteral("playbackDuration")).toDouble() * 1000.0);
        int elapsedMs = params.value(QStringLiteral("elapsedTimeInMilliseconds"),
                                     params.value(QStringLiteral("elapsedTime")))
                            .toInt();
        if (elapsedMs <= 0 && params.contains(QStringLiteral("elapsedTime")))
            elapsedMs = int(params.value(QStringLiteral("elapsedTime")).toDouble() * 1000.0);
        const int status = params.value(QStringLiteral("playbackStatus"), 1).toInt();
        const bool playing = status == 1 || status == 2
            || params.value(QStringLiteral("playbackRate")).toDouble() > 0.0;
        if (!title.isEmpty() || !artist.isEmpty()) {
            emit nowPlayingInfo(title, artist, playing, qMax(0, elapsedMs / 1000),
                                qMax(1, durationMs / 1000));
        }
        return;
    }

    if (type.contains(QStringLiteral("nav"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("turnByTurn"), Qt::CaseInsensitive)
        || params.contains(QStringLiteral("maneuverDescription"))
        || params.contains(QStringLiteral("destinationName"))) {
        const QString dest = params.value(QStringLiteral("destinationName"),
                                          params.value(QStringLiteral("destination")))
                                 .toString();
        QString text = params.value(QStringLiteral("maneuverDescription"),
                                    params.value(QStringLiteral("instruction")))
                           .toString();
        const QString dist = params.value(QStringLiteral("distanceRemainingDisplayString"),
                                          params.value(QStringLiteral("distanceRemaining")))
                                 .toString();
        if (!dist.isEmpty() && !text.contains(dist))
            text = dist + QLatin1Char(' ') + text;
        const QString turn = mapManeuverTurn(params.value(QStringLiteral("maneuverType"),
                                                          params.value(QStringLiteral("maneuver"))));
        int etaMin = params.value(QStringLiteral("etaMinutes")).toInt();
        if (etaMin <= 0) {
            const qint64 etaSec = params.value(QStringLiteral("timeRemaining")).toLongLong();
            if (etaSec > 0)
                etaMin = int((etaSec + 59) / 60);
        }
        const int speedLimit = params.value(QStringLiteral("speedLimit"),
                                            params.value(QStringLiteral("currentRoadSpeedLimit")))
                                   .toInt();
        const bool active = !params.value(QStringLiteral("stopped")).toBool()
            && (!text.isEmpty() || !dest.isEmpty());
        emit navigationInfo(active, text, turn, speedLimit, etaMin, dest);
        return;
    }

    if (type.contains(QStringLiteral("notif"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("message"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("sms"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("bulletin"), Qt::CaseInsensitive)
        || params.contains(QStringLiteral("notification"))
        || params.contains(QStringLiteral("messageBody"))
        || params.contains(QStringLiteral("smsBody"))) {
        QVariantMap n = params.value(QStringLiteral("notification")).toMap();
        if (n.isEmpty())
            n = params;
        const QString app = n.value(QStringLiteral("appName"),
                                    n.value(QStringLiteral("applicationName"),
                                            n.value(QStringLiteral("bundleDisplayName"))))
                                .toString();
        const QString title = n.value(QStringLiteral("title"),
                                      n.value(QStringLiteral("subtitle"),
                                              n.value(QStringLiteral("sender"))))
                                  .toString();
        const QString body = n.value(QStringLiteral("body"),
                                     n.value(QStringLiteral("message"),
                                             n.value(QStringLiteral("messageBody"),
                                                     n.value(QStringLiteral("text")))))
                                 .toString();
        if (!app.isEmpty() || !title.isEmpty() || !body.isEmpty())
            emit notificationInfo(app, title, body);
        return;
    }

    if (type.contains(QStringLiteral("telephon"), Qt::CaseInsensitive)
        || type.contains(QStringLiteral("call"), Qt::CaseInsensitive)
        || params.contains(QStringLiteral("callState"))
        || params.contains(QStringLiteral("telephony"))) {
        QVariantMap tel = params.value(QStringLiteral("telephony")).toMap();
        if (tel.isEmpty())
            tel = params;
        const int state = tel.value(QStringLiteral("callState"),
                                    tel.value(QStringLiteral("status")))
                              .toInt();
        const QString name = tel.value(QStringLiteral("displayName"),
                                       tel.value(QStringLiteral("callerName")))
                                 .toString();
        const QString number = tel.value(QStringLiteral("remoteID"),
                                         tel.value(QStringLiteral("number")))
                                   .toString();
        // 0 idle, 1 ringing, 2 connecting, 3 active (common mapping)
        const bool ringing = state == 1;
        const bool active = state == 2 || state == 3 || state == 4;
        emit telephonyInfo(active, ringing, name, number);
        return;
    }

    if (type == QLatin1String("modesChanged")) {
        const QVariantList appStates = params.value(QStringLiteral("appStates")).toList();
        for (const QVariant &a : appStates) {
            const QVariantMap am = a.toMap();
            const qint64 id = am.value(QStringLiteral("appStateID")).toLongLong();
            const bool on = am.value(QStringLiteral("state")).toBool()
                || am.value(QStringLiteral("state")).toInt() > 0;
            // INFO advertises: 1=speech, 2=phone?, 3=turnByTurn?
            if (id == 3)
                emit navigationInfo(on, on ? QStringLiteral("导航中") : QString(),
                                    QStringLiteral("straight"), 0, 0, {});
            if (id == 2)
                emit telephonyInfo(on, false, on ? QStringLiteral("通话中") : QString(), {});
        }
    }
}

bool AirPlayServer::sendLocation(double latitude, double longitude, double altitude, double speedMps,
                                 double course, double accuracy)
{
    if (!m_eventSock || !m_eventCipher || !m_eventCipher->isValid())
        return false;
    QVariantMap location;
    location.insert(QStringLiteral("latitude"), latitude);
    location.insert(QStringLiteral("longitude"), longitude);
    location.insert(QStringLiteral("altitude"), altitude);
    location.insert(QStringLiteral("speed"), speedMps >= 0.0 ? speedMps : -1.0);
    location.insert(QStringLiteral("course"), course >= 0.0 ? course : -1.0);
    location.insert(QStringLiteral("horizontalAccuracy"), accuracy > 0.0 ? accuracy : 25.0);
    location.insert(QStringLiteral("verticalAccuracy"), accuracy > 0.0 ? accuracy * 1.5 : 40.0);
    location.insert(QStringLiteral("timestamp"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    QVariantMap cmd;
    cmd.insert(QStringLiteral("type"), QStringLiteral("locationUpdate"));
    cmd.insert(QStringLiteral("location"), location);
    return sendEventCommand(cmd);
}

bool AirPlayServer::sendHardKey(const QString &key, bool down)
{
    const QString k = key.trimmed().toLower();
    if (k == QLatin1String("home") && down) {
        emit hostUiRequested();
        return true;
    }

    quint8 media = 0;
    if (k == QLatin1String("play"))
        media = 1;
    else if (k == QLatin1String("pause"))
        media = 2;
    else if (k == QLatin1String("playpause") || k == QLatin1String("play_pause"))
        media = 3;
    else if (k == QLatin1String("next"))
        media = 4;
    else if (k == QLatin1String("prev") || k == QLatin1String("previous"))
        media = 5;
    else if (k == QLatin1String("back"))
        media = 6;
    else if (k == QLatin1String("siri") || k == QLatin1String("voice")
             || k == QLatin1String("voice_command"))
        media = 7;

    if (media != 0) {
        const bool ok = sendHidReport(QStringLiteral("2a2a2a2c"),
                                      buildMediaReport(down ? media : 0));
        if (ok)
            emit log(QStringLiteral("hardKey %1 down=%2").arg(k).arg(int(down)));
        return ok;
    }

    if (k == QLatin1String("phone_accept") || k == QLatin1String("phone_end")
        || k == QLatin1String("phone_reject")) {
        QByteArray report(1, 0);
        if (down) {
            if (k == QLatin1String("phone_accept"))
                report[0] = 0x01;
            else
                report[0] = 0x02;
        }
        const bool ok = sendHidReport(QStringLiteral("2a2a2a2d"), report);
        if (ok)
            emit log(QStringLiteral("hardKey %1 down=%2").arg(k).arg(int(down)));
        return ok;
    }

    emit log(QStringLiteral("hardKey unknown %1").arg(key));
    return false;
}

QByteArray AirPlayServer::buildTouchReport(int x, int y, bool down)
{
    QByteArray report(12, 0);
    report[0] = 0;
    report[1] = down ? 0x01 : 0x00;
    report[2] = char(x & 0xff);
    report[3] = char((x >> 8) & 0xff);
    report[4] = char(y & 0xff);
    report[5] = char((y >> 8) & 0xff);
    report[6] = 1;
    return report;
}

bool AirPlayServer::flushEventOut()
{
    if (!m_eventSock)
        return false;
    while (!m_eventOut.isEmpty()) {
        const qint64 n = m_eventSock->write(m_eventOut);
        if (n <= 0)
            return false;
        m_eventOut.remove(0, int(n));
    }
    return true;
}

bool AirPlayServer::sendEventCommand(const QVariantMap &command)
{
    if (!m_eventSock || !m_eventCipher || !m_eventCipher->isValid())
        return false;
    ++m_eventCseq;
    const QByteArray body = AirPlayBplist::encode(command);
    QByteArray head;
    head.append("POST /command RTSP/1.0\r\n");
    head.append("Content-Type: application/x-apple-binary-plist\r\n");
    head.append("Content-Length: ");
    head.append(QByteArray::number(body.size()));
    head.append("\r\nCSeq: ");
    head.append(QByteArray::number(m_eventCseq));
    head.append("\r\n\r\n");
    const QByteArray packet = m_eventCipher->encrypt(head + body);
    if (packet.isEmpty())
        return false;
    m_eventOut.append(packet);
    return flushEventOut() || m_eventSock->state() == QAbstractSocket::ConnectedState;
}

bool AirPlayServer::sendTouch(double xNorm, double yNorm, bool down)
{
    if (m_displayW <= 0 || m_displayH <= 0)
        return false;
    xNorm = qBound(0.0, xNorm, 1.0);
    yNorm = qBound(0.0, yNorm, 1.0);
    const int xMax = qMax(0, m_displayW - 1);
    const int yMax = qMax(0, m_displayH - 1);
    const int x = qBound(0, int(qRound(xNorm * double(xMax))), xMax);
    const int y = qBound(0, int(qRound(yNorm * double(yMax))), yMax);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!down && !m_touchDown)
        return true;
    const bool moving = down && m_touchDown;
    if (moving) {
        if (x == m_lastTouchPx && y == m_lastTouchPy)
            return true;
        if (now - m_lastTouchSendMs < 16)
            return true;
    }
    m_lastTouchX = xNorm;
    m_lastTouchY = yNorm;
    m_lastTouchMs = now;
    m_lastTouchSendMs = now;
    m_lastTouchPx = x;
    m_lastTouchPy = y;
    QVariantMap cmd;
    cmd.insert(QStringLiteral("type"), QStringLiteral("hidSendReport"));
    cmd.insert(QStringLiteral("uuid"), QStringLiteral("2a2a2a2a"));
    cmd.insert(QStringLiteral("hidReport"), buildTouchReport(x, y, down));
    const bool ok = sendEventCommand(cmd);
    if (ok) {
        m_touchDown = down;
        if (!down || !moving)
            emit log(QStringLiteral("touch %1,%2 down=%3").arg(x).arg(y).arg(int(down)));
    } else {
        emit log(QStringLiteral("touch dropped: event not ready"));
    }
    return ok;
}

QByteArray AirPlayServer::buildInfoPlist() const
{
    const int kW = m_displayW;
    const int kH = m_displayH;
    const int physW = 200;
    const int physH = qMax(1, int(qRound(double(physW) * double(kH) / double(kW))));
    const QString uuid = QStringLiteral("b7e6c5a0-1111-4000-8000-000000000001");

    const int safeX = qBound(0, m_safeLeft, kW - 1);
    const int safeY = qBound(0, m_safeTop, kH - 1);
    const int safeW = qMax(1, kW - m_safeLeft - m_safeRight);
    const int safeH = qMax(1, kH - m_safeTop - m_safeBottom);
    auto makeArea = [&](int dockEdge) {
        return QVariantMap{
            {QStringLiteral("widthPixels"), kW},
            {QStringLiteral("heightPixels"), kH},
            {QStringLiteral("originXPixels"), 0},
            {QStringLiteral("originYPixels"), 0},
            {QStringLiteral("viewAreaStatusBarEdge"), dockEdge},
            {QStringLiteral("safeArea"),
             QVariantMap{{QStringLiteral("widthPixels"), safeW},
                         {QStringLiteral("heightPixels"), safeH},
                         {QStringLiteral("originXPixels"), safeX},
                         {QStringLiteral("originYPixels"), safeY},
                         {QStringLiteral("drawUIOutsideSafeArea"), true}}},
        };
    };
    // DiPlay DRIVER_SIDE: area0=driver(2), area1=bottom(1); initial=driver.
    const QVariantList viewAreas{makeArea(2), makeArea(1)};

    QVariantMap display;
    display.insert(QStringLiteral("uuid"), uuid);
    display.insert(QStringLiteral("type"), 110);
    display.insert(QStringLiteral("maxFPS"), 60);
    display.insert(QStringLiteral("widthPixels"), kW);
    display.insert(QStringLiteral("heightPixels"), kH);
    display.insert(QStringLiteral("widthPhysical"), physW);
    display.insert(QStringLiteral("heightPhysical"), physH);
    display.insert(QStringLiteral("features"), 0x0a);
    display.insert(QStringLiteral("primaryInputDevice"), 1);
    display.insert(QStringLiteral("initialViewArea"), 0);
    display.insert(QStringLiteral("viewAreaTransitionControl"), true);
    display.insert(QStringLiteral("viewAreas"), viewAreas);

    QVariantMap resourceScreen{
        {QStringLiteral("resourceID"), 1},
        {QStringLiteral("transferType"), 1},
        {QStringLiteral("transferPriority"), 100},
        {QStringLiteral("takeConstraint"), 100},
        {QStringLiteral("borrowConstraint"), 100},
        {QStringLiteral("unborrowConstraint"), 100},
    };
    QVariantMap resourceAudio = resourceScreen;
    resourceAudio.insert(QStringLiteral("resourceID"), 2);
    QVariantMap modes;
    modes.insert(QStringLiteral("resources"), QVariantList{resourceScreen, resourceAudio});
    modes.insert(QStringLiteral("appStates"), QVariantList{
        QVariantMap{{QStringLiteral("appStateID"), 2}, {QStringLiteral("state"), false}},
        QVariantMap{{QStringLiteral("appStateID"), 1}, {QStringLiteral("speechMode"), -1}},
        QVariantMap{{QStringLiteral("appStateID"), 3}, {QStringLiteral("state"), false}},
    });

    auto hid = [&](quint32 uid, const QString &name, const QByteArray &desc) {
        return QVariantMap{
            {QStringLiteral("hidProductID"), qlonglong(1)},
            {QStringLiteral("hidVendorID"), qlonglong(2)},
            {QStringLiteral("hidCountryCode"), qlonglong(0)},
            {QStringLiteral("uuid"), QString::number(uid, 16)},
            {QStringLiteral("name"), name},
            {QStringLiteral("displayUUID"), uuid},
            {QStringLiteral("hidDescriptor"), desc},
        };
    };
    const QByteArray touchDesc = touchHidDescriptor(kW, kH);
    const QByteArray knobDesc = QByteArray::fromHex(
        "05010908a1010509090115002501750195018102050c0a23020a2402950281029505810105010901a100"
        "093009311581257f750895028102c009381581257f750895018106c0");
    const QByteArray mediaDesc = QByteArray::fromHex(
        "050c0901a10115002507050c0a00000ab0000ab1000acd000ab5000ab6000a9e020acf00750895018100c0");
    const QByteArray telDesc = QByteArray::fromHex(
        "050b0907a10115002511050b0900092009210926092f09b009b109b209b309b409b509b609b709b809b909ba"
        "09bb0507092a750895018100c0");

    QVariantMap info;
    info.insert(QStringLiteral("sourceVersion"), m_sourceVersion);
    info.insert(QStringLiteral("features"), qlonglong(0x615653aee2LL));
    info.insert(QStringLiteral("statusFlags"), qlonglong(4));
    info.insert(QStringLiteral("model"), m_model);
    info.insert(QStringLiteral("manufacturer"), QStringLiteral("MP157"));
    info.insert(QStringLiteral("deviceID"), m_deviceId);
    info.insert(QStringLiteral("bluetoothIDs"), QVariantList{m_bluetoothMac});
    info.insert(QStringLiteral("name"), m_deviceName);
    info.insert(QStringLiteral("rightHandDrive"), false);
    info.insert(QStringLiteral("keepAliveLowPower"), false);
    info.insert(QStringLiteral("keepAliveSendStatsAsBody"), false);
    info.insert(QStringLiteral("modes"), modes);
    info.insert(QStringLiteral("extendedFeatures"),
                QVariantList{QStringLiteral("vocoderInfo"), QStringLiteral("enhancedRequestCarUI")});
    {
        QByteArray png;
        const QString iconPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath(QStringLiteral("skoda-oem-icon.png"));
        QFile iconFile(iconPath);
        if (iconFile.open(QIODevice::ReadOnly))
            png = iconFile.readAll();
        if (png.isEmpty()) {
            QImage icon(128, 128, QImage::Format_ARGB32);
            icon.fill(QColor(0x2E, 0x6B, 0x2E));
            QBuffer buf(&png);
            buf.open(QIODevice::WriteOnly);
            icon.save(&buf, "PNG");
        }
        info.insert(QStringLiteral("oemIconVisible"), true);
        info.insert(QStringLiteral("oemIconLabel"), QStringLiteral("汽车"));
        info.insert(QStringLiteral("oemIcons"), QVariantList{QVariantMap{
            {QStringLiteral("imageData"), png},
            {QStringLiteral("widthPixels"), 128},
            {QStringLiteral("heightPixels"), 128},
            {QStringLiteral("prerendered"), true},
        }});
    }

    auto latency = [](int type, const QString &audioType = {}) {
        QVariantMap entry{{QStringLiteral("type"), type},
                          {QStringLiteral("inputLatencyMicros"), qlonglong(0)},
                          {QStringLiteral("outputLatencyMicros"), qlonglong(0)}};
        if (!audioType.isEmpty())
            entry.insert(QStringLiteral("audioType"), audioType);
        return entry;
    };
    // Exact DiPlay 48 kHz entertainment offer + type 103 AAC for mainBuffered.
    constexpr int pcm = 0x3fc | 0xc000;
    constexpr int pcmMono = 0x154 | 0x4000;
    constexpr int opus = 0x70000000;
    constexpr int aacLc = 0x800000;
    auto format = [](int type, const QString &audioType, int output, int input = -1) {
        QVariantMap entry{{QStringLiteral("type"), type},
                          {QStringLiteral("audioType"), audioType},
                          {QStringLiteral("audioOutputFormats"), qlonglong(output)}};
        if (input >= 0)
            entry.insert(QStringLiteral("audioInputFormats"), qlonglong(input));
        return entry;
    };
    info.insert(QStringLiteral("audioLatencies"), QVariantList{
        latency(96), latency(96, QStringLiteral("default")), latency(96, QStringLiteral("media")),
        latency(100), latency(100, QStringLiteral("default")), latency(100, QStringLiteral("media")),
        latency(100, QStringLiteral("telephony")), latency(100, QStringLiteral("speechRecognition")),
        latency(100, QStringLiteral("alert")), latency(101), latency(101, QStringLiteral("default")),
        latency(102, QStringLiteral("default")),
    });
    info.insert(QStringLiteral("audioFormats"), QVariantList{
        format(96, QStringLiteral("compatibility"), pcm),
        format(96, QStringLiteral("media"), pcm),
        format(96, QStringLiteral("default"), pcm),
        format(100, QStringLiteral("compatibility"), pcm),
        format(101, QStringLiteral("compatibility"), pcm),
        format(100, QStringLiteral("default"), pcm | opus),
        format(100, QStringLiteral("alert"), pcm | opus),
        format(100, QStringLiteral("media"), pcm),
        format(100, QStringLiteral("telephony"), pcmMono | opus),
        format(100, QStringLiteral("speechRecognition"), pcmMono | opus),
        format(101, QStringLiteral("default"), pcm | opus),
        format(102, QStringLiteral("media"), aacLc),
    });

    info.insert(QStringLiteral("displays"), QVariantList{display});
    info.insert(QStringLiteral("hidDevices"), QVariantList{
        hid(0x2a2a2a2a, QStringLiteral("Touchscreen"), touchDesc),
        hid(0x2a2a2a2b, QStringLiteral("Knob"), knobDesc),
        hid(0x2a2a2a2c, QStringLiteral("Media"), mediaDesc),
        hid(0x2a2a2a2d, QStringLiteral("Telephony"), telDesc),
    });
    const QByteArray encoded = AirPlayBplist::encode(info);
    return encoded;
}

QByteArray AirPlayServer::streamOutputKey(const QVariantMap &stream) const
{
    if (m_sharedSecret.size() != 32)
        return {};
    const QString connectionId = unsignedPlistDecimal(stream.value(QStringLiteral("streamConnectionID")));
    if (connectionId.isEmpty())
        return {};
    return AirPlayCrypto::hkdfSha512(
        m_sharedSecret, QByteArray("DataStream-Salt") + connectionId.toLatin1(),
        QByteArray("DataStream-Output-Encryption-Key"));
}

void AirPlayServer::audioFormatFromBits(quint64 bits, int *sampleRate, int *channels, bool *aac)
{
    *aac = (bits & 0x400000ULL) != 0 || (bits & 0x800000ULL) != 0;
    if (bits & 0x800000ULL) {
        *sampleRate = 48000;
        *channels = 2;
        return;
    }
    if (bits & 0x400000ULL) {
        *sampleRate = 44100;
        *channels = 2;
        return;
    }
    if (bits & 0x8000ULL) {
        *sampleRate = 48000;
        *channels = 2;
        return;
    }
    if (bits & 0x4000ULL) {
        *sampleRate = 48000;
        *channels = 1;
        return;
    }
    if (bits & 0x800ULL) {
        *sampleRate = 44100;
        *channels = 2;
        return;
    }
    if (bits & 0x400ULL) {
        *sampleRate = 44100;
        *channels = 1;
        return;
    }
    *sampleRate = 44100;
    *channels = 2;
}

QVariantMap AirPlayServer::setupAudioStream(const QVariantMap &stream, int type)
{
    const QByteArray key = streamOutputKey(stream);
    if (key.isEmpty()) {
        emit log(QStringLiteral("audio SETUP missing key type=%1").arg(type));
        return {};
    }
    int sampleRate = 44100;
    int channels = 2;
    bool aac = false;
    audioFormatFromBits(quint64(stream.value(QStringLiteral("audioFormat")).toULongLong()), &sampleRate,
                        &channels, &aac);
    const QString audioType = stream.value(QStringLiteral("audioType")).toString();
    emit log(QStringLiteral("audio SETUP type=%1 audioType=%2 rate=%3 ch=%4 aac=%5")
                 .arg(type)
                 .arg(audioType)
                 .arg(sampleRate)
                 .arg(channels)
                 .arg(aac));

    m_audio.reset();
    if (!m_pcm)
        m_pcm = std::make_unique<AirPlayPcmPlayer>(this);
    m_pcm->stop();
    if (!m_pcm->start(sampleRate, channels)) {
        emit log(QStringLiteral("pcm player start failed"));
        return {};
    }
    m_audio = std::make_unique<AirPlayAudioStream>(key, sampleRate, channels, aac, this);
    connect(m_audio.get(), &AirPlayAudioStream::log, this, &AirPlayServer::log);
    connect(m_audio.get(), &AirPlayAudioStream::pcmReady, this, [this](const QByteArray &pcm) {
        if (m_pcm)
            m_pcm->writePcm(pcm);
    });
    if (!m_audio->start()) {
        emit log(QStringLiteral("audio listen failed"));
        m_audio.reset();
        m_pcm->stop();
        return {};
    }
    QVariantMap rsp{
        {QStringLiteral("type"), type},
        {QStringLiteral("dataPort"), qlonglong(m_audio->dataPort())},
        {QStringLiteral("controlPort"), qlonglong(m_audio->controlPort())},
    };
    if (stream.contains(QStringLiteral("streamConnectionID")))
        rsp.insert(QStringLiteral("streamConnectionID"), stream.value(QStringLiteral("streamConnectionID")));
    return rsp;
}

QVariantMap AirPlayServer::setupIapDataStream(const QVariantMap &stream)
{
    static const QString kIapUuid = QStringLiteral("E9459FD0-BCAD-4C45-820F-1E72447EF2F2");
    const QString uuid = stream.value(QStringLiteral("clientTypeUUID")).toString().toUpper();
    if (uuid != kIapUuid) {
        emit log(QStringLiteral("iAP SETUP unknown uuid=%1").arg(uuid));
        return {};
    }
    if (m_sharedSecret.size() != 32) {
        emit log(QStringLiteral("iAP SETUP missing shared secret"));
        return {};
    }
    const QString seed = unsignedPlistDecimal(stream.value(QStringLiteral("seed")));
    if (seed.isEmpty()) {
        emit log(QStringLiteral("iAP SETUP missing seed"));
        return {};
    }
    const QByteArray key = AirPlayCrypto::hkdfSha512(
        m_sharedSecret, QByteArray("DataStream-Salt") + seed.toLatin1(),
        QByteArray("DataStream-Output-Encryption-Key"));
    Q_UNUSED(key);
    if (m_iapServer) {
        for (QTcpSocket *s : std::as_const(m_iapSocks)) {
            if (s)
                s->deleteLater();
        }
        m_iapSocks.clear();
        m_iapServer->close();
        m_iapServer.reset();
    }
    m_iapServer = std::make_unique<QTcpServer>(this);
    if (!m_iapServer->listen(QHostAddress::AnyIPv4, 0)) {
        emit log(QStringLiteral("iAP listen failed"));
        m_iapServer.reset();
        return {};
    }
    connect(m_iapServer.get(), &QTcpServer::newConnection, this, [this] {
        while (m_iapServer && m_iapServer->hasPendingConnections()) {
            QTcpSocket *sock = m_iapServer->nextPendingConnection();
            if (!sock)
                continue;
            sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            m_iapSocks.append(sock);
            emit log(QStringLiteral("iAP tunnel connected from %1").arg(sock->peerAddress().toString()));
            connect(sock, &QTcpSocket::readyRead, sock, [sock] { sock->readAll(); });
            connect(sock, &QTcpSocket::disconnected, this, [this, sock] {
                m_iapSocks.removeAll(sock);
                sock->deleteLater();
                emit log(QStringLiteral("iAP tunnel disconnected"));
            });
        }
    });
    const quint16 port = m_iapServer->serverPort();
    emit log(QStringLiteral("iAP SETUP seed=%1 dataPort=%2").arg(seed).arg(port));
    QVariantMap rsp{
        {QStringLiteral("type"), 130},
        {QStringLiteral("streamID"), qlonglong(1)},
        {QStringLiteral("dataPort"), qlonglong(port)},
    };
    if (stream.contains(QStringLiteral("streamConnectionID")))
        rsp.insert(QStringLiteral("streamConnectionID"), stream.value(QStringLiteral("streamConnectionID")));
    return rsp;
}

QVariantMap AirPlayServer::setupBufferedAudio(const QVariantMap &stream)
{
    const QByteArray key = stream.value(QStringLiteral("shk")).toByteArray();
    if (key.size() != 32) {
        emit log(QStringLiteral("buffered SETUP missing shk"));
        return {};
    }
    int sampleRate = 48000;
    int channels = 2;
    bool aac = true;
    audioFormatFromBits(quint64(stream.value(QStringLiteral("audioFormat")).toULongLong()), &sampleRate,
                        &channels, &aac);
    if (!aac)
        sampleRate = 48000;
    emit log(QStringLiteral("audio SETUP type=103 client=%1 rate=%2")
                 .arg(stream.value(QStringLiteral("clientID")).toString())
                 .arg(sampleRate));
    m_buffered.reset();
    if (!m_pcm)
        m_pcm = std::make_unique<AirPlayPcmPlayer>(this);
    m_pcm->stop();
    if (!m_pcm->start(sampleRate, channels)) {
        emit log(QStringLiteral("pcm player start failed"));
        return {};
    }
    m_buffered = std::make_unique<AirPlayBufferedAudio>(key, sampleRate, channels, this);
    connect(m_buffered.get(), &AirPlayBufferedAudio::log, this, &AirPlayServer::log);
    connect(m_buffered.get(), &AirPlayBufferedAudio::pcmReady, this, [this](const QByteArray &pcm) {
        if (m_pcm)
            m_pcm->writePcm(pcm);
    });
    if (!m_buffered->start()) {
        emit log(QStringLiteral("buffered listen failed"));
        m_buffered.reset();
        m_pcm->stop();
        return {};
    }
    return QVariantMap{
        {QStringLiteral("type"), 103},
        {QStringLiteral("dataPort"), qlonglong(m_buffered->dataPort())},
        {QStringLiteral("audioBufferSize"), qlonglong(AirPlayBufferedAudio::kBufferBytes)},
    };
}

QByteArray AirPlayServer::handleBufferedControl(const QByteArray &method, const QByteArray &body)
{
    if (!m_buffered)
        return {};
    QVariantMap dict;
    if (!body.isEmpty()) {
        const QVariant decoded = AirPlayBplist::decode(body);
        if (decoded.canConvert<QVariantMap>())
            dict = decoded.toMap();
    }
    if (method == "GETANCHOR") {
        const QVariantMap a = m_buffered->anchor();
        if (a.isEmpty())
            return {};
        return AirPlayBplist::encode(a);
    }
    if (method == "FLUSHBUFFERED") {
        m_buffered->flush(dict.value(QStringLiteral("flushUntilTS")).toLongLong());
        return {};
    }
    if (method == "SETRATE" || method == "SETRATEANCHORTIME") {
        int rate = 0;
        const QVariant rateVar = dict.value(QStringLiteral("rate"));
        if (rateVar.typeId() == QMetaType::Double || rateVar.typeId() == QMetaType::Float)
            rate = rateVar.toDouble() >= 0.5 ? 1 : 0;
        else
            rate = rateVar.toInt() > 0 ? 1 : 0;
        qint64 rtp = -1;
        if (dict.contains(QStringLiteral("rtpTime")))
            rtp = dict.value(QStringLiteral("rtpTime")).toLongLong();
        const QVariantMap a = m_buffered->setRate(rtp, rate);
        emit log(QStringLiteral("%1 rate=%2 rtp=%3")
                     .arg(QString::fromLatin1(method))
                     .arg(rate)
                     .arg(rtp));
        return AirPlayBplist::encode(a);
    }
    return {};
}

QByteArray AirPlayServer::handleSetup(ClientState *client, const QByteArray &body)
{
    Q_UNUSED(client);
    const QVariant decoded = AirPlayBplist::decode(body);
    if (!decoded.isValid() || !decoded.canConvert<QVariantMap>())
        return {};
    const QVariantMap dict = decoded.toMap();
    if (dict.contains(QStringLiteral("streams"))) {
        QVariantList responseStreams;
        const QVariantList streams = dict.value(QStringLiteral("streams")).toList();
        for (const QVariant &entry : streams) {
            const QVariantMap stream = entry.toMap();
            const int type = stream.value(QStringLiteral("type")).toInt();
            {
                QFile dump(QStringLiteral("airplay-stream-setup.bplist"));
                if (dump.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    dump.write(body);
            }
            emit log(QStringLiteral("SETUP stream type=%1 keys=%2")
                         .arg(type)
                         .arg(QStringList(stream.keys()).join(QLatin1Char(','))));
            if (type == 110 || type == 111) {
                const QString connId = unsignedPlistDecimal(stream.value(QStringLiteral("streamConnectionID")));
                const QByteArray key = streamOutputKey(stream);
                if (key.isEmpty()) {
                    emit log(QStringLiteral("screen SETUP missing key type=%1 conn=%2").arg(type).arg(connId));
                    continue;
                }
                emit log(QStringLiteral("screen key type=%1 conn=%2 key=%3")
                             .arg(type)
                             .arg(connId)
                             .arg(QString::fromLatin1(key.toHex().left(16))));
                m_screen = std::make_unique<AirPlayScreenStream>(key, this);
                connect(m_screen.get(), &AirPlayScreenStream::config, this, &AirPlayServer::onScreenConfig);
                connect(m_screen.get(), &AirPlayScreenStream::frame, this, &AirPlayServer::onScreenFrame);
                connect(m_screen.get(), &AirPlayScreenStream::log, this, &AirPlayServer::log);
                connect(m_screen.get(), &AirPlayScreenStream::closed, this, [this](const QString &why) {
                    emit log(QStringLiteral("screen closed: %1").arg(why));
                });
                const quint16 dataPort = m_screen->start();
                if (dataPort == 0)
                    continue;
                emit log(QStringLiteral("screen stream type=%1 dataPort=%2").arg(type).arg(dataPort));
                responseStreams.append(QVariantMap{
                    {QStringLiteral("type"), type},
                    {QStringLiteral("dataPort"), qlonglong(dataPort)},
                });
            } else if (type == 96 || type == 100 || type == 101 || type == 102) {
                const QVariantMap audioRsp = setupAudioStream(stream, type);
                if (!audioRsp.isEmpty())
                    responseStreams.append(audioRsp);
                else
                    emit log(QStringLiteral("audio SETUP failed type=%1").arg(type));
            } else if (type == 103) {
                const QVariantMap bufferedRsp = setupBufferedAudio(stream);
                if (!bufferedRsp.isEmpty())
                    responseStreams.append(bufferedRsp);
                else
                    emit log(QStringLiteral("audio SETUP failed type=103"));
            } else if (type == 130) {
                const QVariantMap iapRsp = setupIapDataStream(stream);
                if (!iapRsp.isEmpty())
                    responseStreams.append(iapRsp);
                else
                    emit log(QStringLiteral("iAP SETUP failed keys=%1")
                                 .arg(QStringList(stream.keys()).join(QLatin1Char(','))));
            } else {
                emit log(QStringLiteral("SETUP ignore stream type=%1 keys=%2")
                             .arg(type)
                             .arg(QStringList(stream.keys()).join(QLatin1Char(','))));
            }
        }
        return AirPlayBplist::encode({{QStringLiteral("streams"), responseStreams}});
    }

    emit log(QStringLiteral("session SETUP keys=%1").arg(QStringList(dict.keys()).join(QLatin1Char(','))));
    {
        QFile dump(QStringLiteral("airplay-setup.bplist"));
        if (dump.open(QIODevice::WriteOnly | QIODevice::Truncate))
            dump.write(body);
    }
    QStringList proposed;
    const QVariant featuresVar = dict.value(QStringLiteral("features"));
    if (featuresVar.canConvert<QVariantList>()) {
        for (const QVariant &f : featuresVar.toList())
            proposed.append(f.toString());
        emit log(QStringLiteral("session SETUP features=%1").arg(proposed.join(QLatin1Char(','))));
    }
    const int peerTimingPort = dict.value(QStringLiteral("timingPort")).toInt();
    if (client && client->socket && peerTimingPort > 0)
        startTimingPeer(client->socket->peerAddress(), quint16(peerTimingPort));

    QVariantList enabled{QStringLiteral("iAPChannel"), QStringLiteral("viewAreas")};
    QVariantMap response;
    response.insert(QStringLiteral("timingPort"), qlonglong(m_timingPort));
    response.insert(QStringLiteral("eventPort"), qlonglong(m_eventPort));
    const QVariant kalp = dict.value(QStringLiteral("keepAliveLowPower"));
    if (kalp.toBool() || kalp.toLongLong() == 1) {
        m_keepAlivePort = ensureKeepAliveUdp();
        response.insert(QStringLiteral("keepAlivePort"), qlonglong(m_keepAlivePort));
    }
    response.insert(QStringLiteral("enabledFeatures"), enabled);
    QStringList enabledNames;
    for (const QVariant &v : enabled)
        enabledNames.append(v.toString());
    emit log(QStringLiteral("session SETUP timing=%1 event=%2 peerTiming=%3 keepAlive=%4 enabled=%5")
                 .arg(m_timingPort)
                 .arg(m_eventPort)
                 .arg(peerTimingPort)
                 .arg(m_keepAlivePort)
                 .arg(enabledNames.join(QLatin1Char(','))));
    return AirPlayBplist::encode(response);
}

void AirPlayServer::handleRequest(ClientState *client, const QByteArray &method, const QString &path,
                                  const QString &protocol, const QHash<QString, QString> &headers,
                                  const QByteArray &body)
{
    const QString cseq = headers.value(QStringLiteral("cseq"));
    const QString pathLower = path.toLower();
    const QHostAddress peer = client->socket ? client->socket->peerAddress() : QHostAddress();
    const bool carplayPath = method == "SETUP" || method == "RECORD" || method == "TEARDOWN"
        || pathLower.contains(QStringLiteral("pair-")) || pathLower.contains(QStringLiteral("auth-setup"))
        || pathLower.endsWith(QStringLiteral("/info"));
    if (!client->announced && !isLocalPeer(peer) && carplayPath) {
        client->announced = true;
        emit clientConnected(peer.toString());
        emit log(QStringLiteral("phone session %1 %2 %3")
                     .arg(peer.toString(), QString::fromLatin1(method), path));
    }
    emit log(QStringLiteral("%1 %2 (%3 bytes)").arg(QString::fromLatin1(method), path).arg(body.size()));

    if (method == "SETUP") {
        const QByteArray responseBody = handleSetup(client, body);
        if (responseBody.isEmpty()) {
            sendResponse(client, protocol, 400, {}, {}, cseq);
            return;
        }
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/x-apple-binary-plist")}},
                     responseBody, cseq);
        return;
    }
    if (method == "RECORD") {
        sendResponse(client, protocol, 200, {}, {}, cseq);
        emit sessionActive();
        QTimer::singleShot(50, this, [this] {
            if (!m_eventSock || !m_eventCipher)
                return;
            const bool ok = sendEventCommand({
                {QStringLiteral("type"), QStringLiteral("requestUI")},
            });
            emit log(QStringLiteral("event requestUI %1").arg(ok ? QStringLiteral("ok") : QStringLiteral("fail")));
        });
        return;
    }
    if (method == "SETRATE" || method == "SETRATEANCHORTIME" || method == "GETANCHOR"
        || method == "FLUSHBUFFERED") {
        const QByteArray rsp = handleBufferedControl(method, body);
        if (rsp.isEmpty() && method != "FLUSHBUFFERED" && method != "GETANCHOR") {
            sendResponse(client, protocol, 200, {}, {}, cseq);
            return;
        }
        if (rsp.isEmpty()) {
            sendResponse(client, protocol, 200, {}, {}, cseq);
            return;
        }
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/x-apple-binary-plist")}},
                     rsp, cseq);
        return;
    }
    if (method == "TEARDOWN") {
        emit log(QStringLiteral("TEARDOWN body=%1").arg(QString::fromLatin1(body.toHex())));
        m_screen.reset();
        m_audio.reset();
        m_buffered.reset();
        if (m_pcm)
            m_pcm->stop();
        for (QTcpSocket *s : std::as_const(m_iapSocks)) {
            if (s)
                s->deleteLater();
        }
        m_iapSocks.clear();
        if (m_iapServer) {
            m_iapServer->close();
            m_iapServer.reset();
        }
        m_timingTimer.stop();
        sendResponse(client, protocol, 200, {}, {}, cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/pair-setup"))) {
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/pairing+tlv8")}},
                     client->pairSetup->handle(body), cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/pair-verify"))) {
        const QByteArray rsp = client->pairVerify->handle(body);
        if (client->pairVerify->verified)
            client->enableCipherAfterResponse = true;
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/pairing+tlv8")}},
                     rsp, cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/auth-setup"))) {
        if (!m_mfi) {
            sendResponse(client, protocol, 400, {}, {}, cseq);
            return;
        }
        const QByteArray rsp = AirPlayAuthSetup::handle(body, *m_mfi);
        if (rsp.isEmpty()) {
            sendResponse(client, protocol, 400, {}, {}, cseq);
            return;
        }
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/octet-stream")}},
                     rsp, cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/info"))) {
        const QByteArray infoBody = buildInfoPlist();
        emit log(QStringLiteral("/info bytes=%1").arg(infoBody.size()));
        {
            QFile dump(QStringLiteral("airplay-info.bplist"));
            if (dump.open(QIODevice::WriteOnly | QIODevice::Truncate))
                dump.write(infoBody);
        }
        sendResponse(client, protocol, 200,
                     {{QStringLiteral("Content-Type"), QStringLiteral("application/x-apple-binary-plist")}},
                     infoBody, cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/command"))) {
        const QVariant decoded = AirPlayBplist::decode(body);
        if (decoded.canConvert<QVariantMap>()) {
            const QVariantMap cmd = decoded.toMap();
            const QString type = cmd.value(QStringLiteral("type")).toString();
            const QVariantMap params = cmd.value(QStringLiteral("params")).toMap();
            emit log(QStringLiteral("command type=%1 params=%2")
                         .arg(type, QStringList(params.keys()).join(QLatin1Char(','))));
            if (type == QLatin1String("modesChanged")) {
                QFile dump(QStringLiteral("airplay-modes.bplist"));
                if (dump.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    dump.write(body);
            }
            handleIncomingCommand(cmd);
        } else {
            emit log(QStringLiteral("command bodyHex=%1").arg(QString::fromLatin1(body.toHex())));
        }
        sendResponse(client, protocol, 200, {}, {}, cseq);
        return;
    }
    if (pathLower.endsWith(QStringLiteral("/feedback"))) {
        sendResponse(client, protocol, 200, {}, {}, cseq);
        return;
    }
    sendResponse(client, protocol, 200, {}, {}, cseq);
}

void AirPlayServer::onEventNewConnection()
{
    while (auto *sock = m_eventServer.nextPendingConnection()) {
        if (m_eventSock) {
            sock->abort();
            sock->deleteLater();
            continue;
        }
        m_eventSock = sock;
        m_eventSock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        if (m_sharedSecret.size() != 32) {
            emit log(QStringLiteral("event rejected: no shared secret"));
            m_eventSock->abort();
            m_eventSock->deleteLater();
            m_eventSock = nullptr;
            continue;
        }
        const QByteArray writeKey = AirPlayCrypto::hkdfSha512(
            m_sharedSecret, QByteArray("Events-Salt"), QByteArray("Events-Write-Encryption-Key"));
        const QByteArray readKey = AirPlayCrypto::hkdfSha512(
            m_sharedSecret, QByteArray("Events-Salt"), QByteArray("Events-Read-Encryption-Key"));
        m_eventCipher = std::make_unique<AirPlayControlCipher>(readKey, writeKey);
        m_eventBuf.clear();
        m_eventPlain.clear();
        m_eventOut.clear();
        m_touchDown = false;
        m_lastTouchSendMs = 0;
        m_lastTouchPx = -1;
        m_lastTouchPy = -1;
        connect(m_eventSock, &QTcpSocket::readyRead, this, &AirPlayServer::onEventReadyRead);
        connect(m_eventSock, &QTcpSocket::bytesWritten, this, [this](qint64) { flushEventOut(); });
        connect(m_eventSock, &QTcpSocket::disconnected, this, &AirPlayServer::onEventDisconnected);
        emit log(QStringLiteral("event connected from %1").arg(m_eventSock->peerAddress().toString()));
    }
}

void AirPlayServer::onEventReadyRead()
{
    if (!m_eventSock || !m_eventCipher)
        return;
    m_eventBuf.append(m_eventSock->readAll());
    QByteArray rest;
    const QByteArray plain = m_eventCipher->decrypt(m_eventBuf, &rest);
    m_eventBuf = rest;
    if (plain.isNull())
        return;
    m_eventPlain.append(plain);
    const QByteArray headerEnd("\r\n\r\n");
    while (true) {
        const int headerEndIndex = m_eventPlain.indexOf(headerEnd);
        if (headerEndIndex < 0)
            break;
        const QByteArray headerText = m_eventPlain.left(headerEndIndex);
        const QList<QByteArray> lines = headerText.split('\n');
        QByteArray requestLine = lines.value(0);
        if (requestLine.endsWith('\r'))
            requestLine.chop(1);
        QHash<QString, QString> headers;
        for (int i = 1; i < lines.size(); ++i) {
            QByteArray line = lines[i];
            if (line.endsWith('\r'))
                line.chop(1);
            const int sep = line.indexOf(':');
            if (sep < 0)
                continue;
            headers.insert(QString::fromLatin1(line.left(sep)).trimmed().toLower(),
                           QString::fromLatin1(line.mid(sep + 1)).trimmed());
        }
        const int contentLength = headers.value(QStringLiteral("content-length")).toInt();
        const int bodyStart = headerEndIndex + 4;
        if (m_eventPlain.size() < bodyStart + contentLength)
            break;
        const QByteArray reqBody = m_eventPlain.mid(bodyStart, contentLength);
        m_eventPlain.remove(0, bodyStart + contentLength);

        if (requestLine.startsWith("RTSP/") || requestLine.startsWith("HTTP/"))
            continue;

        const QList<QByteArray> parts = requestLine.split(' ');
        const QByteArray method = parts.value(0);
        const QString path = QString::fromLatin1(parts.value(1)).toLower();
        if (method == "POST" && path.endsWith(QLatin1String("/command")) && !reqBody.isEmpty()) {
            const QVariant decoded = AirPlayBplist::decode(reqBody);
            if (decoded.canConvert<QVariantMap>())
                handleIncomingCommand(decoded.toMap());
        }
        const QString protocol = QString::fromLatin1(parts.value(2, "RTSP/1.0"));
        const QString cseq = headers.value(QStringLiteral("cseq"));
        QByteArray rsp;
        rsp.append(protocol.toLatin1());
        rsp.append(" 200 OK\r\n");
        if (!cseq.isEmpty()) {
            rsp.append("CSeq: ");
            rsp.append(cseq.toLatin1());
            rsp.append("\r\n");
        }
        rsp.append("Content-Length: 0\r\n\r\n");
        m_eventOut.append(m_eventCipher->encrypt(rsp));
        flushEventOut();
    }
}

void AirPlayServer::onEventDisconnected()
{
    if (m_eventSock) {
        m_eventSock->deleteLater();
        m_eventSock = nullptr;
    }
    m_eventCipher.reset();
    m_eventBuf.clear();
    m_eventPlain.clear();
    m_eventOut.clear();
    m_touchDown = false;
    emit log(QStringLiteral("event disconnected"));
}

void AirPlayServer::onScreenConfig(const QByteArray &avcC)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    auto *decoder = static_cast<AirPlayH264Decoder *>(m_mfDecoder);
    if (!decoder) {
        decoder = new AirPlayH264Decoder;
        m_mfDecoder = decoder;
    }
    {
        QFile dump(QStringLiteral("airplay-avcc.bin"));
        if (dump.open(QIODevice::WriteOnly | QIODevice::Truncate))
            dump.write(avcC);
    }
    if (!decoder->configure(avcC)) {
        emit log(QStringLiteral("h264 configure failed %1").arg(decoder->lastError()));
    } else {
        const QString err = decoder->lastError();
        if (!err.startsWith(QLatin1String("unchanged"))) {
            QMutexLocker lock(&m_decodeMutex);
            m_decodeQueue.clear();
            m_waitIdr.store(true);
        }
        emit log(QStringLiteral("h264 configured %1 %2").arg(avcC.size()).arg(err));
    }
#else
    Q_UNUSED(avcC);
#endif
}

void AirPlayServer::onDecodedFrame(const QImage &img, const QString &err)
{
    static int decoded = 0;
    static int dropped = 0;
    if (!img.isNull()) {
        ++decoded;
        pushVideoFrame(img);
        if (decoded <= 3 || (decoded % 120) == 0)
            emit log(QStringLiteral("h264 decoded #%1 %2x%3").arg(decoded).arg(img.width()).arg(img.height()));
    } else if (!err.startsWith(QLatin1String("need-more"))) {
        ++dropped;
        if (dropped <= 8 || (dropped % 120) == 0)
            emit log(QStringLiteral("h264 drop #%1 err=%2").arg(dropped).arg(err));
    }
}

static bool annexBIsIdr(const QByteArray &annexB)
{
    for (int i = 0; i + 4 < annexB.size(); ++i) {
        if (annexB[i] == 0 && annexB[i + 1] == 0 && annexB[i + 2] == 0 && annexB[i + 3] == 1) {
            if ((quint8(annexB[i + 4]) & 0x1f) == 5)
                return true;
            i += 3;
        } else if (i + 3 < annexB.size() && annexB[i] == 0 && annexB[i + 1] == 0 && annexB[i + 2] == 1) {
            if ((quint8(annexB[i + 3]) & 0x1f) == 5)
                return true;
            i += 2;
        }
    }
    return false;
}

void AirPlayServer::drainDecodeQueue()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (m_decodeBusy.exchange(true))
        return;
    (void)QtConcurrent::run([this]() {
        auto *decoder = static_cast<AirPlayH264Decoder *>(m_mfDecoder);
        while (decoder) {
            QByteArray frame;
            bool needFlush = false;
            {
                QMutexLocker lock(&m_decodeMutex);
                while (!m_decodeQueue.isEmpty()) {
                    const QByteArray next = m_decodeQueue.dequeue();
                    if (m_waitIdr.load()) {
                        if (!annexBIsIdr(next))
                            continue;
                        m_waitIdr.store(false);
                        needFlush = true;
                        m_decodeQueue.clear();
                    }
                    frame = next;
                    break;
                }
                if (frame.isEmpty()) {
                    m_decodeBusy.store(false);
                    return;
                }
            }
            if (needFlush)
                decoder->flush();
            const QImage img = decoder->decode(frame);
            const QString err = decoder->lastError();
            QMetaObject::invokeMethod(this, "onDecodedFrame", Qt::QueuedConnection, Q_ARG(QImage, img),
                                      Q_ARG(QString, err));
        }
        m_decodeBusy.store(false);
    });
#endif
}

void AirPlayServer::onScreenFrame(const QByteArray &annexB)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (!m_mfDecoder || annexB.isEmpty())
        return;
    bool needKey = false;
    {
        QMutexLocker lock(&m_decodeMutex);
        if (m_decodeQueue.size() >= 8) {
            m_decodeQueue.clear();
            m_waitIdr.store(true);
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastForceKeyMs >= 1000) {
                m_lastForceKeyMs = now;
                needKey = true;
            }
        }
        m_decodeQueue.enqueue(annexB);
    }
    if (needKey) {
        emit log(QStringLiteral("decode backlog: forceKeyFrame"));
        sendEventCommand({{QStringLiteral("type"), QStringLiteral("forceKeyFrame")}});
    }
    drainDecodeQueue();
#else
    Q_UNUSED(annexB);
#endif
}

void AirPlayServer::pushVideoFrame(const QImage &image)
{
    m_videoFrame = image;
    emit videoFrameChanged();
}
