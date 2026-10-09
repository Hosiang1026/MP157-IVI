#include "AirPlayCrypto.hpp"

#include "third_party/monocypher.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <cstring>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace AirPlayCrypto {
namespace {

struct Sha512Ctx {
    uint64_t state[8];
    uint64_t bitlen;
    uint8_t data[128];
    size_t datalen;
};

constexpr uint64_t rotr64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

void sha512Transform(Sha512Ctx *ctx, const uint8_t data[128])
{
    static const uint64_t K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
        0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};
    uint64_t m[80];
    for (int i = 0; i < 16; ++i) {
        m[i] = (uint64_t(data[i * 8]) << 56) | (uint64_t(data[i * 8 + 1]) << 48) |
               (uint64_t(data[i * 8 + 2]) << 40) | (uint64_t(data[i * 8 + 3]) << 32) |
               (uint64_t(data[i * 8 + 4]) << 24) | (uint64_t(data[i * 8 + 5]) << 16) |
               (uint64_t(data[i * 8 + 6]) << 8) | uint64_t(data[i * 8 + 7]);
    }
    for (int i = 16; i < 80; ++i) {
        const uint64_t s0 = rotr64(m[i - 15], 1) ^ rotr64(m[i - 15], 8) ^ (m[i - 15] >> 7);
        const uint64_t s1 = rotr64(m[i - 2], 19) ^ rotr64(m[i - 2], 61) ^ (m[i - 2] >> 6);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }
    uint64_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint64_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    for (int i = 0; i < 80; ++i) {
        const uint64_t S1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        const uint64_t ch = (e & f) ^ ((~e) & g);
        const uint64_t t1 = h + S1 + ch + K[i] + m[i];
        const uint64_t S0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        const uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint64_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha512InitRaw(Sha512Ctx *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
}

void sha512UpdateRaw(Sha512Ctx *ctx, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 128) {
            sha512Transform(ctx, ctx->data);
            ctx->bitlen += 1024;
            ctx->datalen = 0;
        }
    }
}

void sha512FinalRaw(Sha512Ctx *ctx, uint8_t hash[64])
{
    size_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    if (i > 112) {
        while (i < 128)
            ctx->data[i++] = 0;
        sha512Transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 112)
        ctx->data[i++] = 0;
    ctx->bitlen += uint64_t(ctx->datalen) * 8;
    for (int j = 0; j < 8; ++j)
        ctx->data[127 - j] = uint8_t((ctx->bitlen >> (8 * j)) & 0xff);
    for (int j = 0; j < 8; ++j)
        ctx->data[119 - j] = 0;
    sha512Transform(ctx, ctx->data);
    for (int j = 0; j < 8; ++j) {
        hash[j * 8 + 0] = uint8_t((ctx->state[j] >> 56) & 0xff);
        hash[j * 8 + 1] = uint8_t((ctx->state[j] >> 48) & 0xff);
        hash[j * 8 + 2] = uint8_t((ctx->state[j] >> 40) & 0xff);
        hash[j * 8 + 3] = uint8_t((ctx->state[j] >> 32) & 0xff);
        hash[j * 8 + 4] = uint8_t((ctx->state[j] >> 24) & 0xff);
        hash[j * 8 + 5] = uint8_t((ctx->state[j] >> 16) & 0xff);
        hash[j * 8 + 6] = uint8_t((ctx->state[j] >> 8) & 0xff);
        hash[j * 8 + 7] = uint8_t(ctx->state[j] & 0xff);
    }
}

void sha512HashFn(uint8_t hash[64], const uint8_t *message, size_t message_size)
{
    Sha512Ctx ctx;
    sha512InitRaw(&ctx);
    sha512UpdateRaw(&ctx, message, message_size);
    sha512FinalRaw(&ctx, hash);
}

struct Ed25519Sha512SignCtx {
    crypto_sign_ctx_abstract abs;
    Sha512Ctx hash;
};

void edShaInit(void *ctx) { sha512InitRaw(&static_cast<Ed25519Sha512SignCtx *>(ctx)->hash); }
void edShaUpdate(void *ctx, const uint8_t *m, size_t s)
{
    sha512UpdateRaw(&static_cast<Ed25519Sha512SignCtx *>(ctx)->hash, m, s);
}
void edShaFinal(void *ctx, uint8_t *h)
{
    sha512FinalRaw(&static_cast<Ed25519Sha512SignCtx *>(ctx)->hash, h);
}

const crypto_sign_vtable kEd25519Sha512Vtable = {
    sha512HashFn,
    edShaInit,
    edShaUpdate,
    edShaFinal,
    sizeof(Ed25519Sha512SignCtx),
};

void pad16(QByteArray &buf)
{
    while (buf.size() % 16 != 0)
        buf.append(char(0));
}

void writeU64LE(QByteArray &buf, quint64 v)
{
    for (int i = 0; i < 8; ++i) {
        buf.append(char(v & 0xff));
        v >>= 8;
    }
}

QByteArray hmacSha512(const QByteArray &key, const QByteArray &data)
{
    QByteArray k = key;
    if (k.size() > 128)
        k = sha512(k);
    while (k.size() < 128)
        k.append(char(0));
    QByteArray oKey = k;
    QByteArray iKey = k;
    for (int i = 0; i < 128; ++i) {
        oKey[i] = char(uint8_t(oKey[i]) ^ 0x5c);
        iKey[i] = char(uint8_t(iKey[i]) ^ 0x36);
    }
    return sha512(oKey + sha512(iKey + data));
}

}

QByteArray randomBytes(int n)
{
    QByteArray out(n, 0);
    for (int i = 0; i < n; ++i)
        out[i] = char(QRandomGenerator::system()->generate() & 0xff);
    return out;
}

QByteArray sha512(const QByteArray &a)
{
    QByteArray out(64, 0);
    sha512HashFn(reinterpret_cast<uint8_t *>(out.data()),
                 reinterpret_cast<const uint8_t *>(a.constData()), size_t(a.size()));
    return out;
}

QByteArray sha512(const QByteArray &a, const QByteArray &b)
{
    return sha512(QList<QByteArray>{a, b});
}

QByteArray sha512(const QList<QByteArray> &parts)
{
    Sha512Ctx ctx;
    sha512InitRaw(&ctx);
    for (const QByteArray &p : parts)
        sha512UpdateRaw(&ctx, reinterpret_cast<const uint8_t *>(p.constData()), size_t(p.size()));
    QByteArray out(64, 0);
    sha512FinalRaw(&ctx, reinterpret_cast<uint8_t *>(out.data()));
    return out;
}

QByteArray hkdfSha512(const QByteArray &ikm, const QByteArray &salt, const QByteArray &info, int length)
{
    const QByteArray realSalt = salt.isEmpty() ? QByteArray(64, 0) : salt;
    const QByteArray prk = hmacSha512(realSalt, ikm);
    QByteArray okm;
    QByteArray t;
    uint8_t counter = 1;
    while (okm.size() < length) {
        QByteArray block = t;
        block.append(info);
        block.append(char(counter++));
        t = hmacSha512(prk, block);
        okm.append(t);
    }
    return okm.left(length);
}

void ed25519PublicKey(const uint8_t seed[32], uint8_t pub[32])
{
    crypto_sign_public_key_custom_hash(pub, seed, &kEd25519Sha512Vtable);
}

QByteArray ed25519Sign(const QByteArray &seed, const QByteArray &message)
{
    uint8_t pk[32];
    ed25519PublicKey(reinterpret_cast<const uint8_t *>(seed.constData()), pk);
    Ed25519Sha512SignCtx ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    crypto_sign_init_first_pass_custom_hash(&ctx.abs,
                                            reinterpret_cast<const uint8_t *>(seed.constData()), pk,
                                            &kEd25519Sha512Vtable);
    crypto_sign_update(&ctx.abs, reinterpret_cast<const uint8_t *>(message.constData()),
                       size_t(message.size()));
    crypto_sign_init_second_pass(&ctx.abs);
    crypto_sign_update(&ctx.abs, reinterpret_cast<const uint8_t *>(message.constData()),
                       size_t(message.size()));
    QByteArray sig(64, 0);
    crypto_sign_final(&ctx.abs, reinterpret_cast<uint8_t *>(sig.data()));
    return sig;
}

bool ed25519Verify(const QByteArray &publicKey, const QByteArray &message, const QByteArray &signature)
{
    if (publicKey.size() != 32 || signature.size() != 64)
        return false;
    Ed25519Sha512SignCtx ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    crypto_check_init_custom_hash(&ctx.abs, reinterpret_cast<const uint8_t *>(signature.constData()),
                                  reinterpret_cast<const uint8_t *>(publicKey.constData()),
                                  &kEd25519Sha512Vtable);
    crypto_check_update(&ctx.abs, reinterpret_cast<const uint8_t *>(message.constData()),
                        size_t(message.size()));
    return crypto_check_final(&ctx.abs) == 0;
}

X25519KeyPair x25519Generate()
{
    X25519KeyPair kp;
    kp.privateKey = randomBytes(32);
    kp.publicKey.resize(32);
    crypto_x25519_public_key(reinterpret_cast<uint8_t *>(kp.publicKey.data()),
                             reinterpret_cast<const uint8_t *>(kp.privateKey.constData()));
    return kp;
}

QByteArray x25519Shared(const QByteArray &privateKey, const QByteArray &peerPublic)
{
    QByteArray shared(32, 0);
    crypto_x25519(reinterpret_cast<uint8_t *>(shared.data()),
                  reinterpret_cast<const uint8_t *>(privateKey.constData()),
                  reinterpret_cast<const uint8_t *>(peerPublic.constData()));
    return shared;
}

QByteArray nonceLabel(const char *label)
{
    QByteArray nonce(12, 0);
    const int n = int(std::strlen(label));
    const int copy = n < 8 ? n : 8;
    for (int i = 0; i < copy; ++i)
        nonce[4 + i] = label[i];
    return nonce;
}

QByteArray nonce64(quint64 counter)
{
    QByteArray nonce(12, 0);
    for (int i = 0; i < 8; ++i)
        nonce[4 + i] = char((counter >> (8 * i)) & 0xff);
    return nonce;
}

QByteArray chachaSeal(const QByteArray &key, const QByteArray &nonce12, const QByteArray &plaintext,
                      const QByteArray &aad)
{
    if (key.size() != 32 || nonce12.size() != 12)
        return {};
    uint8_t block[64];
    std::memset(block, 0, 64);
    crypto_ietf_chacha20_ctr(block, block, 64, reinterpret_cast<const uint8_t *>(key.constData()),
                             reinterpret_cast<const uint8_t *>(nonce12.constData()), 0);
    uint8_t polyKey[32];
    std::memcpy(polyKey, block, 32);

    QByteArray cipher(plaintext.size(), 0);
    if (!plaintext.isEmpty()) {
        crypto_ietf_chacha20_ctr(reinterpret_cast<uint8_t *>(cipher.data()),
                                 reinterpret_cast<const uint8_t *>(plaintext.constData()),
                                 size_t(plaintext.size()),
                                 reinterpret_cast<const uint8_t *>(key.constData()),
                                 reinterpret_cast<const uint8_t *>(nonce12.constData()), 1);
    }

    QByteArray macInput = aad;
    pad16(macInput);
    QByteArray paddedCipher = cipher;
    pad16(paddedCipher);
    macInput.append(paddedCipher);
    writeU64LE(macInput, quint64(aad.size()));
    writeU64LE(macInput, quint64(cipher.size()));

    uint8_t mac[16];
    crypto_poly1305(mac, reinterpret_cast<const uint8_t *>(macInput.constData()),
                    size_t(macInput.size()), polyKey);
    cipher.append(reinterpret_cast<const char *>(mac), 16);
    crypto_wipe(polyKey, 32);
    return cipher;
}

QByteArray chachaOpen(const QByteArray &key, const QByteArray &nonce12, const QByteArray &ciphertextAndTag,
                      const QByteArray &aad)
{
    if (key.size() != 32 || nonce12.size() != 12 || ciphertextAndTag.size() < 16)
        return QByteArray();
    const QByteArray cipher = ciphertextAndTag.left(ciphertextAndTag.size() - 16);
    const QByteArray tag = ciphertextAndTag.right(16);

    uint8_t block[64];
    std::memset(block, 0, 64);
    crypto_ietf_chacha20_ctr(block, block, 64, reinterpret_cast<const uint8_t *>(key.constData()),
                             reinterpret_cast<const uint8_t *>(nonce12.constData()), 0);
    uint8_t polyKey[32];
    std::memcpy(polyKey, block, 32);

    QByteArray macInput = aad;
    pad16(macInput);
    QByteArray paddedCipher = cipher;
    pad16(paddedCipher);
    macInput.append(paddedCipher);
    writeU64LE(macInput, quint64(aad.size()));
    writeU64LE(macInput, quint64(cipher.size()));

    uint8_t mac[16];
    crypto_poly1305(mac, reinterpret_cast<const uint8_t *>(macInput.constData()),
                    size_t(macInput.size()), polyKey);
    crypto_wipe(polyKey, 32);
    if (crypto_verify16(mac, reinterpret_cast<const uint8_t *>(tag.constData())) != 0)
        return QByteArray();

    QByteArray plain(cipher.size(), 0);
    if (!cipher.isEmpty()) {
        crypto_ietf_chacha20_ctr(reinterpret_cast<uint8_t *>(plain.data()),
                                 reinterpret_cast<const uint8_t *>(cipher.constData()),
                                 size_t(cipher.size()),
                                 reinterpret_cast<const uint8_t *>(key.constData()),
                                 reinterpret_cast<const uint8_t *>(nonce12.constData()), 1);
    }
    return plain;
}

QByteArray sha1(const QList<QByteArray> &parts)
{
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0)
        return {};
    DWORD objLen = 0, cb = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0);
    QByteArray obj(int(objLen), 0);
    if (BCryptCreateHash(alg, &hash, reinterpret_cast<PUCHAR>(obj.data()), objLen, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }
    for (const QByteArray &p : parts)
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(p.constData())), ULONG(p.size()), 0);
    QByteArray out(20, 0);
    BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(out.data()), 20, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
#else
    Q_UNUSED(parts);
    return {};
#endif
}

QByteArray sha256(const QList<QByteArray> &parts)
{
    QByteArray all;
    for (const QByteArray &p : parts)
        all.append(p);
    return QCryptographicHash::hash(all, QCryptographicHash::Sha256);
}

QByteArray aesCtr128(const QByteArray &key16, const QByteArray &iv16, const QByteArray &data)
{
#ifdef Q_OS_WIN
    if (key16.size() != 16 || iv16.size() != 16)
        return {};
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
        return {};
    BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_ECB)),
                      sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    if (BCryptGenerateSymmetricKey(alg, &key, nullptr, 0,
                                   reinterpret_cast<PUCHAR>(const_cast<char *>(key16.constData())), 16, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return {};
    }
    QByteArray counter = iv16;
    QByteArray out(data.size(), 0);
    for (int offset = 0; offset < data.size(); offset += 16) {
        QByteArray keystream(16, 0);
        ULONG written = 0;
        if (BCryptEncrypt(key, reinterpret_cast<PUCHAR>(counter.data()), 16, nullptr, nullptr, 0,
                          reinterpret_cast<PUCHAR>(keystream.data()), 16, &written, 0) != 0) {
            BCryptDestroyKey(key);
            BCryptCloseAlgorithmProvider(alg, 0);
            return {};
        }
        const int n = qMin(16, data.size() - offset);
        for (int i = 0; i < n; ++i)
            out[offset + i] = char(quint8(data[offset + i]) ^ quint8(keystream[i]));
        for (int i = 15; i >= 0; --i) {
            const quint8 v = quint8(counter[i]) + 1;
            counter[i] = char(v);
            if (v != 0)
                break;
        }
    }
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return out;
#else
    Q_UNUSED(key16);
    Q_UNUSED(iv16);
    Q_UNUSED(data);
    return {};
#endif
}

QByteArray concat(const QList<QByteArray> &parts)
{
    QByteArray out;
    for (const QByteArray &p : parts)
        out.append(p);
    return out;
}

}
