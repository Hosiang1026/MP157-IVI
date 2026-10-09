#include "Iap2LinkEngine.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

Iap2LinkEngine::Event Iap2LinkEngine::Event::control(const QByteArray &bytes)
{
    Event e;
    e.type = Type::Control;
    e.bytes = bytes;
    return e;
}

Iap2LinkEngine::Event Iap2LinkEngine::Event::session(int sessionId, const QByteArray &bytes)
{
    Event e;
    e.type = Type::Session;
    e.sessionId = sessionId;
    e.bytes = bytes;
    return e;
}

Iap2LinkEngine::Event Iap2LinkEngine::Event::writableEvent(bool value)
{
    Event e;
    e.type = Type::Writable;
    e.writable = value;
    return e;
}

Iap2LinkEngine::Event Iap2LinkEngine::Event::dead(const std::optional<QString> &reason)
{
    Event e;
    e.type = Type::Dead;
    e.deadReason = reason;
    return e;
}

const QByteArray &Iap2LinkEngine::iap2Marker()
{
    static const QByteArray marker = QByteArray("\xff\x55\x02\x00\xee\x10", 6);
    return marker;
}

QByteArray Iap2LinkEngine::SynchronizationPayload::encode() const
{
    QByteArray out;
    out.reserve(10 + sessions.size() * Iap2LinkEngine::kSessionDescriptorBytes);
    out.append(char(Iap2LinkEngine::kSynchronizationVersion));
    out.append(char(maxOutgoing));
    Iap2LinkEngine::writeU16(out, maxLength);
    Iap2LinkEngine::writeU16(out, retransmissionTimeoutMillis);
    Iap2LinkEngine::writeU16(out, acknowledgementTimeoutMillis);
    out.append(char(maxRetransmissions));
    out.append(char(maxAcknowledgements));
    for (const SessionDescriptor &s : sessions) {
        out.append(char(s.id));
        out.append(char(s.kind));
        out.append(char(s.version));
    }
    return out;
}

std::optional<Iap2LinkEngine::SynchronizationPayload> Iap2LinkEngine::SynchronizationPayload::decode(
    const QByteArray &bytes)
{
    if (bytes.size() < Iap2LinkEngine::kSynchronizationFixedBytes
        || Iap2LinkEngine::u8(bytes[0]) != Iap2LinkEngine::kSynchronizationVersion)
        return std::nullopt;
    SynchronizationPayload payload;
    payload.maxOutgoing = Iap2LinkEngine::u8(bytes[1]);
    payload.maxLength = Iap2LinkEngine::readU16(bytes, 2);
    payload.retransmissionTimeoutMillis = Iap2LinkEngine::readU16(bytes, 4);
    payload.acknowledgementTimeoutMillis = Iap2LinkEngine::readU16(bytes, 6);
    payload.maxRetransmissions = Iap2LinkEngine::u8(bytes[8]);
    payload.maxAcknowledgements = Iap2LinkEngine::u8(bytes[9]);
    int offset = Iap2LinkEngine::kSynchronizationFixedBytes;
    while (offset + Iap2LinkEngine::kSessionDescriptorBytes <= bytes.size()) {
        SessionDescriptor d;
        d.id = Iap2LinkEngine::u8(bytes[offset]);
        d.kind = Iap2LinkEngine::u8(bytes[offset + 1]);
        d.version = Iap2LinkEngine::u8(bytes[offset + 2]);
        payload.sessions.append(d);
        offset += Iap2LinkEngine::kSessionDescriptorBytes;
    }
    return payload;
}

Iap2LinkEngine::Iap2LinkEngine(const Iap2LinkConfig &config)
    : m_config(config)
    , m_peerSynchronization(localSynchronization())
{
    m_receive = ReceiveBuffer();
}

Iap2LinkEngine::State Iap2LinkEngine::state() const
{
    return m_state;
}

bool Iap2LinkEngine::writable() const
{
    return m_writable;
}

Iap2LinkEngine::SynchronizationPayload Iap2LinkEngine::peerSynchronization() const
{
    return m_peerSynchronization;
}

QByteArray Iap2LinkEngine::takeOutput()
{
    QByteArray bytes = m_output;
    m_output.clear();
    return bytes;
}

std::optional<Iap2LinkEngine::Event> Iap2LinkEngine::pollEvent()
{
    if (m_events.isEmpty())
        return std::nullopt;
    return m_events.dequeue();
}

std::optional<qint64> Iap2LinkEngine::nextDeadlineMillis() const
{
    std::optional<qint64> best;
    auto consider = [&](const std::optional<qint64> &v) {
        if (!v.has_value())
            return;
        if (!best.has_value() || *v < *best)
            best = v;
    };
    consider(m_markerDeadlineMillis);
    consider(m_synchronizationDeadlineMillis);
    consider(m_sendAcknowledgementDeadlineMillis);
    consider(m_receiveAcknowledgementDeadlineMillis);
    return best;
}

void Iap2LinkEngine::start(bool wiredInitiator, qint64 nowMillis)
{
    if (m_state != State::Idle)
        return;
    m_state = State::Detecting;
    if (!appendOutput(iap2Marker()))
        return;
    m_markerDeadlineMillis = nowMillis + kMarkerResendMillis;
    if (wiredInitiator && m_state != State::Dead)
        enterNegotiating(nowMillis);
}

void Iap2LinkEngine::feed(const QByteArray &bytes, qint64 nowMillis)
{
    if (m_state == State::Dead || bytes.isEmpty())
        return;
    int offset = 0;
    while (offset < bytes.size() && m_state != State::Dead) {
        const int room = m_receive.remainingCapacity();
        if (room == 0) {
            if (!parseAvailable(nowMillis)) {
                die(QStringLiteral("iAP2 inbound frame exceeds %1 bytes").arg(kMaxWireFrameBytes));
                return;
            }
            continue;
        }
        const int count = qMin(room, bytes.size() - offset);
        m_receive.append(bytes, offset, count);
        offset += count;
        while (parseAvailable(nowMillis) && m_state != State::Dead) {
        }
    }
}

void Iap2LinkEngine::feedEof()
{
    die(std::nullopt);
}

void Iap2LinkEngine::advanceTime(qint64 nowMillis)
{
    while (m_state != State::Dead) {
        const auto deadline = nextDeadlineMillis();
        if (!deadline.has_value())
            return;
        if (*deadline > nowMillis)
            return;
        if (m_markerDeadlineMillis.has_value() && *m_markerDeadlineMillis <= nowMillis) {
            m_markerDeadlineMillis = std::nullopt;
            if (m_state == State::Detecting) {
                if (appendOutput(iap2Marker()))
                    m_markerDeadlineMillis = nowMillis + kMarkerResendMillis;
            }
        } else if (m_synchronizationDeadlineMillis.has_value()
                   && *m_synchronizationDeadlineMillis <= nowMillis) {
            m_synchronizationDeadlineMillis = std::nullopt;
            if (m_state == State::Negotiating) {
                sendSynchronization();
                if (m_state != State::Dead)
                    m_synchronizationDeadlineMillis = nowMillis + kSynchronizationResendMillis;
            }
        } else if (m_sendAcknowledgementDeadlineMillis.has_value()
                   && *m_sendAcknowledgementDeadlineMillis <= nowMillis) {
            m_sendAcknowledgementDeadlineMillis = std::nullopt;
            if (m_state == State::Normal) {
                m_lastAcknowledgedReceived = m_lastReceivedInOrder;
                sendAcknowledgement();
            }
        } else if (m_receiveAcknowledgementDeadlineMillis.has_value()
                   && *m_receiveAcknowledgementDeadlineMillis <= nowMillis) {
            m_receiveAcknowledgementDeadlineMillis = std::nullopt;
            retransmitDuePacket(nowMillis);
        }
    }
}

void Iap2LinkEngine::sendControl(const QByteArray &bytes, qint64 nowMillis)
{
    sendSession(kControlSessionId, bytes, nowMillis);
}

void Iap2LinkEngine::sendSession(int sessionId, const QByteArray &bytes, qint64 nowMillis)
{
    if (sessionId < 0 || sessionId > 0xff)
        throw std::invalid_argument("iAP2 session id must fit in one byte");
    if (bytes.size() > kMaxPayloadBytes)
        throw std::invalid_argument("iAP2 session payload exceeds max");
    if (m_peerSynchronizationReceived || m_state == State::Normal) {
        if (!peerPayloadIsAcceptable(bytes))
            throw std::invalid_argument("iAP2 session payload exceeds peer maxLength");
    }
    Packet packet;
    packet.sessionId = sessionId;
    packet.payload = bytes;
    sendPacket(std::move(packet), nowMillis);
}

bool Iap2LinkEngine::parseAvailable(qint64 nowMillis)
{
    if (m_state == State::Dead)
        return false;
    if (m_state == State::Detecting) {
        if (m_receive.size() < iap2Marker().size())
            return false;
        if (!m_receive.matches(iap2Marker())) {
            die(QStringLiteral("iAP2 marker was not received"));
            return false;
        }
        m_receive.discard(iap2Marker().size());
        enterNegotiating(nowMillis);
        return true;
    }

    if (m_awaitingPayload.has_value()) {
        const Header pending = *m_awaitingPayload;
        const int payloadWithChecksumBytes = pending.length - kHeaderBytes;
        if (m_receive.size() < payloadWithChecksumBytes)
            return false;
        const QByteArray payloadWithChecksum = m_receive.take(payloadWithChecksumBytes);
        m_awaitingPayload = std::nullopt;
        if (checksumValid(payloadWithChecksum)) {
            const QByteArray payload = payloadWithChecksum.left(payloadWithChecksum.size() - kChecksumBytes);
            processFrame(pending, &payload, nowMillis);
        }
        return true;
    }

    bool discarded = false;
    while (m_receive.size() >= 2
           && (m_receive[0] != kLinkStartHigh || m_receive[1] != kLinkStartLow)) {
        m_receive.discard(1);
        discarded = true;
    }
    if (m_receive.size() < kHeaderBytes)
        return discarded;
    const QByteArray rawHeader = m_receive.take(kHeaderBytes);
    const auto header = parseHeader(rawHeader);
    if (!header.has_value() || header->length < kHeaderBytes)
        return true;
    if (header->length > kMaxWireFrameBytes) {
        die(QStringLiteral("iAP2 frame length %1 exceeds %2").arg(header->length).arg(kMaxWireFrameBytes));
        return false;
    }
    if (header->length == kHeaderBytes) {
        processFrame(*header, nullptr, nowMillis);
    } else {
        m_awaitingPayload = header;
    }
    return true;
}

void Iap2LinkEngine::enterNegotiating(qint64 nowMillis)
{
    m_state = State::Negotiating;
    m_markerDeadlineMillis = std::nullopt;
    sendSynchronization();
    if (m_state != State::Dead)
        m_synchronizationDeadlineMillis = nowMillis + kSynchronizationResendMillis;
}

void Iap2LinkEngine::processFrame(const Header &header, const QByteArray *payload, qint64 nowMillis)
{
    if ((header.control & kControlReset) != 0) {
        die(QStringLiteral("peer sent iAP2 reset"));
        return;
    }
    if ((header.control & kControlSynchronize) != 0) {
        const QByteArray empty;
        const auto sync = SynchronizationPayload::decode(payload ? *payload : empty);
        if (sync.has_value())
            handleSynchronization(*sync, header.sequence);
    }
    if ((header.control & kControlAcknowledgement) != 0) {
        m_accumulatedAcknowledgements += 1;
        handleAcknowledgement(header.acknowledgement, nowMillis);
    }
    if ((header.control & kControlExtendedAcknowledgement) != 0 && payload) {
        handleExtendedAcknowledgement(*payload);
    }
    if ((header.control & ~kControlAcknowledgement) == 0 && payload) {
        Packet packet;
        packet.sequence = header.sequence;
        packet.sessionId = header.sessionId;
        packet.payload = *payload;
        handleData(std::move(packet), nowMillis);
    }
    if (m_peerSynchronization.maxAcknowledgements > 0
        && m_accumulatedAcknowledgements >= m_peerSynchronization.maxAcknowledgements) {
        m_accumulatedAcknowledgements = 0;
        m_lastAcknowledgedReceived = m_lastReceivedInOrder;
        sendAcknowledgement();
    }
}

void Iap2LinkEngine::handleSynchronization(const SynchronizationPayload &sync, int sequence)
{
    if (m_state != State::Negotiating)
        return;
    m_peerSynchronization = sync;
    m_peerSynchronizationReceived = true;
    m_lastReceivedInOrder = sequence;
    m_lastAcknowledgedReceived = sequence;
    sendAcknowledgement();
}

void Iap2LinkEngine::handleAcknowledgement(int acknowledgement, qint64 nowMillis)
{
    if (m_state == State::Negotiating) {
        bool pendingOk = true;
        for (const Packet &p : m_queued) {
            if (!peerPayloadIsAcceptable(p.payload)) {
                pendingOk = false;
                break;
            }
        }
        if (!m_peerSynchronizationReceived || !peerLimitsAreUsable() || !pendingOk) {
            die(QStringLiteral("peer iAP2 maxLength cannot carry pending control packets"));
            return;
        }
        m_state = State::Normal;
        m_synchronizationDeadlineMillis = std::nullopt;
        setWritable(true);
    }
    m_lastSentAcknowledged = acknowledgement;

    bool rearmed = false;
    while (!m_unacknowledged.isEmpty()) {
        const Packet &first = m_unacknowledged.first();
        const int distance = sequenceDistance(first.sequence, m_lastSentAcknowledged);
        if (distance > 0 && distance <= m_peerSynchronization.maxAcknowledgements + kAckSlack) {
            m_receiveAcknowledgementDeadlineMillis = first.deadlineMillis;
            rearmed = true;
            break;
        }
        m_unacknowledged.removeFirst();
    }
    if (!rearmed)
        m_receiveAcknowledgementDeadlineMillis = std::nullopt;

    while (sequenceDistance(m_sentSequence, m_lastSentAcknowledged) < m_peerSynchronization.maxOutgoing) {
        if (m_queued.isEmpty())
            break;
        Packet packet = m_queued.dequeue();
        sendPacket(std::move(packet), nowMillis);
        setWritable(true);
    }
}

void Iap2LinkEngine::handleExtendedAcknowledgement(const QByteArray &missing)
{
    if (m_state != State::Normal)
        return;
    for (Packet &packet : m_unacknowledged) {
        bool found = false;
        for (char b : missing) {
            if (u8(b) == packet.sequence) {
                found = true;
                break;
            }
        }
        if (!found)
            continue;
        packet.retransmissions += 1;
        if (packet.retransmissions == m_peerSynchronization.maxRetransmissions) {
            die(QStringLiteral("iAP2 packet %1 was not acknowledged").arg(packet.sequence));
            return;
        }
        sendData(packet.sequence, packet.sessionId, packet.payload);
        m_sendAcknowledgementDeadlineMillis = std::nullopt;
        m_receiveAcknowledgementDeadlineMillis = packet.deadlineMillis;
    }
}

void Iap2LinkEngine::handleData(Packet packet, qint64 nowMillis)
{
    const int distance = sequenceDistance(packet.sequence, m_lastReceivedInOrder);
    if (distance == 0 || distance > m_peerSynchronization.maxOutgoing + kAckSlack) {
        sendAcknowledgement();
        return;
    }
    for (const Packet &existing : m_outOfOrder) {
        if (existing.sequence == packet.sequence)
            return;
    }
    if (m_outOfOrder.size() >= m_config.maximumOutOfOrderPackets) {
        die(QStringLiteral("iAP2 out-of-order packet limit exceeded"));
        return;
    }
    m_outOfOrder.append(packet);
    if (distance > 1) {
        if (distance >= m_peerSynchronization.maxOutgoing) {
            QList<int> missing;
            int sequence = m_lastReceivedInOrder;
            while (sequenceDistance(packet.sequence, sequence) > 1) {
                sequence = nextSequence(sequence);
                missing.append(sequence);
            }
            m_sendAcknowledgementDeadlineMillis = std::nullopt;
            sendExtendedAcknowledgement(missing);
        }
        return;
    }

    std::sort(m_outOfOrder.begin(), m_outOfOrder.end(), [this](const Packet &a, const Packet &b) {
        return sequenceDistance(a.sequence, m_lastReceivedInOrder)
            < sequenceDistance(b.sequence, m_lastReceivedInOrder);
    });
    while (!m_outOfOrder.isEmpty()
           && sequenceDistance(m_outOfOrder.first().sequence, m_lastReceivedInOrder) == 1) {
        const Packet inOrder = m_outOfOrder.takeFirst();
        m_lastReceivedInOrder = inOrder.sequence;
        if (inOrder.sessionId == kControlSessionId)
            enqueueEvent(Event::control(inOrder.payload));
        else
            enqueueEvent(Event::session(inOrder.sessionId, inOrder.payload));
    }

    if (m_peerSynchronization.maxAcknowledgements == 0)
        return;
    const int windowBeforeForcedAck =
        qMax(1, m_peerSynchronization.maxOutgoing - m_config.maxOutgoingDelta);
    if (sequenceDistance(m_lastReceivedInOrder, m_lastAcknowledgedReceived) >= windowBeforeForcedAck) {
        m_sendAcknowledgementDeadlineMillis = std::nullopt;
        m_lastAcknowledgedReceived = m_lastReceivedInOrder;
        sendAcknowledgement();
    } else {
        m_sendAcknowledgementDeadlineMillis =
            nowMillis + m_peerSynchronization.acknowledgementTimeoutMillis;
    }
}

void Iap2LinkEngine::sendPacket(Packet packet, qint64 nowMillis)
{
    if (m_state != State::Normal
        || sequenceDistance(m_sentSequence, m_lastSentAcknowledged) > m_peerSynchronization.maxOutgoing) {
        if (m_queued.size() >= m_config.maximumQueuedPackets) {
            die(QStringLiteral("iAP2 outbound queue limit exceeded"));
        } else {
            m_queued.enqueue(packet);
            setWritable(false);
        }
        return;
    }
    m_sentSequence = nextSequence(m_sentSequence);
    packet.sequence = m_sentSequence;
    packet.retransmissions = 0;
    packet.deadlineMillis = nowMillis + m_peerSynchronization.retransmissionTimeoutMillis;
    m_sendAcknowledgementDeadlineMillis = std::nullopt;
    sendData(packet.sequence, packet.sessionId, packet.payload);
    m_lastAcknowledgedReceived = m_lastReceivedInOrder;
    if (m_peerSynchronization.maxRetransmissions > 0) {
        m_unacknowledged.append(packet);
        m_receiveAcknowledgementDeadlineMillis = packet.deadlineMillis;
    } else {
        m_lastSentAcknowledged = m_sentSequence;
    }
}

void Iap2LinkEngine::retransmitDuePacket(qint64 nowMillis)
{
    if (m_state != State::Normal || m_unacknowledged.isEmpty())
        return;
    auto nextIt = std::min_element(m_unacknowledged.begin(), m_unacknowledged.end(),
                                   [](const Packet &a, const Packet &b) {
                                       return a.deadlineMillis < b.deadlineMillis;
                                   });
    if (nextIt == m_unacknowledged.end())
        return;
    Packet &next = *nextIt;
    std::optional<qint64> secondDeadline;
    for (const Packet &p : m_unacknowledged) {
        if (&p == &next)
            continue;
        if (!secondDeadline.has_value() || p.deadlineMillis < *secondDeadline)
            secondDeadline = p.deadlineMillis;
    }
    next.deadlineMillis = nowMillis + m_peerSynchronization.retransmissionTimeoutMillis;
    next.retransmissions += 1;
    if (next.retransmissions == m_peerSynchronization.maxRetransmissions) {
        die(QStringLiteral("iAP2 packet %1 was not acknowledged").arg(next.sequence));
        return;
    }
    sendData(next.sequence, next.sessionId, next.payload);
    m_receiveAcknowledgementDeadlineMillis = secondDeadline.has_value() ? secondDeadline : next.deadlineMillis;
}

void Iap2LinkEngine::sendSynchronization()
{
    const QByteArray payload = localSynchronization().encode();
    writePacket(&payload, m_sentSequence, kControlSynchronize, 0);
}

void Iap2LinkEngine::sendAcknowledgement()
{
    writePacket(nullptr, m_sentSequence, kControlAcknowledgement, 0);
}

void Iap2LinkEngine::sendExtendedAcknowledgement(const QList<int> &missing)
{
    QByteArray payload;
    payload.reserve(missing.size());
    for (int seq : missing)
        payload.append(char(seq));
    writePacket(&payload, m_sentSequence, kControlExtendedAcknowledgement, 0);
}

void Iap2LinkEngine::sendData(int sequence, int sessionId, const QByteArray &payload)
{
    writePacket(&payload, sequence, kControlAcknowledgement, sessionId);
}

void Iap2LinkEngine::writePacket(const QByteArray *payload, int sequence, int control, int sessionId)
{
    if (m_state == State::Dead)
        return;
    m_accumulatedAcknowledgements = 0;
    const int length = payload ? payload->size() + kHeaderBytes + kChecksumBytes : kHeaderBytes;
    QByteArray header(kHeaderBytes, Qt::Uninitialized);
    header[0] = kLinkStartHigh;
    header[1] = kLinkStartLow;
    header[2] = char((length >> 8) & 0xff);
    header[3] = char(length & 0xff);
    header[4] = char(control);
    header[5] = char(sequence);
    header[6] = char(m_lastReceivedInOrder);
    header[7] = char(sessionId);
    header[8] = char(checksum(header, kHeaderBytes - kChecksumBytes));
    QByteArray frame;
    frame.reserve(length);
    frame.append(header);
    if (payload) {
        frame.append(*payload);
        frame.append(char(checksum(*payload, payload->size())));
    }
    appendOutput(frame);
}

void Iap2LinkEngine::setWritable(bool value)
{
    if (m_writable == value)
        return;
    m_writable = value;
    enqueueEvent(Event::writableEvent(value));
}

void Iap2LinkEngine::die(const std::optional<QString> &reason)
{
    if (m_state == State::Dead)
        return;
    m_markerDeadlineMillis = std::nullopt;
    m_synchronizationDeadlineMillis = std::nullopt;
    m_sendAcknowledgementDeadlineMillis = std::nullopt;
    m_receiveAcknowledgementDeadlineMillis = std::nullopt;
    m_state = State::Dead;
    m_writable = false;
    m_output.clear();
    m_events.clear();
    m_events.enqueue(Event::dead(reason));
}

bool Iap2LinkEngine::appendOutput(const QByteArray &bytes)
{
    if (m_state == State::Dead)
        return false;
    if (bytes.size() > m_config.maximumPendingOutputBytes - m_output.size()) {
        die(QStringLiteral("iAP2 pending output limit exceeded"));
        return false;
    }
    m_output.append(bytes);
    return true;
}

void Iap2LinkEngine::enqueueEvent(const Event &event)
{
    if (m_state == State::Dead)
        return;
    if (m_events.size() >= m_config.maximumPendingEvents) {
        die(QStringLiteral("iAP2 pending event limit exceeded"));
        return;
    }
    m_events.enqueue(event);
}

bool Iap2LinkEngine::peerLimitsAreUsable() const
{
    return m_peerSynchronization.maxLength > kHeaderBytes + kChecksumBytes;
}

bool Iap2LinkEngine::peerPayloadIsAcceptable(const QByteArray &payload) const
{
    return peerLimitsAreUsable()
        && payload.size() <= m_peerSynchronization.maxLength - kHeaderBytes - kChecksumBytes;
}

Iap2LinkEngine::SynchronizationPayload Iap2LinkEngine::localSynchronization() const
{
    SynchronizationPayload sync;
    sync.maxOutgoing = m_config.maxOutgoing;
    sync.maxLength = kMaxWireFrameBytes;
    sync.retransmissionTimeoutMillis =
        m_config.zeroAcknowledgements ? 0 : kDefaultRetransmissionTimeoutMillis;
    sync.acknowledgementTimeoutMillis =
        m_config.zeroAcknowledgements ? 0 : m_config.acknowledgementTimeoutMillis;
    sync.maxRetransmissions = m_config.zeroAcknowledgements ? 0 : kDefaultMaxRetransmissions;
    sync.maxAcknowledgements = m_config.zeroAcknowledgements ? 0 : kDefaultMaxAcknowledgements;
    sync.sessions = {
        {kControlSessionId, 0, m_config.controlSessionVersion},
        {kEaSessionId, 2, 1},
        {kFileTransferSessionId, 1, 2},
    };
    return sync;
}

int Iap2LinkEngine::ReceiveBuffer::size() const
{
    return m_tail - m_head;
}

int Iap2LinkEngine::ReceiveBuffer::remainingCapacity()
{
    compact();
    if (m_bytes.size() < kMaxWireFrameBytes)
        m_bytes.resize(kMaxWireFrameBytes);
    return m_bytes.size() - m_tail;
}

char Iap2LinkEngine::ReceiveBuffer::operator[](int index) const
{
    return m_bytes.at(m_head + index);
}

void Iap2LinkEngine::ReceiveBuffer::append(const QByteArray &source, int offset, int count)
{
    if (count > remainingCapacity())
        throw std::invalid_argument("ReceiveBuffer overflow");
    memcpy(m_bytes.data() + m_tail, source.constData() + offset, size_t(count));
    m_tail += count;
}

void Iap2LinkEngine::ReceiveBuffer::discard(int count)
{
    if (count < 0 || count > size())
        throw std::invalid_argument("ReceiveBuffer discard");
    m_head += count;
    if (m_head == m_tail) {
        m_head = 0;
        m_tail = 0;
    }
}

QByteArray Iap2LinkEngine::ReceiveBuffer::take(int count)
{
    if (count < 0 || count > size())
        throw std::invalid_argument("ReceiveBuffer take");
    QByteArray out = m_bytes.mid(m_head, count);
    discard(count);
    return out;
}

bool Iap2LinkEngine::ReceiveBuffer::matches(const QByteArray &expected) const
{
    if (size() < expected.size())
        return false;
    return memcmp(m_bytes.constData() + m_head, expected.constData(), size_t(expected.size())) == 0;
}

void Iap2LinkEngine::ReceiveBuffer::compact()
{
    if (m_head == 0)
        return;
    if (m_head < m_tail)
        memmove(m_bytes.data(), m_bytes.constData() + m_head, size_t(m_tail - m_head));
    m_tail -= m_head;
    m_head = 0;
}

std::optional<Iap2LinkEngine::Header> Iap2LinkEngine::parseHeader(const QByteArray &bytes)
{
    if (bytes.size() != kHeaderBytes || !checksumValid(bytes))
        return std::nullopt;
    if (bytes[0] != kLinkStartHigh || bytes[1] != kLinkStartLow)
        return std::nullopt;
    Header header;
    header.length = readU16(bytes, 2);
    header.control = u8(bytes[4]);
    header.sequence = u8(bytes[5]);
    header.acknowledgement = u8(bytes[6]);
    header.sessionId = u8(bytes[7]);
    return header;
}

bool Iap2LinkEngine::checksumValid(const QByteArray &bytes)
{
    int sum = 0;
    for (char b : bytes)
        sum = (sum + u8(b)) & 0xff;
    return sum == 0;
}

int Iap2LinkEngine::checksum(const QByteArray &bytes, int count)
{
    int sum = 0;
    for (int i = 0; i < count; ++i)
        sum += u8(bytes[i]);
    return (-sum) & 0xff;
}

int Iap2LinkEngine::sequenceDistance(int sequence, std::optional<int> previous)
{
    if (!previous.has_value())
        return 0;
    return (sequence - *previous) & 0xff;
}

int Iap2LinkEngine::nextSequence(int sequence)
{
    return (sequence + 1) & 0xff;
}

int Iap2LinkEngine::readU16(const QByteArray &bytes, int offset)
{
    return (u8(bytes[offset]) << 8) | u8(bytes[offset + 1]);
}

int Iap2LinkEngine::u8(char byte)
{
    return int(quint8(byte));
}

void Iap2LinkEngine::writeU16(QByteArray &out, int value)
{
    out.append(char((value >> 8) & 0xff));
    out.append(char(value & 0xff));
}
