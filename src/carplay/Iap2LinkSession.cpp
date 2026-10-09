#include "Iap2LinkSession.hpp"

#include <chrono>
#include <exception>
#include <utility>

Iap2LinkSessionWorker::Iap2LinkSessionWorker(Iap2LinkEngine engine,
                                             bool wiredInitiator,
                                             QIODevice *device,
                                             bool ownDevice,
                                             WriteCallback write,
                                             ReadCallback read)
    : m_engine(std::move(engine))
    , m_wiredInitiator(wiredInitiator)
    , m_device(device)
    , m_ownDevice(ownDevice)
    , m_write(std::move(write))
    , m_read(std::move(read))
{
}

Iap2LinkSessionWorker::~Iap2LinkSessionWorker()
{
    if (m_ownDevice && m_device) {
        m_device->close();
        delete m_device;
        m_device = nullptr;
    }
}

void Iap2LinkSessionWorker::startPump()
{
    if (m_running)
        return;
    m_running = true;
    m_terminated = false;
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Iap2LinkSessionWorker::tick);

    const qint64 now = nowMillis();
    m_engine.start(m_wiredInitiator, now);
    emit stateChanged(m_engine.state());
    flushOutput(now);
    drainEvents();
    scheduleNext(now);
}

void Iap2LinkSessionWorker::stopPump()
{
    if (m_terminated)
        return;
    terminate(std::nullopt);
}

void Iap2LinkSessionWorker::enqueueControl(const QByteArray &bytes)
{
    QMutexLocker lock(&m_cmdMutex);
    if (m_terminated)
        return;
    if (m_commands.size() >= kMaxPendingCommands
        || m_commandBytes + bytes.size() > kMaxPendingCommandBytes) {
        lock.unlock();
        terminate(QStringLiteral("iAP2 command queue limit exceeded"));
        return;
    }
    m_commands.enqueue(bytes);
    m_commandBytes += bytes.size();
}

bool Iap2LinkSessionWorker::enqueueControlAndFlush(const QByteArray &bytes)
{
    if (m_terminated)
        return false;
    enqueueControl(bytes);
    const qint64 now = nowMillis();
    m_engine.advanceTime(now);
    pumpCommands(now);
    flushOutput(now);
    drainEvents();
    return !m_terminated;
}

void Iap2LinkSessionWorker::tick()
{
    if (m_terminated || !m_running)
        return;
    const qint64 now = nowMillis();
    m_engine.advanceTime(now);
    drainInput(now);
    pumpCommands(now);
    flushOutput(now);
    drainEvents();
    if (!m_terminated)
        scheduleNext(nowMillis());
}

void Iap2LinkSessionWorker::flushOutput(qint64 now)
{
    Q_UNUSED(now);
    if (m_terminated)
        return;
    const QByteArray out = m_engine.takeOutput();
    if (out.isEmpty())
        return;
    if (writeBytes(out) < 0)
        terminate(QStringLiteral("iAP2 write failed"));
}

void Iap2LinkSessionWorker::drainInput(qint64 now)
{
    if (m_terminated)
        return;
    while (!m_terminated) {
        const QByteArray chunk = readBytes(kReceiveChunkBytes);
        if (chunk.isEmpty())
            break;
        m_engine.feed(chunk, now);
        flushOutput(now);
        drainEvents();
        if (m_engine.state() == Iap2LinkEngine::State::Dead)
            break;
    }
    if (m_device && !m_device->isOpen() && !m_terminated) {
        m_engine.feedEof();
        drainEvents();
    }
}

void Iap2LinkSessionWorker::drainEvents()
{
    while (true) {
        const auto event = m_engine.pollEvent();
        if (!event.has_value())
            break;
        switch (event->type) {
        case Iap2LinkEngine::Event::Type::Control:
            emit controlReceived(event->bytes);
            break;
        case Iap2LinkEngine::Event::Type::Session:
            emit sessionReceived(event->sessionId, event->bytes);
            break;
        case Iap2LinkEngine::Event::Type::Writable:
            if (event->writable)
                emit becameReady();
            break;
        case Iap2LinkEngine::Event::Type::Dead:
            terminate(event->deadReason);
            return;
        }
    }
    emit stateChanged(m_engine.state());
}

void Iap2LinkSessionWorker::pumpCommands(qint64 now)
{
    if (m_terminated || m_engine.state() != Iap2LinkEngine::State::Normal)
        return;
    while (m_engine.writable()) {
        QByteArray payload;
        {
            QMutexLocker lock(&m_cmdMutex);
            if (m_commands.isEmpty())
                break;
            payload = m_commands.dequeue();
            m_commandBytes -= payload.size();
        }
        try {
            m_engine.sendControl(payload, now);
        } catch (const std::exception &ex) {
            terminate(QString::fromUtf8(ex.what()));
            return;
        }
        flushOutput(now);
        drainEvents();
        if (m_terminated)
            return;
    }
}

void Iap2LinkSessionWorker::scheduleNext(qint64 now)
{
    if (!m_timer || m_terminated)
        return;
    int delay = kDefaultPollMs;
    if (const auto deadline = m_engine.nextDeadlineMillis()) {
        const qint64 delta = *deadline - now;
        if (delta <= 0)
            delay = 1;
        else if (delta < delay)
            delay = int(delta);
    }
    if (m_device && m_device->bytesAvailable() > 0)
        delay = 1;
    m_timer->start(delay);
}

void Iap2LinkSessionWorker::terminate(const std::optional<QString> &reason)
{
    if (m_terminated)
        return;
    m_terminated = true;
    m_running = false;
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    if (m_device && m_device->isOpen())
        m_device->close();
    {
        QMutexLocker lock(&m_cmdMutex);
        m_commands.clear();
        m_commandBytes = 0;
    }
    emit becameDead(reason.value_or(QString()));
    emit stateChanged(Iap2LinkEngine::State::Dead);
}

qint64 Iap2LinkSessionWorker::nowMillis() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

qint64 Iap2LinkSessionWorker::writeBytes(const QByteArray &data)
{
    if (m_write)
        return m_write(data);
    if (!m_device || !m_device->isOpen())
        return -1;
    qint64 total = 0;
    while (total < data.size()) {
        const qint64 n = m_device->write(data.constData() + total, data.size() - total);
        if (n < 0)
            return -1;
        if (n == 0) {
            if (!m_device->waitForBytesWritten(100))
                return -1;
            continue;
        }
        total += n;
    }
    return total;
}

QByteArray Iap2LinkSessionWorker::readBytes(int maxBytes)
{
    if (m_read)
        return m_read(maxBytes);
    if (!m_device || !m_device->isOpen())
        return {};
    if (m_device->bytesAvailable() <= 0) {
        m_device->waitForReadyRead(0);
        if (m_device->bytesAvailable() <= 0)
            return {};
    }
    return m_device->read(qMin(qint64(maxBytes), m_device->bytesAvailable()));
}

Iap2LinkSession::Iap2LinkSession(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Iap2LinkEngine::State>("Iap2LinkEngine::State");
}

Iap2LinkSession::~Iap2LinkSession()
{
    stop();
}

void Iap2LinkSession::setDevice(QIODevice *device, bool takeOwnership)
{
    QMutexLocker lock(&m_mutex);
    m_device = device;
    m_ownDevice = takeOwnership;
    m_write = nullptr;
    m_read = nullptr;
}

void Iap2LinkSession::setCallbacks(WriteCallback write, ReadCallback read)
{
    QMutexLocker lock(&m_mutex);
    m_write = std::move(write);
    m_read = std::move(read);
    m_device = nullptr;
    m_ownDevice = false;
}

void Iap2LinkSession::start(bool wiredInitiator, const Iap2LinkConfig &config)
{
    stop();

    QMutexLocker lock(&m_mutex);
    m_ready = false;
    m_dead = false;
    m_deadReason.clear();
    m_controls.clear();
    m_controlBytes = 0;
    m_engineState = Iap2LinkEngine::State::Idle;

    m_thread = new QThread;
    m_worker = new Iap2LinkSessionWorker(
        Iap2LinkEngine(config),
        wiredInitiator,
        m_device,
        m_ownDevice,
        m_write,
        m_read);
    m_ownDevice = false;
    m_device = nullptr;

    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &Iap2LinkSessionWorker::startPump);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    // Receiver may live on a QThread::create() worker without an event loop.
    // DirectConnection keeps ready/control delivery from depending on that loop.
    connect(m_worker, &Iap2LinkSessionWorker::becameReady, this, &Iap2LinkSession::onWorkerReady,
            Qt::DirectConnection);
    connect(m_worker, &Iap2LinkSessionWorker::becameDead, this, &Iap2LinkSession::onWorkerDead,
            Qt::DirectConnection);
    connect(m_worker, &Iap2LinkSessionWorker::controlReceived, this, &Iap2LinkSession::onWorkerControl,
            Qt::DirectConnection);
    connect(m_worker, &Iap2LinkSessionWorker::sessionReceived, this, &Iap2LinkSession::onWorkerSession,
            Qt::DirectConnection);
    connect(m_worker, &Iap2LinkSessionWorker::stateChanged, this, &Iap2LinkSession::onWorkerState,
            Qt::DirectConnection);

    m_thread->start();
}

void Iap2LinkSession::stop()
{
    Iap2LinkSessionWorker *worker = nullptr;
    QThread *thread = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        worker = m_worker;
        thread = m_thread;
        m_worker = nullptr;
        m_thread = nullptr;
    }
    if (!thread)
        return;
    if (worker)
        QMetaObject::invokeMethod(worker, "stopPump", Qt::QueuedConnection);
    thread->quit();
    thread->wait(3000);
    if (thread->isRunning()) {
        thread->terminate();
        thread->wait(1000);
    }
    delete thread;

    QMutexLocker lock(&m_mutex);
    m_ready = false;
    m_cv.wakeAll();
}

bool Iap2LinkSession::awaitReady(int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    if (m_ready && !m_dead)
        return true;
    if (m_dead)
        return false;
    if (timeoutMs < 0)
        timeoutMs = 0;
    m_cv.wait(&m_mutex, timeoutMs);
    return m_ready && !m_dead;
}

bool Iap2LinkSession::sendControl(const QByteArray &bytes)
{
    if (bytes.size() > Iap2LinkEngine::kMaxPayloadBytes)
        return false;
    QMutexLocker lock(&m_mutex);
    if (m_dead || !m_worker)
        return false;
    Iap2LinkSessionWorker *worker = m_worker;
    lock.unlock();
    bool ok = false;
    if (!QMetaObject::invokeMethod(worker, "enqueueControlAndFlush", Qt::BlockingQueuedConnection,
                                   Q_RETURN_ARG(bool, ok),
                                   Q_ARG(QByteArray, bytes)))
        return false;
    return ok;
}

QByteArray Iap2LinkSession::recvControl(int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    if (timeoutMs < 0)
        timeoutMs = 0;
    if (m_controls.isEmpty() && !m_dead)
        m_cv.wait(&m_mutex, timeoutMs);
    if (m_controls.isEmpty())
        return {};
    const QByteArray bytes = m_controls.dequeue();
    m_controlBytes -= bytes.size();
    return bytes;
}

bool Iap2LinkSession::isReady() const
{
    QMutexLocker lock(&m_mutex);
    return m_ready && !m_dead;
}

bool Iap2LinkSession::isDead() const
{
    QMutexLocker lock(&m_mutex);
    return m_dead;
}

QString Iap2LinkSession::deadReason() const
{
    QMutexLocker lock(&m_mutex);
    return m_deadReason;
}

Iap2LinkEngine::State Iap2LinkSession::engineState() const
{
    QMutexLocker lock(&m_mutex);
    return m_engineState;
}

void Iap2LinkSession::onWorkerReady()
{
    QMutexLocker lock(&m_mutex);
    if (m_ready || m_dead)
        return;
    m_ready = true;
    m_cv.wakeAll();
    lock.unlock();
    emit readyChanged(true);
}

void Iap2LinkSession::onWorkerDead(const QString &reason)
{
    QMutexLocker lock(&m_mutex);
    if (m_dead)
        return;
    m_dead = true;
    m_ready = false;
    m_deadReason = reason;
    m_engineState = Iap2LinkEngine::State::Dead;
    m_cv.wakeAll();
    lock.unlock();
    emit readyChanged(false);
    emit dead(reason);
}

void Iap2LinkSession::onWorkerControl(const QByteArray &bytes)
{
    QMutexLocker lock(&m_mutex);
    if (m_dead)
        return;
    if (m_controls.size() >= kMaxPendingControls
        || m_controlBytes + bytes.size() > kMaxPendingControlBytes) {
        lock.unlock();
        onWorkerDead(QStringLiteral("iAP2 pending control limit exceeded"));
        return;
    }
    m_controls.enqueue(bytes);
    m_controlBytes += bytes.size();
    m_cv.wakeAll();
}

void Iap2LinkSession::onWorkerSession(int sessionId, const QByteArray &bytes)
{
    emit sessionPayload(sessionId, bytes);
}

void Iap2LinkSession::onWorkerState(Iap2LinkEngine::State state)
{
    QMutexLocker lock(&m_mutex);
    m_engineState = state;
}
