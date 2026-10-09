#include "LocalMfiAuth.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <wincrypt.h>
#ifndef NCRYPT_PKCS8_PRIVATEKEY_BLOB
#define NCRYPT_PKCS8_PRIVATEKEY_BLOB L"PKCS8_PRIVATEKEY"
#endif
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "crypt32.lib")
#endif

namespace {

constexpr int kMaxFileBytes = 16 * 1024;

QByteArray readBounded(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法打开 %1").arg(path);
        return {};
    }
    const QByteArray data = file.read(kMaxFileBytes + 1);
    if (data.isEmpty() || data.size() > kMaxFileBytes) {
        *error = QStringLiteral("身份文件无效: %1").arg(path);
        return {};
    }
    return data;
}

#ifdef Q_OS_WIN

struct NCryptKey {
    NCRYPT_PROV_HANDLE provider = 0;
    NCRYPT_KEY_HANDLE key = 0;
    ~NCryptKey()
    {
        if (key)
            NCryptFreeObject(key);
        if (provider)
            NCryptFreeObject(provider);
    }
};

bool importPkcs8(const QByteArray &pk8, NCryptKey *out, QString *error)
{
    SECURITY_STATUS st = NCryptOpenStorageProvider(&out->provider, MS_KEY_STORAGE_PROVIDER, 0);
    if (st != ERROR_SUCCESS) {
        *error = QStringLiteral("NCryptOpenStorageProvider 失败");
        return false;
    }
    st = NCryptImportKey(out->provider, 0, NCRYPT_PKCS8_PRIVATEKEY_BLOB, nullptr,
                         &out->key, reinterpret_cast<PBYTE>(const_cast<char *>(pk8.constData())),
                         static_cast<DWORD>(pk8.size()), NCRYPT_SILENT_FLAG);
    if (st != ERROR_SUCCESS) {
        *error = QStringLiteral("导入 identity.pk8 失败 (0x%1)").arg(quint32(st), 0, 16);
        return false;
    }
    return true;
}

QByteArray signDigest(const QByteArray &pk8, const QByteArray &challenge, QString *error)
{
    if (challenge.size() != 32) {
        *error = QStringLiteral("MFi v3 挑战必须为 32 字节");
        return {};
    }
    NCryptKey imported;
    if (!importPkcs8(pk8, &imported, error))
        return {};
    DWORD sigLen = 0;
    SECURITY_STATUS st = NCryptSignHash(imported.key, nullptr,
                                        reinterpret_cast<PBYTE>(const_cast<char *>(challenge.constData())),
                                        32, nullptr, 0, &sigLen, 0);
    if (st != ERROR_SUCCESS || sigLen == 0) {
        *error = QStringLiteral("NCryptSignHash 尺寸查询失败");
        return {};
    }
    QByteArray sig(int(sigLen), Qt::Uninitialized);
    st = NCryptSignHash(imported.key, nullptr,
                        reinterpret_cast<PBYTE>(const_cast<char *>(challenge.constData())),
                        32, reinterpret_cast<PBYTE>(sig.data()), sigLen, &sigLen, 0);
    if (st != ERROR_SUCCESS) {
        *error = QStringLiteral("NCryptSignHash 失败 (0x%1)").arg(quint32(st), 0, 16);
        return {};
    }
    sig.resize(int(sigLen));
    if (sig.size() != 64) {
        *error = QStringLiteral("ECDSA 签名长度异常: %1").arg(sig.size());
        return {};
    }
    return sig;
}

bool verifyDigest(const QByteArray &p7b, const QByteArray &challenge, const QByteArray &rawSig, QString *error)
{
    CRYPT_DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(p7b.size());
    blob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(p7b.constData()));
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_PKCS7, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                     0, 0, &blob);
    if (!store) {
        *error = QStringLiteral("解析 certificate.p7b 失败");
        return false;
    }
    PCCERT_CONTEXT cert = CertEnumCertificatesInStore(store, nullptr);
    if (!cert) {
        CertCloseStore(store, 0);
        *error = QStringLiteral("certificate.p7b 中无证书");
        return false;
    }
    BCRYPT_KEY_HANDLE pub = nullptr;
    if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &cert->pCertInfo->SubjectPublicKeyInfo,
                                     0, nullptr, &pub)) {
        CertFreeCertificateContext(cert);
        CertCloseStore(store, 0);
        *error = QStringLiteral("导入证书公钥失败");
        return false;
    }
    BCRYPT_PKCS1_PADDING_INFO unused{};
    Q_UNUSED(unused);
    NTSTATUS nts = BCryptVerifySignature(pub, nullptr,
                                         reinterpret_cast<PUCHAR>(const_cast<char *>(challenge.constData())),
                                         32,
                                         reinterpret_cast<PUCHAR>(const_cast<char *>(rawSig.constData())),
                                         static_cast<ULONG>(rawSig.size()), 0);
    BCryptDestroyKey(pub);
    CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);
    if (nts != 0) {
        *error = QStringLiteral("私钥与证书不匹配");
        return false;
    }
    return true;
}

#else

QByteArray signDigest(const QByteArray &, const QByteArray &, QString *error)
{
    *error = QStringLiteral("当前平台尚未接入 OpenSSL MFi 签名");
    return {};
}

bool verifyDigest(const QByteArray &, const QByteArray &, const QByteArray &, QString *error)
{
    *error = QStringLiteral("当前平台尚未接入 OpenSSL MFi 验签");
    return false;
}

#endif

} // namespace

QString LocalMfiAuth::defaultDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("offline-mfi"));
}

bool LocalMfiAuth::load(const QString &directory)
{
    m_ready = false;
    m_pk8.clear();
    m_certificate.clear();
    m_error.clear();

    QDir dir(directory);
    if (!dir.exists()) {
        m_error = QStringLiteral("缺少 offline-mfi 目录: %1").arg(directory);
        return false;
    }

    m_pk8 = readBounded(dir.filePath(QStringLiteral("identity.pk8")), &m_error);
    if (m_pk8.isEmpty())
        return false;
    m_certificate = readBounded(dir.filePath(QStringLiteral("certificate.p7b")), &m_error);
    if (m_certificate.isEmpty())
        return false;

    if (!verifyKeyMatchesCert())
        return false;

    m_ready = true;
    return true;
}

bool LocalMfiAuth::isReady() const
{
    return m_ready;
}

QString LocalMfiAuth::errorString() const
{
    return m_error;
}

int LocalMfiAuth::protocolMajor() const
{
    return 3;
}

QByteArray LocalMfiAuth::certificate() const
{
    return m_certificate;
}

QByteArray LocalMfiAuth::signChallenge(const QByteArray &challenge) const
{
    if (!m_ready)
        return {};
    QString error;
    const QByteArray sig = signDigest(m_pk8, challenge, &error);
    if (sig.isEmpty())
        const_cast<LocalMfiAuth *>(this)->m_error = error;
    return sig;
}

bool LocalMfiAuth::verifyKeyMatchesCert() const
{
    QByteArray challenge(32, Qt::Uninitialized);
    for (int i = 0; i < challenge.size(); ++i)
        challenge[i] = char(QRandomGenerator::global()->generate() & 0xff);
    QString error;
    const QByteArray sig = signDigest(m_pk8, challenge, &error);
    if (sig.isEmpty()) {
        const_cast<LocalMfiAuth *>(this)->m_error = error;
        return false;
    }
    if (!verifyDigest(m_certificate, challenge, sig, &error)) {
        const_cast<LocalMfiAuth *>(this)->m_error = error;
        return false;
    }
    return true;
}
