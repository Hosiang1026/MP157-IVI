#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>

namespace Iap2Wire {

constexpr int kCsmStart = 0x4040;
constexpr int kCsmHeaderBytes = 6;
constexpr int kMaxFrameBytes = 0xffff;
constexpr int kMaxBodyBytes = kMaxFrameBytes - kCsmHeaderBytes;

constexpr int kRequestCertificate = 0xaa00;
constexpr int kAccessoryCertificate = 0xaa01;
constexpr int kRequestChallenge = 0xaa02;
constexpr int kChallengeResponse = 0xaa03;
constexpr int kAuthenticationFailed = 0xaa04;
constexpr int kAuthenticationSucceeded = 0xaa05;

QByteArray encodeParam(quint16 id, const QByteArray &payload);
QByteArray encodeFrame(quint16 messageId, const QByteArray &body);
QByteArray accessoryCertificateFrame(const QByteArray &certificate);
QByteArray challengeResponseFrame(const QByteArray &signature);

struct Frame {
    quint16 messageId = 0;
    QByteArray body;
};

class CsmFramer {
public:
    QList<Frame> offer(const QByteArray &chunk);

private:
    QByteArray m_buf;
};

} // namespace Iap2Wire
