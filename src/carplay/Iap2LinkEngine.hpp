#pragma once

#include <QByteArray>
#include <QList>
#include <QQueue>
#include <QString>

#include <optional>

struct Iap2LinkConfig {
    int maxOutgoing = 30;
    int maxOutgoingDelta = 0;
    int acknowledgementTimeoutMillis = 500;
    bool zeroAcknowledgements = false;
    int controlSessionVersion = 1;
    int maximumQueuedPackets = 64;
    int maximumOutOfOrderPackets = 64;
    int maximumPendingOutputBytes = 1'048'576;
    int maximumPendingEvents = 256;
};

inline Iap2LinkConfig iap2WiredLinkConfig()
{
    Iap2LinkConfig c;
    c.maxOutgoing = 4;
    c.controlSessionVersion = 2;
    c.zeroAcknowledgements = true;
    return c;
}

inline Iap2LinkConfig iap2WirelessLinkConfig()
{
    Iap2LinkConfig c;
    c.maxOutgoing = 4;
    c.controlSessionVersion = 2;
    return c;
}

class Iap2LinkEngine {
public:
    enum class State {
        Idle,
        Detecting,
        Negotiating,
        Normal,
        Dead,
    };

    struct SessionDescriptor {
        int id = 0;
        int kind = 0;
        int version = 0;
    };

    struct SynchronizationPayload {
        int maxOutgoing = 0;
        int maxLength = 0;
        int retransmissionTimeoutMillis = 0;
        int acknowledgementTimeoutMillis = 0;
        int maxRetransmissions = 0;
        int maxAcknowledgements = 0;
        QList<SessionDescriptor> sessions;

        QByteArray encode() const;
        static std::optional<SynchronizationPayload> decode(const QByteArray &bytes);
    };

    struct Event {
        enum class Type {
            Control,
            Session,
            Writable,
            Dead,
        };
        Type type = Type::Dead;
        QByteArray bytes;
        int sessionId = 0;
        bool writable = false;
        std::optional<QString> deadReason;

        static Event control(const QByteArray &bytes);
        static Event session(int sessionId, const QByteArray &bytes);
        static Event writableEvent(bool value);
        static Event dead(const std::optional<QString> &reason);
    };

    static constexpr int kControlSessionId = 10;
    static constexpr int kEaSessionId = 11;
    static constexpr int kFileTransferSessionId = 12;
    static constexpr int kMaxWireFrameBytes = 0xffff;
    static constexpr int kMaxPayloadBytes = kMaxWireFrameBytes - 9 - 1;

    static const QByteArray &iap2Marker();

    explicit Iap2LinkEngine(const Iap2LinkConfig &config = Iap2LinkConfig());

    State state() const;
    bool writable() const;
    SynchronizationPayload peerSynchronization() const;

    QByteArray takeOutput();
    std::optional<Event> pollEvent();
    std::optional<qint64> nextDeadlineMillis() const;

    void start(bool wiredInitiator, qint64 nowMillis);
    void feed(const QByteArray &bytes, qint64 nowMillis);
    void feedEof();
    void advanceTime(qint64 nowMillis);
    void sendControl(const QByteArray &bytes, qint64 nowMillis);
    void sendSession(int sessionId, const QByteArray &bytes, qint64 nowMillis);

private:
    struct Header {
        int length = 0;
        int control = 0;
        int sequence = 0;
        int acknowledgement = 0;
        int sessionId = 0;
    };

    struct Packet {
        int sequence = 0;
        int sessionId = 0;
        QByteArray payload;
        int retransmissions = 0;
        qint64 deadlineMillis = 0;
    };

    class ReceiveBuffer {
    public:
        int size() const;
        int remainingCapacity();
        char operator[](int index) const;
        void append(const QByteArray &source, int offset, int count);
        void discard(int count);
        QByteArray take(int count);
        bool matches(const QByteArray &expected) const;

    private:
        void compact();
        QByteArray m_bytes;
        int m_head = 0;
        int m_tail = 0;
    };

    static constexpr int kHeaderBytes = 9;
    static constexpr int kChecksumBytes = 1;
    static constexpr int kSessionDescriptorBytes = 3;
    static constexpr int kSynchronizationFixedBytes = 10;
    static constexpr int kSynchronizationVersion = 1;
    static constexpr char kLinkStartHigh = char(0xff);
    static constexpr char kLinkStartLow = char(0x5a);
    static constexpr int kControlSynchronize = 0x80;
    static constexpr int kControlAcknowledgement = 0x40;
    static constexpr int kControlExtendedAcknowledgement = 0x20;
    static constexpr int kControlReset = 0x10;
    static constexpr int kInitialSequence = 99;
    static constexpr qint64 kMarkerResendMillis = 1000;
    static constexpr qint64 kSynchronizationResendMillis = 500;
    static constexpr int kDefaultRetransmissionTimeoutMillis = 4000;
    static constexpr int kDefaultMaxRetransmissions = 4;
    static constexpr int kDefaultMaxAcknowledgements = 3;
    static constexpr int kAckSlack = 10;

    bool parseAvailable(qint64 nowMillis);
    void enterNegotiating(qint64 nowMillis);
    void processFrame(const Header &header, const QByteArray *payload, qint64 nowMillis);
    void handleSynchronization(const SynchronizationPayload &sync, int sequence);
    void handleAcknowledgement(int acknowledgement, qint64 nowMillis);
    void handleExtendedAcknowledgement(const QByteArray &missing);
    void handleData(Packet packet, qint64 nowMillis);
    void sendPacket(Packet packet, qint64 nowMillis);
    void retransmitDuePacket(qint64 nowMillis);
    void sendSynchronization();
    void sendAcknowledgement();
    void sendExtendedAcknowledgement(const QList<int> &missing);
    void sendData(int sequence, int sessionId, const QByteArray &payload);
    void writePacket(const QByteArray *payload, int sequence, int control, int sessionId);
    void setWritable(bool value);
    void die(const std::optional<QString> &reason);
    bool appendOutput(const QByteArray &bytes);
    void enqueueEvent(const Event &event);
    bool peerLimitsAreUsable() const;
    bool peerPayloadIsAcceptable(const QByteArray &payload) const;
    SynchronizationPayload localSynchronization() const;

    static std::optional<Header> parseHeader(const QByteArray &bytes);
    static bool checksumValid(const QByteArray &bytes);
    static int checksum(const QByteArray &bytes, int count);
    static int sequenceDistance(int sequence, std::optional<int> previous);
    static int nextSequence(int sequence);
    static int readU16(const QByteArray &bytes, int offset);
    static int u8(char byte);
    static void writeU16(QByteArray &out, int value);

    Iap2LinkConfig m_config;
    State m_state = State::Idle;
    SynchronizationPayload m_peerSynchronization;
    bool m_peerSynchronizationReceived = false;
    int m_sentSequence = kInitialSequence;
    std::optional<int> m_lastSentAcknowledged;
    int m_lastReceivedInOrder = 0;
    std::optional<int> m_lastAcknowledgedReceived;
    int m_accumulatedAcknowledgements = 0;
    bool m_writable = false;

    QList<Packet> m_unacknowledged;
    QQueue<Packet> m_queued;
    QList<Packet> m_outOfOrder;
    QQueue<Event> m_events;
    QByteArray m_output;
    ReceiveBuffer m_receive;
    std::optional<Header> m_awaitingPayload;

    std::optional<qint64> m_markerDeadlineMillis;
    std::optional<qint64> m_synchronizationDeadlineMillis;
    std::optional<qint64> m_sendAcknowledgementDeadlineMillis;
    std::optional<qint64> m_receiveAcknowledgementDeadlineMillis;
};
