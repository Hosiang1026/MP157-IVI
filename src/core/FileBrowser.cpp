#include "FileBrowser.hpp"

#include "MediaSession.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSet>
#include <QStorageInfo>
#include <QUrl>
#include <QVariantMap>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QByteArray httpResponse(int code, const char *reason, const QByteArray &body, const char *type,
                        const char *extra = nullptr)
{
    QByteArray out;
    out += "HTTP/1.1 ";
    out += QByteArray::number(code);
    out += ' ';
    out += reason;
    out += "\r\nContent-Type: ";
    out += type;
    out += "\r\nContent-Length: ";
    out += QByteArray::number(body.size());
    out += "\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n";
    if (extra)
        out += extra;
    out += "\r\n";
    out += body;
    return out;
}

QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
}

}

FileBrowser::FileBrowser(MediaSession *media, QObject *parent)
    : QObject(parent)
    , m_media(media)
    , m_shareStatus(QStringLiteral("未开启"))
{
    rebuildRoots();
    connect(&m_http, &QTcpServer::newConnection, this, &FileBrowser::onNewConnection);
    openRoot(QStringLiteral("inbox"));
}

FileBrowser::~FileBrowser()
{
    stopShare();
}

void FileBrowser::ensureLocalDirs() const
{
    for (const Root &r : m_rootList) {
        if (!r.removable)
            QDir().mkpath(r.absPath);
    }
}

void FileBrowser::rebuildRoots()
{
    const QString base = QCoreApplication::applicationDirPath();
    const QString keepId = m_rootId;
    m_rootList = {
        {QStringLiteral("music"), QStringLiteral("音乐"),
         QDir(base).filePath(QStringLiteral("media/music")), false},
        {QStringLiteral("video"), QStringLiteral("视频"),
         QDir(base).filePath(QStringLiteral("media/video")), false},
        {QStringLiteral("pictures"), QStringLiteral("图片"),
         QDir(base).filePath(QStringLiteral("media/pictures")), false},
        {QStringLiteral("recordings"), QStringLiteral("录像"),
         QDir(base).filePath(QStringLiteral("recordings")), false},
        {QStringLiteral("inbox"), QStringLiteral("无线接收"),
         QDir(base).filePath(QStringLiteral("media/inbox")), false},
    };

#ifdef Q_OS_WIN
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0)
            continue;
        const wchar_t rootPath[] = {wchar_t(L'A' + i), L':', L'\\', 0};
        if (GetDriveTypeW(rootPath) != DRIVE_REMOVABLE)
            continue;
        const QString letter = QString(QChar('A' + i));
        const QString path = letter + QStringLiteral(":/");
        if (!QDir(path).exists())
            continue;
        m_rootList.push_back({QStringLiteral("usb_") + letter, QStringLiteral("U盘 ") + letter,
                              QDir(path).absolutePath(), true});
    }
#else
    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    QStringList mountBases = {
        QStringLiteral("/media/") + user,
        QStringLiteral("/run/media/") + user,
        QStringLiteral("/mnt"),
    };
    QSet<QString> seen;
    for (const QString &baseMount : mountBases) {
        QDir dir(baseMount);
        if (!dir.exists())
            continue;
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &info : entries) {
            const QString abs = info.absoluteFilePath();
            if (seen.contains(abs))
                continue;
            seen.insert(abs);
            m_rootList.push_back({QStringLiteral("usb_") + info.fileName(),
                                  QStringLiteral("U盘 ") + info.fileName(), abs, true});
        }
    }
    for (const QStorageInfo &vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isValid() || !vol.isReady() || vol.isReadOnly())
            continue;
        const QString root = vol.rootPath();
        if (root == QLatin1String("/") || seen.contains(root))
            continue;
        const QString fs = QString::fromUtf8(vol.fileSystemType()).toLower();
        if (!(fs.contains(QLatin1String("vfat")) || fs.contains(QLatin1String("fat"))
              || fs.contains(QLatin1String("exfat")) || fs.contains(QLatin1String("ntfs"))))
            continue;
        seen.insert(root);
        const QString name = vol.name().isEmpty() ? QFileInfo(root).fileName() : vol.name();
        m_rootList.push_back(
            {QStringLiteral("usb_") + name, QStringLiteral("U盘 ") + name, root, true});
    }
#endif

    ensureLocalDirs();
    m_roots.clear();
    for (const Root &r : m_rootList) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), r.id);
        m.insert(QStringLiteral("name"), r.name);
        m.insert(QStringLiteral("removable"), r.removable);
        m_roots.push_back(m);
    }
    emit rootsChanged();

    bool still = false;
    for (const Root &r : m_rootList) {
        if (r.id == keepId) {
            still = true;
            break;
        }
    }
    if (!still && !keepId.isEmpty())
        openRoot(QStringLiteral("inbox"));
}

QString FileBrowser::rootName() const
{
    for (const Root &r : m_rootList) {
        if (r.id == m_rootId)
            return r.name;
    }
    return {};
}

QString FileBrowser::rootAbs() const
{
    return rootAbsById(m_rootId);
}

QString FileBrowser::rootAbsById(const QString &id) const
{
    for (const Root &r : m_rootList) {
        if (r.id == id)
            return QDir(r.absPath).absolutePath();
    }
    return {};
}

QString FileBrowser::inboxAbs() const
{
    return rootAbsById(QStringLiteral("inbox"));
}

bool FileBrowser::isUsbRoot(const QString &id) const
{
    return id.startsWith(QLatin1String("usb_"));
}

QString FileBrowser::absOf(const QString &rel) const
{
    const QString root = rootAbs();
    if (root.isEmpty())
        return {};
    if (rel.isEmpty() || rel == QLatin1String("/"))
        return QDir(root).absolutePath();
    QString clean = rel;
    clean.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (clean.startsWith(QLatin1Char('/')))
        clean.remove(0, 1);
    return QDir::cleanPath(QDir(root).absoluteFilePath(clean));
}

bool FileBrowser::underRoot(const QString &abs) const
{
    const QString root = rootAbs();
    const QString path = QDir::cleanPath(abs);
    return !root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')));
}

bool FileBrowser::underInbox(const QString &abs) const
{
    const QString root = inboxAbs();
    const QString path = QDir::cleanPath(abs);
    return !root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')));
}

QString FileBrowser::shareUrl() const
{
    if (!m_shareRunning || m_hostIp.isEmpty())
        return {};
    return QStringLiteral("http://%1:%2/").arg(m_hostIp).arg(m_sharePort);
}

void FileBrowser::setShareStatus(const QString &status)
{
    if (m_shareStatus == status)
        return;
    m_shareStatus = status;
    emit shareChanged();
}

void FileBrowser::setLastEvent(const QString &event)
{
    m_lastEvent = event;
    emit lastEventChanged();
}

QString FileBrowser::kindOf(const QString &fileName)
{
    const QString s = QFileInfo(fileName).suffix().toLower();
    if (s == QLatin1String("wav"))
        return QStringLiteral("music");
    if (s == QLatin1String("avi") || s == QLatin1String("mp4") || s == QLatin1String("mov")
        || s == QLatin1String("mkv") || s == QLatin1String("webm"))
        return QStringLiteral("video");
    if (s == QLatin1String("jpg") || s == QLatin1String("jpeg") || s == QLatin1String("png")
        || s == QLatin1String("webp") || s == QLatin1String("bmp") || s == QLatin1String("gif"))
        return QStringLiteral("image");
    return {};
}

QString FileBrowser::uniquePath(const QString &dir, const QString &fileName) const
{
    QString dest = QDir(dir).filePath(fileName);
    if (!QFile::exists(dest))
        return dest;
    const QString stem = QFileInfo(fileName).completeBaseName();
    const QString suffix = QFileInfo(fileName).suffix();
    return QDir(dir).filePath(stem + QLatin1Char('_')
                              + QString::number(QDateTime::currentSecsSinceEpoch())
                              + (suffix.isEmpty() ? QString() : (QLatin1Char('.') + suffix)));
}

void FileBrowser::openRoot(const QString &id)
{
    if (rootAbsById(id).isEmpty())
        return;
    m_rootId = id;
    m_rel.clear();
    ensureLocalDirs();
    reloadEntries();
    emit pathChanged();
}

void FileBrowser::enter(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || name == QLatin1String("..") || name == QLatin1String("."))
        return;
    const QString nextRel = m_rel.isEmpty() ? name : (m_rel + QLatin1Char('/') + name);
    const QString abs = absOf(nextRel);
    if (!underRoot(abs) || !QFileInfo(abs).isDir())
        return;
    m_rel = nextRel;
    reloadEntries();
    emit pathChanged();
}

void FileBrowser::goUp()
{
    if (m_rel.isEmpty())
        return;
    const int slash = m_rel.lastIndexOf(QLatin1Char('/'));
    m_rel = slash >= 0 ? m_rel.left(slash) : QString();
    reloadEntries();
    emit pathChanged();
}

void FileBrowser::refresh()
{
    rebuildRoots();
    if (!rootAbsById(m_rootId).isEmpty()) {
        reloadEntries();
        emit pathChanged();
    }
}

bool FileBrowser::removeEntry(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String(".."))
        return false;
    const QString abs = absOf(m_rel.isEmpty() ? name : (m_rel + QLatin1Char('/') + name));
    if (!underRoot(abs))
        return false;
    QFileInfo info(abs);
    bool ok = false;
    if (info.isDir())
        ok = QDir(abs).removeRecursively();
    else
        ok = QFile::remove(abs);
    if (ok) {
        setLastEvent(QStringLiteral("已删除 %1").arg(name));
        reloadEntries();
        if (m_media && (m_rootId == QLatin1String("music") || m_rootId == QLatin1String("inbox")))
            m_media->rescan();
    }
    return ok;
}

QVariantList FileBrowser::copyTargets(const QString &name) const
{
    QVariantList out;
    if (name.isEmpty() || name.contains(QLatin1Char('/')))
        return out;
    const QString abs = absOf(m_rel.isEmpty() ? name : (m_rel + QLatin1Char('/') + name));
    if (!underRoot(abs) || QFileInfo(abs).isDir())
        return out;

    const QString kind = kindOf(name);
    auto add = [&](const QString &id, const QString &label) {
        if (id == m_rootId && m_rel.isEmpty())
            return;
        if (rootAbsById(id).isEmpty())
            return;
        QVariantMap m;
        m.insert(QStringLiteral("id"), id);
        m.insert(QStringLiteral("name"), label);
        out.push_back(m);
    };

    if (isUsbRoot(m_rootId)) {
        add(QStringLiteral("inbox"), QStringLiteral("无线接收"));
        if (kind == QLatin1String("music"))
            add(QStringLiteral("music"), QStringLiteral("音乐"));
        if (kind == QLatin1String("video")) {
            add(QStringLiteral("video"), QStringLiteral("视频"));
            add(QStringLiteral("recordings"), QStringLiteral("录像"));
        }
        if (kind == QLatin1String("image"))
            add(QStringLiteral("pictures"), QStringLiteral("图片"));
    } else {
        for (const Root &r : m_rootList) {
            if (r.removable)
                add(r.id, r.name);
        }
        if (kind == QLatin1String("music") && m_rootId != QLatin1String("music"))
            add(QStringLiteral("music"), QStringLiteral("音乐"));
        if (kind == QLatin1String("video")) {
            if (m_rootId != QLatin1String("video"))
                add(QStringLiteral("video"), QStringLiteral("视频"));
            if (m_rootId != QLatin1String("recordings"))
                add(QStringLiteral("recordings"), QStringLiteral("录像"));
        }
        if (kind == QLatin1String("image") && m_rootId != QLatin1String("pictures"))
            add(QStringLiteral("pictures"), QStringLiteral("图片"));
        if (m_rootId != QLatin1String("inbox"))
            add(QStringLiteral("inbox"), QStringLiteral("无线接收"));
    }
    return out;
}

bool FileBrowser::copyEntry(const QString &name, const QString &destRootId)
{
    if (name.isEmpty() || destRootId.isEmpty())
        return false;
    const QString src = absOf(m_rel.isEmpty() ? name : (m_rel + QLatin1Char('/') + name));
    if (!underRoot(src) || QFileInfo(src).isDir())
        return false;
    const QString destDir = rootAbsById(destRootId);
    if (destDir.isEmpty())
        return false;
    QDir().mkpath(destDir);
    const QString dest = uniquePath(destDir, safeFileName(name));
    if (!QFile::copy(src, dest)) {
        setLastEvent(QStringLiteral("拷贝失败 %1").arg(name));
        return false;
    }
    setLastEvent(QStringLiteral("已拷贝 %1 → %2")
                     .arg(QFileInfo(dest).fileName(),
                          [&] {
                              for (const Root &r : m_rootList) {
                                  if (r.id == destRootId)
                                      return r.name;
                              }
                              return destRootId;
                          }()));
    if (m_media && destRootId == QLatin1String("music"))
        m_media->rescan();
    if (destRootId == m_rootId)
        reloadEntries();
    return true;
}

bool FileBrowser::openEntry(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')))
        return false;
    const QString abs = absOf(m_rel.isEmpty() ? name : (m_rel + QLatin1Char('/') + name));
    if (!underRoot(abs))
        return false;
    QFileInfo info(abs);
    if (info.isDir()) {
        enter(name);
        return true;
    }

    const QString kind = kindOf(name);
    if (kind == QLatin1String("music")) {
        const QString musicDir = rootAbsById(QStringLiteral("music"));
        QString playPath = abs;
        if (QDir::cleanPath(info.absolutePath()) != QDir::cleanPath(musicDir)) {
            QDir().mkpath(musicDir);
            const QString same = QDir(musicDir).filePath(safeFileName(name));
            if (QFile::exists(same)) {
                playPath = same;
            } else {
                playPath = uniquePath(musicDir, safeFileName(name));
                if (!QFile::copy(abs, playPath)) {
                    setLastEvent(QStringLiteral("无法复制到音乐库"));
                    return false;
                }
            }
            setLastEvent(QStringLiteral("已加入音乐库"));
        }
        if (m_media) {
            m_media->rescan();
            m_media->playFile(playPath);
        }
        emit requestOpenApp(QStringLiteral("music"));
        return true;
    }

    if (kind == QLatin1String("video")) {
        const QString videoDir = rootAbsById(QStringLiteral("video"));
        QString clip = info.completeBaseName();
        QString dest = QDir(videoDir).filePath(clip + QStringLiteral(".avi"));
        if (QDir::cleanPath(info.absolutePath()) != QDir::cleanPath(videoDir)) {
            QDir().mkpath(videoDir);
            if (!QFile::exists(dest)) {
                if (!QFile::copy(abs, dest)) {
                    dest = uniquePath(videoDir, safeFileName(name));
                    if (!QFile::copy(abs, dest)) {
                        setLastEvent(QStringLiteral("无法复制到视频库"));
                        return false;
                    }
                }
            }
            clip = QFileInfo(dest).completeBaseName();
            setLastEvent(QStringLiteral("已加入视频库"));
        }
        if (m_pendingVideo != clip) {
            m_pendingVideo = clip;
            emit pendingVideoChanged();
        }
        emit requestOpenApp(QStringLiteral("video"));
        return true;
    }

    setLastEvent(QStringLiteral("仅支持 WAV / AVI 播放"));
    return false;
}

void FileBrowser::clearPendingVideo()
{
    if (m_pendingVideo.isEmpty())
        return;
    m_pendingVideo.clear();
    emit pendingVideoChanged();
}

void FileBrowser::reloadEntries()
{
    m_entries.clear();
    const QString abs = absOf(m_rel);
    QDir dir(abs);
    if (!dir.exists()) {
        emit entriesChanged();
        return;
    }
    const auto infos = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                         QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &info : infos) {
        QVariantMap item;
        const QString kind = info.isDir() ? QString() : kindOf(info.fileName());
        item.insert(QStringLiteral("name"), info.fileName());
        item.insert(QStringLiteral("dir"), info.isDir());
        item.insert(QStringLiteral("size"), info.isDir() ? QVariant() : info.size());
        item.insert(QStringLiteral("sizeText"), info.isDir() ? QStringLiteral("文件夹")
                                                            : formatSize(info.size()));
        item.insert(QStringLiteral("mtime"),
                    info.lastModified().toString(QStringLiteral("MM-dd hh:mm")));
        item.insert(QStringLiteral("playable"),
                    kind == QLatin1String("music") || kind == QLatin1String("video"));
        item.insert(QStringLiteral("kind"), kind);
        m_entries.push_back(item);
    }
    emit entriesChanged();
}

QString FileBrowser::primaryIpv4() const
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

void FileBrowser::startShare()
{
    if (m_shareRunning)
        return;
    m_hostIp = primaryIpv4();
    if (m_hostIp.isEmpty()) {
        setShareStatus(QStringLiteral("无局域网 IP"));
        emit shareChanged();
        return;
    }
    m_sharePort = 8787;
    if (!m_http.listen(QHostAddress::AnyIPv4, m_sharePort)) {
        if (!m_http.listen(QHostAddress::AnyIPv4, 0)) {
            setShareStatus(m_http.errorString());
            emit shareChanged();
            return;
        }
        m_sharePort = m_http.serverPort();
    }
    m_shareRunning = true;
    setShareStatus(QStringLiteral("手机同 Wi‑Fi 打开下方地址"));
    setLastEvent(QStringLiteral("无线传文件已开启"));
    emit shareChanged();
}

void FileBrowser::stopShare()
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        it.key()->disconnect(this);
    m_clients.clear();
    m_http.close();
    m_shareRunning = false;
    setShareStatus(QStringLiteral("未开启"));
    emit shareChanged();
}

void FileBrowser::onNewConnection()
{
    while (QTcpSocket *sock = m_http.nextPendingConnection()) {
        m_clients.insert(sock, {});
        connect(sock, &QTcpSocket::readyRead, this, &FileBrowser::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, [this, sock] {
            m_clients.remove(sock);
            sock->deleteLater();
        });
    }
}

void FileBrowser::onReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !m_clients.contains(sock))
        return;
    Client &c = m_clients[sock];
    c.buffer += sock->readAll();
    if (!c.headerDone) {
        const int sep = c.buffer.indexOf("\r\n\r\n");
        if (sep < 0)
            return;
        c.headerDone = true;
        const QByteArray header = c.buffer.left(sep);
        qint64 contentLength = 0;
        for (const QByteArray &line : header.split('\n')) {
            const QByteArray l = line.trimmed();
            if (l.toLower().startsWith("content-length:"))
                contentLength = l.mid(15).trimmed().toLongLong();
        }
        c.need = sep + 4 + contentLength;
    }
    if (c.need >= 0 && c.buffer.size() < c.need)
        return;
    const QByteArray raw = c.need >= 0 ? c.buffer.left(int(c.need)) : c.buffer;
    m_clients.remove(sock);
    handleRequest(sock, raw);
}

void FileBrowser::handleRequest(QTcpSocket *sock, const QByteArray &raw)
{
    const int sep = raw.indexOf("\r\n\r\n");
    if (sep < 0) {
        sock->write(httpResponse(400, "Bad Request", QByteArrayLiteral("bad request"),
                                 "text/plain; charset=utf-8"));
        sock->disconnectFromHost();
        return;
    }
    const QByteArray header = raw.left(sep);
    const QByteArray body = raw.mid(sep + 4);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty()) {
        sock->disconnectFromHost();
        return;
    }
    const QList<QByteArray> req = lines.first().trimmed().split(' ');
    if (req.size() < 2) {
        sock->disconnectFromHost();
        return;
    }
    const QByteArray method = req.at(0);
    QString path = QUrl::fromPercentEncoding(req.at(1));
    const int q = path.indexOf(QLatin1Char('?'));
    if (q >= 0)
        path = path.left(q);

    if (method == "GET" && (path == QLatin1String("/") || path == QLatin1String("/index.html"))) {
        sock->write(httpResponse(200, "OK", pageHtml(), "text/html; charset=utf-8"));
    } else if (method == "GET" && path == QLatin1String("/api/list")) {
        sock->write(httpResponse(200, "OK", listJson(), "application/json; charset=utf-8"));
    } else if (method == "GET" && path.startsWith(QLatin1String("/dl/"))) {
        const QString name = safeFileName(QUrl::fromPercentEncoding(path.mid(4).toUtf8()));
        const QString abs = QDir(inboxAbs()).filePath(name);
        if (!underInbox(abs) || !QFileInfo::exists(abs) || QFileInfo(abs).isDir()) {
            sock->write(httpResponse(404, "Not Found", QByteArrayLiteral("not found"),
                                     "text/plain; charset=utf-8"));
        } else {
            QFile file(abs);
            if (!file.open(QIODevice::ReadOnly)) {
                sock->write(httpResponse(500, "Error", QByteArrayLiteral("read fail"),
                                         "text/plain; charset=utf-8"));
            } else {
                const QByteArray data = file.readAll();
                QByteArray disp = "Content-Disposition: attachment; filename=\"";
                disp += name.toUtf8();
                disp += "\"\r\n";
                sock->write(
                    httpResponse(200, "OK", data, "application/octet-stream", disp.constData()));
            }
        }
    } else if (method == "POST" && path == QLatin1String("/upload")) {
        QByteArray boundary;
        for (const QByteArray &line : lines) {
            const QByteArray l = line.trimmed();
            if (!l.toLower().startsWith("content-type:"))
                continue;
            const int b = l.indexOf("boundary=");
            if (b >= 0)
                boundary = l.mid(b + 9).trimmed();
        }
        QString saved;
        if (boundary.isEmpty() || !saveUpload(body, boundary, &saved)) {
            sock->write(httpResponse(400, "Bad Request", QByteArrayLiteral("{\"ok\":false}"),
                                     "application/json; charset=utf-8"));
        } else {
            setLastEvent(QStringLiteral("收到 %1").arg(saved));
            reloadEntries();
            if (m_media)
                m_media->rescan();
            const QByteArray json =
                QByteArray("{\"ok\":true,\"name\":\"") + saved.toUtf8() + "\"}";
            sock->write(httpResponse(200, "OK", json, "application/json; charset=utf-8"));
        }
    } else {
        sock->write(httpResponse(404, "Not Found", QByteArrayLiteral("not found"),
                                 "text/plain; charset=utf-8"));
    }
    sock->disconnectFromHost();
}

QString FileBrowser::safeFileName(const QString &name) const
{
    QString n = QFileInfo(name).fileName();
    n.replace(QLatin1Char('/'), QLatin1Char('_'));
    n.replace(QLatin1Char('\\'), QLatin1Char('_'));
    n.remove(QLatin1Char('\0'));
    if (n.isEmpty() || n == QLatin1String(".") || n == QLatin1String(".."))
        n = QStringLiteral("file.bin");
    return n;
}

bool FileBrowser::saveUpload(const QByteArray &body, const QByteArray &boundary, QString *savedName)
{
    const QByteArray mark = "--" + boundary;
    int pos = body.indexOf(mark);
    if (pos < 0)
        return false;
    pos += mark.size();
    if (body.mid(pos, 2) == "--")
        return false;
    if (body.mid(pos, 2) == "\r\n")
        pos += 2;
    const int next = body.indexOf("\r\n" + mark, pos);
    if (next < 0)
        return false;
    const QByteArray part = body.mid(pos, next - pos);
    const int hEnd = part.indexOf("\r\n\r\n");
    if (hEnd < 0)
        return false;
    const QByteArray partHeader = part.left(hEnd);
    QByteArray fileBody = part.mid(hEnd + 4);
    if (fileBody.endsWith("\r\n"))
        fileBody.chop(2);

    QString fileName;
    for (const QByteArray &line : partHeader.split('\n')) {
        const QByteArray l = line.trimmed();
        if (!l.toLower().startsWith("content-disposition:"))
            continue;
        const int fn = l.indexOf("filename=\"");
        if (fn >= 0) {
            const int start = fn + 10;
            const int end = l.indexOf('"', start);
            if (end > start)
                fileName = QString::fromUtf8(l.mid(start, end - start));
        }
    }
    fileName = safeFileName(fileName);
    const QString kind = kindOf(fileName);
    QString destRoot = inboxAbs();
    if (kind == QLatin1String("image"))
        destRoot = rootAbsById(QStringLiteral("pictures"));
    else if (kind == QLatin1String("video"))
        destRoot = rootAbsById(QStringLiteral("video"));
    else if (kind == QLatin1String("music"))
        destRoot = rootAbsById(QStringLiteral("music"));
    if (destRoot.isEmpty())
        destRoot = inboxAbs();
    QDir().mkpath(destRoot);
    const QString dest = uniquePath(destRoot, fileName);
    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    if (out.write(fileBody) != fileBody.size())
        return false;
    if (savedName)
        *savedName = QFileInfo(dest).fileName();
    return true;
}

QByteArray FileBrowser::listJson() const
{
    QJsonArray arr;
    QDir dir(inboxAbs());
    for (const QFileInfo &info :
         dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), info.fileName());
        o.insert(QStringLiteral("size"), info.size());
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("folder"), QStringLiteral("无线接收"));
    root.insert(QStringLiteral("files"), arr);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray FileBrowser::pageHtml() const
{
    const QString title = QStringLiteral("车机文件传输");
    QString list;
    QDir dir(inboxAbs());
    for (const QFileInfo &info :
         dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time)) {
        const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(info.fileName()));
        list += QStringLiteral("<li><a href=\"/dl/%1\">%2</a> · %3</li>")
                    .arg(enc, info.fileName().toHtmlEscaped(), formatSize(info.size()));
    }
    if (list.isEmpty())
        list = QStringLiteral("<li>暂无文件</li>");

    const QString html = QStringLiteral(
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>%1</title>"
        "<style>"
        "body{font-family:-apple-system,sans-serif;margin:24px;background:#111;color:#eee}"
        "h1{font-size:22px} .box{background:#1c1c1e;border-radius:12px;padding:16px;margin:12px 0}"
        "button,input::file-selector-button{background:#0a84ff;color:#fff;border:0;border-radius:10px;"
        "padding:12px 18px;font-size:16px}"
        "li{margin:10px 0} a{color:#64d2ff}"
        "</style></head><body>"
        "<h1>%1</h1>"
        "<div class=box><p>上传到车机（图片/视频/音乐自动归类）</p>"
        "<form method=post action=/upload enctype=multipart/form-data>"
        "<input type=file name=file onchange=\"this.form.submit()\">"
        "</form></div>"
        "<div class=box><p>无线接收可下载</p><ul>%2</ul></div>"
        "</body></html>")
                             .arg(title, list);
    return html.toUtf8();
}
