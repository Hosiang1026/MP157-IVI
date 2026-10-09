#pragma once

#include "Iap2LinkEngine.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include <functional>
#include <optional>

Q_DECLARE_METATYPE(Iap2LinkEngine::State)

class Iap2LinkSessionWorker;

class Iap2LinkSession : public QObject {
    Q_OBJECT
public:
    using WriteCallback = std::function<qint64(const QByteArray &data)>;
    using ReadCallback = std::function<QByteArray(int maxBytes)>;

    explicit Iap2LinkSession(QObject *parent = nullptr);
    ~Iap2LinkSession() override;

    void setDevice(QIODevice *device, bool takeOwnership = false);
    void setCallbacks(WriteCallback write, ReadCallback read);

    void start(bool wiredInitiator, const Iap2LinkConfig &config = Iap2LinkConfig());
    void stop();

    bool awaitReady(int timeoutMs);
    bool sendControl(const QByteArray &bytes);
    QByteArray recvControl(int timeoutMs);

    bool isReady() const;
    bool isDead() const;
    QString deadReason() const;
    Iap2LinkEngine::State engineState() const;

signals:
    void readyChanged(bool ready);
    void dead(const QString &reason);
    void sessionPayload(int sessionId, const QByteArray &bytes);

private:
    friend class Iap2LinkSessionWorker;

    mutable QMutex m_mutex;
    QWaitCondition m_cv;
    QThread *m_thread = nullptr;
    Iap2LinkSessionWorker *m_worker = nullptr;
    QIODevice *m_device = nullptr;
    bool m_ownDevice = false;
    WriteCallback m_write;
    ReadCallback m_read;
    bool m_ready = false;
    bool m_dead = false;
    QString m_deadReason;
    QQueue<QByteArray> m_controls;
    int m_controlBytes = 0;
    Iap2LinkEngine::State m_engineState = Iap2LinkEngine::State::Idle;

    static constexpr int kMaxPendingControls = 64;
    static constexpr int kMaxPendingControlBytes = 1'048'576;

    void onWorkerReady();
    void onWorkerDead(const QString &reason);
    void onWorkerControl(const QByteArray &bytes);
    void onWorkerSession(int sessionId, const QByteArray &bytes);
    void onWorkerState(Iap2LinkEngine::State state);
};

class Iap2LinkSessionWorker : public QObject {
    Q_OBJECT
public:
    using WriteCallback = Iap2LinkSession::WriteCallback;
    using ReadCallback = Iap2LinkSession::ReadCallback;

    Iap2LinkSessionWorker(Iap2LinkEngine engine,
                          bool wiredInitiator,
                          QIODevice *device,
                          bool ownDevice,
                          WriteCallback write,
                          ReadCallback read);

    ~Iap2LinkSessionWorker() override;

public slots:
    void startPump();
    void stopPump();
    void enqueueControl(const QByteArray &bytes);
    bool enqueueControlAndFlush(const QByteArray &bytes);

signals:
    void becameReady();
    void becameDead(const QString &reason);
    void controlReceived(const QByteArray &bytes);
    void sessionReceived(int sessionId, const QByteArray &bytes);
    void stateChanged(Iap2LinkEngine::State state);

private slots:
    void tick();

private:
    void flushOutput(qint64 now);
    void drainInput(qint64 now);
    void drainEvents();
    void pumpCommands(qint64 now);
    void scheduleNext(qint64 now);
    void terminate(const std::optional<QString> &reason);
    qint64 nowMillis() const;
    qint64 writeBytes(const QByteArray &data);
    QByteArray readBytes(int maxBytes);

    Iap2LinkEngine m_engine;
    bool m_wiredInitiator = false;
    QIODevice *m_device = nullptr;
    bool m_ownDevice = false;
    WriteCallback m_write;
    ReadCallback m_read;
    QTimer *m_timer = nullptr;
    bool m_running = false;
    bool m_terminated = false;

    QMutex m_cmdMutex;
    QQueue<QByteArray> m_commands;
    int m_commandBytes = 0;

    static constexpr int kReceiveChunkBytes = 8192;
    static constexpr int kMaxPendingCommands = 64;
    static constexpr int kMaxPendingCommandBytes = 1'048'576;
    static constexpr int kDefaultPollMs = 100;
};
