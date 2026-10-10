#include "AndroidAutoSession.hpp"

AndroidAutoSession::AndroidAutoSession(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("未启动"))
    , m_detail(QStringLiteral("有线 AOAP"))
{
    m_poll.setInterval(200);
    connect(&m_poll, &QTimer::timeout, this, &AndroidAutoSession::onPoll);
    connect(&m_aoap, &AoapTransport::stateChanged, this, &AndroidAutoSession::aoapStateChanged);
    connect(&m_aoap, &AoapTransport::deviceLabelChanged, this, &AndroidAutoSession::deviceLabelChanged);
    connect(&m_aoap, &AoapTransport::detailChanged, this, [this] {
        if (m_running)
            setDetail(m_aoap.detail());
    });
    connect(&m_aoap, &AoapTransport::accessoryReady, this, &AndroidAutoSession::onAoapReady);
    connect(&m_aoap, &AoapTransport::failed, this, &AndroidAutoSession::onAoapFailed);
    connect(&m_aoap, &AoapTransport::bulkActivity, this, [this](int in, int out) {
        if (in > 0)
            m_bytesIn += in;
        if (out > 0)
            m_bytesOut += out;
        emit trafficChanged();
    });
}

void AndroidAutoSession::start()
{
    if (m_running)
        return;
    m_bytesIn = 0;
    m_bytesOut = 0;
    emit trafficChanged();
    setRunning(true);
    setStatus(QStringLiteral("监听 USB"));
    setDetail(QStringLiteral("等待手机插入"));
    m_aoap.startWatch();
}

void AndroidAutoSession::stop()
{
    if (!m_running && !m_sessionOpen)
        return;
    closeSession();
    m_aoap.stop();
    setRunning(false);
    setStatus(QStringLiteral("已停止"));
    setDetail({});
}

void AndroidAutoSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void AndroidAutoSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void AndroidAutoSession::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

void AndroidAutoSession::onAoapReady(const QString &deviceLabel)
{
    setStatus(QStringLiteral("附件就绪"));
    setDetail(deviceLabel);
    openLinkProbe();
}

void AndroidAutoSession::onAoapFailed(const QString &reason)
{
    closeSession();
    setStatus(QStringLiteral("失败"));
    setDetail(reason);
    setRunning(false);
}

void AndroidAutoSession::openLinkProbe()
{
    if (m_sessionOpen)
        return;
    m_sessionOpen = true;
    setStatus(QStringLiteral("链路探测"));
    if (qEnvironmentVariableIsSet("IVI_AA_DEMO")) {
        setDetail(QStringLiteral("演示：AOAP 完成 · TLS/视频通道未接"));
        return;
    }
    setDetail(QStringLiteral("批量端点已开 · 等待手机数据（TLS 未实现）"));
    m_poll.start();
}

void AndroidAutoSession::closeSession()
{
    m_poll.stop();
    m_sessionOpen = false;
}

void AndroidAutoSession::onPoll()
{
    if (!m_sessionOpen || !m_aoap.accessoryOpen())
        return;
    const QByteArray chunk = m_aoap.readBulk(4096, 50);
    if (chunk.isEmpty())
        return;
    setStatus(QStringLiteral("收到数据"));
    setDetail(QStringLiteral("IN %1 B · 累计 %2/%3（需 TLS 会话）")
                  .arg(chunk.size())
                  .arg(m_bytesIn)
                  .arg(m_bytesOut));
}
