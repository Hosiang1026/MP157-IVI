#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QString>

#include <functional>

class BluetoothRfcomm : public QIODevice {
    Q_OBJECT
public:
    static constexpr const char *kIap2Uuid = "00000000-deca-fade-deca-deafdecacafe";
    static constexpr const char *kMapUuid = "00001132-0000-1000-8000-00805f9b34fb";

    using WriteFn = std::function<qint64(const QByteArray &data)>;
    using ReadFn = std::function<QByteArray(int maxBytes)>;

    explicit BluetoothRfcomm(QObject *parent = nullptr);
    ~BluetoothRfcomm() override;

    bool connectTo(const QString &address);
    bool connectToUuid(const QString &address, const QString &serviceUuid);
    void disconnectFromHost();
    QString errorString() const;

    WriteFn writeCallback();
    ReadFn readCallback();

    bool isSequential() const override;
    bool open(OpenMode mode) override;
    void close() override;

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;

private:
    mutable QMutex m_mutex;
    QString m_error;
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    qintptr m_socket = -1;
#endif
#ifdef Q_OS_WIN
    bool m_wsaStarted = false;
#endif
};
