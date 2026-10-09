#include "DlnaRenderer.hpp"

#include "AudioFocus.hpp"

#include <QDateTime>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>

namespace {

const quint16 kSsdpPort = 1900;
const char kSsdpGroup[] = "239.255.255.250";

QByteArray httpOk(const QByteArray &body, const char *contentType)
{
    QByteArray out;
    out += "HTTP/1.1 200 OK\r\n";
    out += "CONTENT-TYPE: ";
    out += contentType;
    out += "\r\n";
    out += "CONTENT-LENGTH: ";
    out += QByteArray::number(body.size());
    out += "\r\n";
    out += "CONNECTION: close\r\n\r\n";
    out += body;
    return out;
}

QByteArray httpNotFound()
{
    return QByteArrayLiteral("HTTP/1.1 404 Not Found\r\nCONTENT-LENGTH: 0\r\nCONNECTION: close\r\n\r\n");
}

} // namespace

DlnaRenderer::DlnaRenderer(AudioFocus *audio, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
    , m_uuid(QStringLiteral("uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_status(QStringLiteral("未启动"))
    , m_transportState(QStringLiteral("STOPPED"))
{
    connect(&m_http, &QTcpServer::newConnection, this, &DlnaRenderer::onHttpNewConnection);
    connect(&m_ssdp, &QUdpSocket::readyRead, this, &DlnaRenderer::onSsdpReadyRead);
    m_announce.setInterval(30000);
    connect(&m_announce, &QTimer::timeout, this, &DlnaRenderer::onAnnounceTick);
    connect(&m_player, &FfmpegUrlPlayer::frameChanged, this, &DlnaRenderer::frameChanged);
    connect(&m_player, &FfmpegUrlPlayer::playingChanged, this, &DlnaRenderer::mediaChanged);
    connect(&m_player, &FfmpegUrlPlayer::errorOccurred, this, [this](const QString &msg) {
        setStatus(QStringLiteral("播放失败"));
        setDetail(msg);
        setTransport(QStringLiteral("STOPPED"));
        if (m_audio)
            m_audio->release(QStringLiteral("dlna"));
    });
    connect(&m_player, &FfmpegUrlPlayer::finished, this, [this] {
        if (m_transportState == QLatin1String("PLAYING")) {
            setTransport(QStringLiteral("STOPPED"));
            setStatus(QStringLiteral("播放结束"));
        }
        if (m_audio)
            m_audio->release(QStringLiteral("dlna"));
        emit mediaChanged();
    });
}

DlnaRenderer::~DlnaRenderer()
{
    stop();
}

void DlnaRenderer::start()
{
    if (m_running)
        return;

    m_hostIp = primaryIpv4();
    emit hostIpChanged();
    if (m_hostIp.isEmpty()) {
        setStatus(QStringLiteral("无网络"));
        setDetail(QStringLiteral("需要局域网 IPv4"));
        return;
    }

    if (!m_http.listen(QHostAddress::AnyIPv4, 0)) {
        setStatus(QStringLiteral("HTTP 失败"));
        setDetail(m_http.errorString());
        return;
    }
    m_httpPort = m_http.serverPort();

    if (!m_ssdp.bind(QHostAddress::AnyIPv4, kSsdpPort,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        m_http.close();
        setStatus(QStringLiteral("SSDP 失败"));
        setDetail(m_ssdp.errorString());
        return;
    }
    m_ssdp.joinMulticastGroup(QHostAddress(QLatin1String(kSsdpGroup)));

    sendSsdpNotify(QStringLiteral("ssdp:alive"));
    m_announce.start();
    setRunning(true);
    setStatus(QStringLiteral("等待投屏"));
    setDetail(QStringLiteral("DLNA 名 MP157-DLNA · %1:%2").arg(m_hostIp).arg(m_httpPort));
}

void DlnaRenderer::stop()
{
    if (m_running)
        sendSsdpNotify(QStringLiteral("ssdp:byebye"));
    m_announce.stop();
    m_ssdp.close();
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        it.key()->disconnect(this);
    m_clients.clear();
    m_http.close();
    m_httpPort = 0;
    stopMedia();
    setRunning(false);
    if (m_status != QStringLiteral("无网络") && m_status != QStringLiteral("HTTP 失败")
        && m_status != QStringLiteral("SSDP 失败"))
        setStatus(QStringLiteral("已停止"));
}

void DlnaRenderer::stopMedia()
{
    m_player.stop();
    m_mediaUri.clear();
    m_mediaTitle.clear();
    m_mediaMeta.clear();
    m_still = QImage();
    setTransport(QStringLiteral("STOPPED"));
    if (m_audio)
        m_audio->release(QStringLiteral("dlna"));
    emit mediaChanged();
    emit frameChanged();
}

void DlnaRenderer::pauseMedia()
{
    m_player.pause();
    setTransport(QStringLiteral("PAUSED_PLAYBACK"));
    setStatus(QStringLiteral("已暂停"));
}

void DlnaRenderer::resumeMedia()
{
    m_player.resume();
    setTransport(QStringLiteral("PLAYING"));
    setStatus(QStringLiteral("播放中"));
}

QImage DlnaRenderer::currentImage() const
{
    const QImage frame = m_player.currentFrame();
    if (!frame.isNull())
        return frame;
    return m_still;
}

void DlnaRenderer::onHttpNewConnection()
{
    while (QTcpSocket *sock = m_http.nextPendingConnection()) {
        m_clients.insert(sock, {});
        connect(sock, &QTcpSocket::readyRead, this, &DlnaRenderer::onHttpReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, [this, sock] {
            m_clients.remove(sock);
            sock->deleteLater();
        });
    }
}

void DlnaRenderer::onHttpReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !m_clients.contains(sock))
        return;
    HttpClient &client = m_clients[sock];
    client.buffer += sock->readAll();
    const int sep = client.buffer.indexOf("\r\n\r\n");
    if (sep < 0)
        return;
    const QByteArray headers = client.buffer.left(sep);
    int contentLength = 0;
    for (const QByteArray &line : headers.split('\n')) {
        const QByteArray t = line.trimmed();
        if (t.toLower().startsWith("content-length:"))
            contentLength = t.mid(15).trimmed().toInt();
    }
    if (client.buffer.size() < sep + 4 + contentLength)
        return;
    const QByteArray req = client.buffer.left(sep + 4 + contentLength);
    client.buffer.remove(0, req.size());
    handleHttp(sock, req);
}

void DlnaRenderer::onSsdpReadyRead()
{
    while (m_ssdp.hasPendingDatagrams()) {
        QByteArray data;
        data.resize(int(m_ssdp.pendingDatagramSize()));
        QHostAddress addr;
        quint16 port = 0;
        m_ssdp.readDatagram(data.data(), data.size(), &addr, &port);
        if (!data.startsWith("M-SEARCH"))
            continue;
        QByteArray st = "ssdp:all";
        for (const QByteArray &line : data.split('\n')) {
            const QByteArray t = line.trimmed();
            if (t.toLower().startsWith("st:"))
                st = t.mid(3).trimmed();
        }
        const QByteArray stLower = st.toLower();
        if (stLower == "ssdp:all" || stLower == "upnp:rootdevice"
            || stLower.contains("mediarenderer") || stLower.contains("avtransport")
            || stLower.contains("connectionmanager") || stLower.contains("renderingcontrol")) {
            replySsdpSearch(addr, port, st);
        }
    }
}

void DlnaRenderer::onAnnounceTick()
{
    if (m_running)
        sendSsdpNotify(QStringLiteral("ssdp:alive"));
}

void DlnaRenderer::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void DlnaRenderer::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void DlnaRenderer::setRunning(bool running)
{
    if (m_running == running)
        return;
    m_running = running;
    emit runningChanged();
}

void DlnaRenderer::setTransport(const QString &state)
{
    if (m_transportState == state)
        return;
    m_transportState = state;
    emit mediaChanged();
}

QString DlnaRenderer::primaryIpv4() const
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return entry.ip().toString();
        }
    }
    return {};
}

void DlnaRenderer::sendSsdpNotify(const QString &nts)
{
    const QString loc = QStringLiteral("http://%1:%2/device.xml").arg(m_hostIp).arg(m_httpPort);
    const QStringList ntsList = {
        QStringLiteral("upnp:rootdevice"),
        QStringLiteral("urn:schemas-upnp-org:device:MediaRenderer:1"),
        m_uuid,
        QStringLiteral("urn:schemas-upnp-org:service:AVTransport:1"),
        QStringLiteral("urn:schemas-upnp-org:service:ConnectionManager:1"),
        QStringLiteral("urn:schemas-upnp-org:service:RenderingControl:1"),
    };
    for (const QString &nt : ntsList) {
        QByteArray pkt;
        pkt += "NOTIFY * HTTP/1.1\r\n";
        pkt += "HOST: 239.255.255.250:1900\r\n";
        pkt += "CACHE-CONTROL: max-age=1800\r\n";
        pkt += "LOCATION: " + loc.toUtf8() + "\r\n";
        pkt += "NT: " + nt.toUtf8() + "\r\n";
        pkt += "NTS: " + nts.toUtf8() + "\r\n";
        pkt += "SERVER: MP157-IVI/1.0 UPnP/1.0 DLNA/1.0\r\n";
        pkt += "USN: " + m_uuid.toUtf8();
        if (nt != m_uuid)
            pkt += "::" + nt.toUtf8();
        pkt += "\r\n\r\n";
        m_ssdp.writeDatagram(pkt, QHostAddress(QLatin1String(kSsdpGroup)), kSsdpPort);
    }
}

void DlnaRenderer::replySsdpSearch(const QHostAddress &addr, quint16 port, const QByteArray &st)
{
    const QString loc = QStringLiteral("http://%1:%2/device.xml").arg(m_hostIp).arg(m_httpPort);
    const QByteArray usnSt = (st.toLower() == "ssdp:all" || st.toLower() == "upnp:rootdevice")
        ? QByteArrayLiteral("upnp:rootdevice")
        : st;
    QByteArray pkt;
    pkt += "HTTP/1.1 200 OK\r\n";
    pkt += "CACHE-CONTROL: max-age=1800\r\n";
    pkt += "DATE: " + QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    pkt += "EXT:\r\n";
    pkt += "LOCATION: " + loc.toUtf8() + "\r\n";
    pkt += "SERVER: MP157-IVI/1.0 UPnP/1.0 DLNA/1.0\r\n";
    pkt += "ST: " + st + "\r\n";
    pkt += "USN: " + m_uuid.toUtf8() + "::" + usnSt + "\r\n";
    pkt += "\r\n";
    m_ssdp.writeDatagram(pkt, addr, port);
}

void DlnaRenderer::handleHttp(QTcpSocket *sock, const QByteArray &req)
{
    const int lineEnd = req.indexOf("\r\n");
    const QByteArray requestLine = lineEnd > 0 ? req.left(lineEnd) : req;
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        sock->write(httpNotFound());
        sock->disconnectFromHost();
        return;
    }
    const QByteArray method = parts[0];
    QByteArray path = parts[1];
    const int q = path.indexOf('?');
    if (q >= 0)
        path = path.left(q);

    if (method == "GET" && path == "/device.xml") {
        sock->write(httpOk(deviceDescription(), "text/xml; charset=\"utf-8\""));
    } else if (method == "GET" && path == "/avtransport.xml") {
        sock->write(httpOk(scpdAvTransport(), "text/xml; charset=\"utf-8\""));
    } else if (method == "GET" && path == "/connectionmanager.xml") {
        sock->write(httpOk(scpdConnectionManager(), "text/xml; charset=\"utf-8\""));
    } else if (method == "GET" && path == "/renderingcontrol.xml") {
        sock->write(httpOk(scpdRenderingControl(), "text/xml; charset=\"utf-8\""));
    } else if (method == "POST"
               && (path == "/avtransport/control" || path == "/connectionmanager/control"
                   || path == "/renderingcontrol/control")) {
        QByteArray action;
        const int sep = req.indexOf("\r\n\r\n");
        for (const QByteArray &line : req.left(sep).split('\n')) {
            const QByteArray t = line.trimmed();
            if (t.toLower().startsWith("soapaction:")) {
                action = t.mid(11).trimmed();
                if (action.startsWith('"'))
                    action = action.mid(1, action.size() - 2);
            }
        }
        const QByteArray body = sep >= 0 ? req.mid(sep + 4) : QByteArray();
        QString service;
        if (path.startsWith("/avtransport"))
            service = QStringLiteral("AVTransport");
        else if (path.startsWith("/connectionmanager"))
            service = QStringLiteral("ConnectionManager");
        else
            service = QStringLiteral("RenderingControl");
        sock->write(httpOk(handleSoap(service, action, body), "text/xml; charset=\"utf-8\""));
    } else {
        sock->write(httpNotFound());
    }
    sock->disconnectFromHost();
}

QByteArray DlnaRenderer::deviceDescription() const
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<device>"
        "<deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>"
        "<friendlyName>MP157-DLNA</friendlyName>"
        "<manufacturer>MP157</manufacturer>"
        "<modelName>MP157-IVI</modelName>"
        "<UDN>%1</UDN>"
        "<serviceList>"
        "<service>"
        "<serviceType>urn:schemas-upnp-org:service:AVTransport:1</serviceType>"
        "<serviceId>urn:upnp-org:serviceId:AVTransport</serviceId>"
        "<SCPDURL>/avtransport.xml</SCPDURL>"
        "<controlURL>/avtransport/control</controlURL>"
        "<eventSubURL>/avtransport/event</eventSubURL>"
        "</service>"
        "<service>"
        "<serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>"
        "<serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>"
        "<SCPDURL>/connectionmanager.xml</SCPDURL>"
        "<controlURL>/connectionmanager/control</controlURL>"
        "<eventSubURL>/connectionmanager/event</eventSubURL>"
        "</service>"
        "<service>"
        "<serviceType>urn:schemas-upnp-org:service:RenderingControl:1</serviceType>"
        "<serviceId>urn:upnp-org:serviceId:RenderingControl</serviceId>"
        "<SCPDURL>/renderingcontrol.xml</SCPDURL>"
        "<controlURL>/renderingcontrol/control</controlURL>"
        "<eventSubURL>/renderingcontrol/event</eventSubURL>"
        "</service>"
        "</serviceList>"
        "</device>"
        "</root>")
                            .arg(m_uuid);
    return xml.toUtf8();
}

QByteArray DlnaRenderer::scpdAvTransport() const
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<actionList>"
        "<action><name>SetAVTransportURI</name></action>"
        "<action><name>Play</name></action>"
        "<action><name>Pause</name></action>"
        "<action><name>Stop</name></action>"
        "<action><name>GetTransportInfo</name></action>"
        "<action><name>GetMediaInfo</name></action>"
        "<action><name>GetPositionInfo</name></action>"
        "</actionList>"
        "<serviceStateTable></serviceStateTable>"
        "</scpd>");
}

QByteArray DlnaRenderer::scpdConnectionManager() const
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<actionList><action><name>GetProtocolInfo</name></action></actionList>"
        "<serviceStateTable></serviceStateTable>"
        "</scpd>");
}

QByteArray DlnaRenderer::scpdRenderingControl() const
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<actionList>"
        "<action><name>GetVolume</name></action>"
        "<action><name>SetVolume</name></action>"
        "<action><name>GetMute</name></action>"
        "<action><name>SetMute</name></action>"
        "</actionList>"
        "<serviceStateTable></serviceStateTable>"
        "</scpd>");
}

QByteArray DlnaRenderer::soapResponse(const QString &service, const QString &action,
                                      const QString &body) const
{
    const QString xml = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body>"
        "<u:%1Response xmlns:u=\"urn:schemas-upnp-org:service:%2:1\">"
        "%3"
        "</u:%1Response>"
        "</s:Body>"
        "</s:Envelope>");
    return xml.arg(action, service, body).toUtf8();
}

QString DlnaRenderer::xmlText(const QByteArray &xml, const char *tag)
{
    const QByteArray open = QByteArray("<") + tag + ">";
    const QByteArray close = QByteArray("</") + tag + ">";
    int a = xml.indexOf(open);
    if (a < 0) {
        const QByteArray open2 = QByteArray("<") + tag + " ";
        a = xml.indexOf(open2);
        if (a < 0)
            return {};
        a = xml.indexOf('>', a);
        if (a < 0)
            return {};
        ++a;
    } else {
        a += open.size();
    }
    const int b = xml.indexOf(close, a);
    if (b < 0)
        return {};
    return QString::fromUtf8(xml.mid(a, b - a)).trimmed();
}

QByteArray DlnaRenderer::handleSoap(const QString &service, const QByteArray &action,
                                    const QByteArray &body)
{
    QString name = QString::fromUtf8(action);
    const int hash = name.lastIndexOf(QLatin1Char('#'));
    if (hash >= 0)
        name = name.mid(hash + 1);

    if (service == QLatin1String("ConnectionManager") && name == QLatin1String("GetProtocolInfo")) {
        return soapResponse(service, name,
                            QStringLiteral("<Source></Source>"
                                           "<Sink>http-get:*:image/jpeg:*,http-get:*:image/png:*,"
                                           "http-get:*:image/gif:*,http-get:*:audio/mpeg:*,"
                                           "http-get:*:audio/wav:*,http-get:*:video/mp4:*</Sink>"));
    }
    if (service == QLatin1String("RenderingControl")) {
        if (name == QLatin1String("GetVolume"))
            return soapResponse(service, name,
                                QStringLiteral("<CurrentVolume>%1</CurrentVolume>").arg(m_volume));
        if (name == QLatin1String("SetVolume")) {
            m_volume = xmlText(body, "DesiredVolume").toInt();
            return soapResponse(service, name, {});
        }
        if (name == QLatin1String("GetMute"))
            return soapResponse(service, name, QStringLiteral("<CurrentMute>0</CurrentMute>"));
        if (name == QLatin1String("SetMute"))
            return soapResponse(service, name, {});
    }
    if (service == QLatin1String("AVTransport")) {
        if (name == QLatin1String("SetAVTransportURI")) {
            applyUri(xmlText(body, "CurrentURI"), xmlText(body, "CurrentURIMetaData"));
            return soapResponse(service, name, {});
        }
        if (name == QLatin1String("Play")) {
            if (!m_mediaUri.isEmpty()) {
                if (m_transportState == QLatin1String("PAUSED_PLAYBACK") && m_player.playing())
                    resumeMedia();
                else
                    fetchMedia(QUrl(m_mediaUri));
            }
            return soapResponse(service, name, {});
        }
        if (name == QLatin1String("Pause")) {
            pauseMedia();
            return soapResponse(service, name, {});
        }
        if (name == QLatin1String("Stop")) {
            stopMedia();
            setStatus(QStringLiteral("等待投屏"));
            return soapResponse(service, name, {});
        }
        if (name == QLatin1String("GetTransportInfo")) {
            return soapResponse(service, name,
                                QStringLiteral("<CurrentTransportState>%1</CurrentTransportState>"
                                               "<CurrentTransportStatus>OK</CurrentTransportStatus>"
                                               "<CurrentSpeed>1</CurrentSpeed>")
                                    .arg(m_transportState));
        }
        if (name == QLatin1String("GetMediaInfo")) {
            return soapResponse(service, name,
                                QStringLiteral("<NrTracks>1</NrTracks>"
                                               "<MediaDuration>00:00:00</MediaDuration>"
                                               "<CurrentURI>%1</CurrentURI>"
                                               "<CurrentURIMetaData></CurrentURIMetaData>"
                                               "<NextURI></NextURI>"
                                               "<NextURIMetaData></NextURIMetaData>"
                                               "<PlayMedium>NETWORK</PlayMedium>"
                                               "<RecordMedium>NOT_IMPLEMENTED</RecordMedium>"
                                               "<WriteStatus>NOT_IMPLEMENTED</WriteStatus>")
                                    .arg(m_mediaUri.toHtmlEscaped()));
        }
        if (name == QLatin1String("GetPositionInfo")) {
            return soapResponse(service, name,
                                QStringLiteral("<Track>1</Track>"
                                               "<TrackDuration>00:00:00</TrackDuration>"
                                               "<TrackMetaData></TrackMetaData>"
                                               "<TrackURI>%1</TrackURI>"
                                               "<RelTime>00:00:00</RelTime>"
                                               "<AbsTime>00:00:00</AbsTime>"
                                               "<RelCount>0</RelCount>"
                                               "<AbsCount>0</AbsCount>")
                                    .arg(m_mediaUri.toHtmlEscaped()));
        }
    }
    return soapResponse(service, name, {});
}

void DlnaRenderer::applyUri(const QString &uri, const QString &meta)
{
    m_player.stop();
    m_mediaUri = uri.trimmed();
    m_mediaMeta = meta;
    m_mediaTitle = xmlText(meta.toUtf8(), "dc:title");
    if (m_mediaTitle.isEmpty())
        m_mediaTitle = QUrl(m_mediaUri).fileName();
    if (m_mediaTitle.isEmpty())
        m_mediaTitle = QStringLiteral("投屏媒体");
    m_still = QImage();
    setTransport(QStringLiteral("STOPPED"));
    setDetail(m_mediaTitle);
    emit mediaChanged();
    emit frameChanged();
}

void DlnaRenderer::beginPlayback(const QUrl &url)
{
    setTransport(QStringLiteral("PLAYING"));
    setStatus(QStringLiteral("播放中"));
    setDetail(m_mediaTitle);
    if (m_audio)
        m_audio->request(QStringLiteral("dlna"), m_audio->mediaPriority());
    m_player.play(url);
    emit mediaChanged();
}

void DlnaRenderer::fetchMedia(const QUrl &url)
{
    if (!url.isValid())
        return;

    const QString path = url.path().toLower();
    if (path.endsWith(QLatin1String(".jpg")) || path.endsWith(QLatin1String(".jpeg"))
        || path.endsWith(QLatin1String(".png")) || path.endsWith(QLatin1String(".gif"))
        || path.endsWith(QLatin1String(".bmp")) || path.endsWith(QLatin1String(".webp"))) {
        QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                beginPlayback(url);
                return;
            }
            QImage img;
            if (!img.loadFromData(reply->readAll())) {
                beginPlayback(url);
                return;
            }
            m_player.stop();
            m_still = img;
            setTransport(QStringLiteral("PLAYING"));
            setStatus(QStringLiteral("图片投屏"));
            setDetail(m_mediaTitle);
            emit mediaChanged();
            emit frameChanged();
        });
        return;
    }

    beginPlayback(url);
}
