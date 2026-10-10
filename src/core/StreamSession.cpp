#include "StreamSession.hpp"

#include "AudioFocus.hpp"
#include "MediaSession.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

namespace {

QString md5Hex(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
}

QString randomSalt()
{
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString out;
    out.reserve(12);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 12; ++i)
        out.append(QLatin1Char(kChars[rng->bounded(36)]));
    return out;
}

QVariantMap trackMap(const QString &id, const QString &title, const QString &artist,
                     const QString &album, const QString &streamUrl = {})
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("title"), title);
    map.insert(QStringLiteral("artist"), artist);
    map.insert(QStringLiteral("album"), album);
    if (!streamUrl.isEmpty())
        map.insert(QStringLiteral("streamUrl"), streamUrl);
    return map;
}

} // namespace

StreamSession::StreamSession(AudioFocus *audio, MediaSession *media, QObject *parent)
    : QObject(parent)
    , m_audio(audio)
    , m_media(media)
    , m_status(QStringLiteral("未连接"))
{
    loadConfig();
    connect(&m_player, &FfmpegUrlPlayer::playingChanged, this, &StreamSession::playingChanged);
    connect(&m_player, &FfmpegUrlPlayer::errorOccurred, this, [this](const QString &msg) {
        setStatus(QStringLiteral("播放失败"));
        setDetail(msg);
        if (m_audio)
            m_audio->release(QStringLiteral("stream"));
    });
    connect(&m_player, &FfmpegUrlPlayer::finished, this, [this] {
        if (m_index >= 0 && m_index + 1 < m_tracks.size()) {
            playIndex(m_index + 1);
            return;
        }
        setStatus(QStringLiteral("已停止"));
        setDetail({});
        if (m_audio)
            m_audio->release(QStringLiteral("stream"));
        emit playingChanged();
    });
    if (m_audio) {
        connect(m_audio, &AudioFocus::ownerChanged, this, [this] {
            if (m_player.playing() && m_audio->owner() != QLatin1String("stream"))
                stop();
        });
    }
}

StreamSession::~StreamSession()
{
    stop();
}

void StreamSession::loadConfig()
{
    QSettings s;
    m_backend = s.value(QStringLiteral("stream/backend"), QStringLiteral("subsonic")).toString();
    m_serverUrl = s.value(QStringLiteral("stream/serverUrl")).toString();
    m_username = s.value(QStringLiteral("stream/username")).toString();
    m_password = s.value(QStringLiteral("stream/password")).toString();
}

void StreamSession::saveConfig()
{
    QSettings s;
    s.setValue(QStringLiteral("stream/backend"), m_backend);
    s.setValue(QStringLiteral("stream/serverUrl"), m_serverUrl);
    s.setValue(QStringLiteral("stream/username"), m_username);
    s.setValue(QStringLiteral("stream/password"), m_password);
    emit configChanged();
}

void StreamSession::setBackend(const QString &v)
{
    const QString b = v.trimmed().toLower();
    if (b != QLatin1String("subsonic") && b != QLatin1String("jellyfin")
        && b != QLatin1String("navidrome"))
        return;
    const QString norm = (b == QLatin1String("navidrome")) ? QStringLiteral("subsonic") : b;
    if (m_backend == norm)
        return;
    m_backend = norm;
    emit configChanged();
}

void StreamSession::setServerUrl(const QString &v)
{
    if (m_serverUrl == v)
        return;
    m_serverUrl = v.trimmed();
    emit configChanged();
}

void StreamSession::setUsername(const QString &v)
{
    if (m_username == v)
        return;
    m_username = v.trimmed();
    emit configChanged();
}

void StreamSession::setPassword(const QString &v)
{
    if (m_password == v)
        return;
    m_password = v;
    emit configChanged();
}

void StreamSession::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void StreamSession::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void StreamSession::setLoading(bool on)
{
    if (m_loading == on)
        return;
    m_loading = on;
    emit loadingChanged();
}

void StreamSession::setLoggedIn(bool on)
{
    if (m_loggedIn == on)
        return;
    m_loggedIn = on;
    emit loggedInChanged();
}

QString StreamSession::normalizedBase() const
{
    QString base = m_serverUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base;
}

QUrl StreamSession::subsonicUrl(const QString &action, const QUrlQuery &extra) const
{
    QUrl url(normalizedBase() + QStringLiteral("/rest/") + action);
    QUrlQuery q = extra;
    q.addQueryItem(QStringLiteral("u"), m_username);
    q.addQueryItem(QStringLiteral("t"), m_token);
    q.addQueryItem(QStringLiteral("s"), m_salt);
    q.addQueryItem(QStringLiteral("v"), QStringLiteral("1.16.1"));
    q.addQueryItem(QStringLiteral("c"), QStringLiteral("MP157-IVI"));
    q.addQueryItem(QStringLiteral("f"), QStringLiteral("json"));
    url.setQuery(q);
    return url;
}

void StreamSession::getJson(const QUrl &url, const std::function<void(const QJsonObject &)> &ok)
{
    setLoading(true);
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MP157-IVI-Stream/1.0"));
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, ok] {
        reply->deleteLater();
        setLoading(false);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("请求失败"));
            setDetail(reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            setStatus(QStringLiteral("响应无效"));
            return;
        }
        ok(doc.object());
    });
}

void StreamSession::getJsonAuth(const QUrl &url, const std::function<void(const QJsonObject &)> &ok)
{
    setLoading(true);
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MP157-IVI-Stream/1.0"));
    req.setRawHeader("X-Emby-Token", m_token.toUtf8());
    req.setRawHeader("X-Emby-Authorization",
                     QStringLiteral("MediaBrowser Client=\"MP157-IVI\", Device=\"IVI\", DeviceId=\"%1\", Version=\"1.0\", Token=\"%2\"")
                         .arg(m_deviceId, m_token)
                         .toUtf8());
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, ok] {
        reply->deleteLater();
        setLoading(false);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("请求失败"));
            setDetail(reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            setStatus(QStringLiteral("响应无效"));
            return;
        }
        ok(doc.object());
    });
}

void StreamSession::login()
{
    saveConfig();
    if (normalizedBase().isEmpty() || m_username.isEmpty() || m_password.isEmpty()) {
        setStatus(QStringLiteral("请填写服务器与账号"));
        return;
    }
    setStatus(QStringLiteral("登录中…"));
    if (m_backend == QLatin1String("jellyfin"))
        loginJellyfin();
    else
        loginSubsonic();
}

void StreamSession::logout()
{
    stop();
    m_token.clear();
    m_salt.clear();
    m_userId.clear();
    m_tracks.clear();
    m_albums.clear();
    emit tracksChanged();
    emit albumsChanged();
    setLoggedIn(false);
    setStatus(QStringLiteral("未连接"));
    setDetail({});
}

void StreamSession::loginSubsonic()
{
    m_salt = randomSalt();
    m_token = md5Hex((m_password + m_salt).toUtf8());
    getJson(subsonicUrl(QStringLiteral("ping.view")), [this](const QJsonObject &obj) {
        const QJsonObject sub = obj.value(QStringLiteral("subsonic-response")).toObject();
        if (sub.value(QStringLiteral("status")).toString() != QLatin1String("ok")) {
            const QString msg = sub.value(QStringLiteral("error")).toObject()
                                    .value(QStringLiteral("message")).toString();
            setStatus(QStringLiteral("登录失败"));
            setDetail(msg.isEmpty() ? QStringLiteral("Subsonic 认证失败") : msg);
            setLoggedIn(false);
            return;
        }
        setLoggedIn(true);
        setStatus(QStringLiteral("已连接"));
        setDetail(QStringLiteral("Subsonic / Navidrome"));
        loadAlbumsSubsonic();
    });
}

void StreamSession::loginJellyfin()
{
    setLoading(true);
    QUrl url(normalizedBase() + QStringLiteral("/Users/authenticatebyname"));
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MP157-IVI-Stream/1.0"));
    req.setRawHeader("X-Emby-Authorization",
                     QStringLiteral("MediaBrowser Client=\"MP157-IVI\", Device=\"IVI\", DeviceId=\"%1\", Version=\"1.0\"")
                         .arg(m_deviceId)
                         .toUtf8());
    QJsonObject body;
    body.insert(QStringLiteral("Username"), m_username);
    body.insert(QStringLiteral("Pw"), m_password);
    QNetworkReply *reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setLoading(false);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("登录失败"));
            setDetail(reply->errorString());
            setLoggedIn(false);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_token = obj.value(QStringLiteral("AccessToken")).toString();
        m_userId = obj.value(QStringLiteral("User")).toObject().value(QStringLiteral("Id")).toString();
        if (m_token.isEmpty() || m_userId.isEmpty()) {
            setStatus(QStringLiteral("登录失败"));
            setDetail(QStringLiteral("Jellyfin 无 token"));
            setLoggedIn(false);
            return;
        }
        setLoggedIn(true);
        setStatus(QStringLiteral("已连接"));
        setDetail(QStringLiteral("Jellyfin"));
        loadAlbumsJellyfin();
    });
}

void StreamSession::loadAlbums()
{
    if (!m_loggedIn)
        return;
    if (m_backend == QLatin1String("jellyfin"))
        loadAlbumsJellyfin();
    else
        loadAlbumsSubsonic();
}

void StreamSession::loadAlbumsSubsonic()
{
    QUrlQuery extra;
    extra.addQueryItem(QStringLiteral("type"), QStringLiteral("newest"));
    extra.addQueryItem(QStringLiteral("size"), QStringLiteral("50"));
    getJson(subsonicUrl(QStringLiteral("getAlbumList2.view"), extra), [this](const QJsonObject &obj) {
        const QJsonObject sub = obj.value(QStringLiteral("subsonic-response")).toObject();
        const QJsonArray arr = sub.value(QStringLiteral("albumList2")).toObject()
                                   .value(QStringLiteral("album")).toArray();
        m_albums.clear();
        for (const QJsonValue &v : arr) {
            const QJsonObject a = v.toObject();
            QVariantMap row;
            row.insert(QStringLiteral("id"), a.value(QStringLiteral("id")).toVariant().toString());
            row.insert(QStringLiteral("name"), a.value(QStringLiteral("name")).toString());
            row.insert(QStringLiteral("artist"), a.value(QStringLiteral("artist")).toString());
            m_albums.push_back(row);
        }
        emit albumsChanged();
        setDetail(QStringLiteral("%1 张专辑").arg(m_albums.size()));
    });
}

void StreamSession::loadAlbumsJellyfin()
{
    QUrl url(normalizedBase() + QStringLiteral("/Users/") + m_userId + QStringLiteral("/Items"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("IncludeItemTypes"), QStringLiteral("MusicAlbum"));
    q.addQueryItem(QStringLiteral("Recursive"), QStringLiteral("true"));
    q.addQueryItem(QStringLiteral("SortBy"), QStringLiteral("DateCreated,SortName"));
    q.addQueryItem(QStringLiteral("SortOrder"), QStringLiteral("Descending"));
    q.addQueryItem(QStringLiteral("Limit"), QStringLiteral("50"));
    url.setQuery(q);
    getJsonAuth(url, [this](const QJsonObject &obj) {
        const QJsonArray arr = obj.value(QStringLiteral("Items")).toArray();
        m_albums.clear();
        for (const QJsonValue &v : arr) {
            const QJsonObject a = v.toObject();
            QVariantMap row;
            row.insert(QStringLiteral("id"), a.value(QStringLiteral("Id")).toString());
            row.insert(QStringLiteral("name"), a.value(QStringLiteral("Name")).toString());
            row.insert(QStringLiteral("artist"),
                       a.value(QStringLiteral("AlbumArtist")).toString());
            m_albums.push_back(row);
        }
        emit albumsChanged();
        setDetail(QStringLiteral("%1 张专辑").arg(m_albums.size()));
    });
}

void StreamSession::openAlbum(const QString &id)
{
    if (id.isEmpty() || !m_loggedIn)
        return;
    if (m_backend == QLatin1String("jellyfin"))
        openAlbumJellyfin(id);
    else
        openAlbumSubsonic(id);
}

void StreamSession::openAlbumSubsonic(const QString &id)
{
    QUrlQuery extra;
    extra.addQueryItem(QStringLiteral("id"), id);
    getJson(subsonicUrl(QStringLiteral("getAlbum.view"), extra), [this](const QJsonObject &obj) {
        const QJsonObject album = obj.value(QStringLiteral("subsonic-response")).toObject()
                                      .value(QStringLiteral("album")).toObject();
        const QString albumName = album.value(QStringLiteral("name")).toString();
        const QString artist = album.value(QStringLiteral("artist")).toString();
        const QJsonArray songs = album.value(QStringLiteral("song")).toArray();
        m_tracks.clear();
        for (const QJsonValue &v : songs) {
            const QJsonObject s = v.toObject();
            m_tracks.push_back(trackMap(s.value(QStringLiteral("id")).toVariant().toString(),
                                        s.value(QStringLiteral("title")).toString(),
                                        s.value(QStringLiteral("artist")).toString().isEmpty()
                                            ? artist
                                            : s.value(QStringLiteral("artist")).toString(),
                                        albumName));
        }
        emit tracksChanged();
        setDetail(albumName);
    });
}

void StreamSession::openAlbumJellyfin(const QString &id)
{
    QUrl url(normalizedBase() + QStringLiteral("/Users/") + m_userId + QStringLiteral("/Items"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("ParentId"), id);
    q.addQueryItem(QStringLiteral("IncludeItemTypes"), QStringLiteral("Audio"));
    q.addQueryItem(QStringLiteral("SortBy"), QStringLiteral("IndexNumber,SortName"));
    url.setQuery(q);
    getJsonAuth(url, [this](const QJsonObject &obj) {
        const QJsonArray arr = obj.value(QStringLiteral("Items")).toArray();
        m_tracks.clear();
        for (const QJsonValue &v : arr) {
            const QJsonObject s = v.toObject();
            m_tracks.push_back(trackMap(s.value(QStringLiteral("Id")).toString(),
                                        s.value(QStringLiteral("Name")).toString(),
                                        s.value(QStringLiteral("Artists")).toArray().isEmpty()
                                            ? s.value(QStringLiteral("AlbumArtist")).toString()
                                            : s.value(QStringLiteral("Artists")).toArray().at(0).toString(),
                                        s.value(QStringLiteral("Album")).toString()));
        }
        emit tracksChanged();
        setDetail(QStringLiteral("%1 首").arg(m_tracks.size()));
    });
}

void StreamSession::search(const QString &query)
{
    const QString q = query.trimmed();
    if (q.isEmpty() || !m_loggedIn)
        return;
    if (m_backend == QLatin1String("jellyfin"))
        searchJellyfin(q);
    else
        searchSubsonic(q);
}

void StreamSession::searchSubsonic(const QString &query)
{
    QUrlQuery extra;
    extra.addQueryItem(QStringLiteral("query"), query);
    extra.addQueryItem(QStringLiteral("songCount"), QStringLiteral("50"));
    extra.addQueryItem(QStringLiteral("albumCount"), QStringLiteral("0"));
    extra.addQueryItem(QStringLiteral("artistCount"), QStringLiteral("0"));
    getJson(subsonicUrl(QStringLiteral("search3.view"), extra), [this](const QJsonObject &obj) {
        const QJsonArray songs = obj.value(QStringLiteral("subsonic-response")).toObject()
                                     .value(QStringLiteral("searchResult3")).toObject()
                                     .value(QStringLiteral("song")).toArray();
        m_tracks.clear();
        for (const QJsonValue &v : songs) {
            const QJsonObject s = v.toObject();
            m_tracks.push_back(trackMap(s.value(QStringLiteral("id")).toVariant().toString(),
                                        s.value(QStringLiteral("title")).toString(),
                                        s.value(QStringLiteral("artist")).toString(),
                                        s.value(QStringLiteral("album")).toString()));
        }
        emit tracksChanged();
        setDetail(QStringLiteral("搜索 %1 首").arg(m_tracks.size()));
    });
}

void StreamSession::searchJellyfin(const QString &query)
{
    QUrl url(normalizedBase() + QStringLiteral("/Users/") + m_userId + QStringLiteral("/Items"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("SearchTerm"), query);
    q.addQueryItem(QStringLiteral("IncludeItemTypes"), QStringLiteral("Audio"));
    q.addQueryItem(QStringLiteral("Recursive"), QStringLiteral("true"));
    q.addQueryItem(QStringLiteral("Limit"), QStringLiteral("50"));
    url.setQuery(q);
    getJsonAuth(url, [this](const QJsonObject &obj) {
        const QJsonArray arr = obj.value(QStringLiteral("Items")).toArray();
        m_tracks.clear();
        for (const QJsonValue &v : arr) {
            const QJsonObject s = v.toObject();
            m_tracks.push_back(trackMap(s.value(QStringLiteral("Id")).toString(),
                                        s.value(QStringLiteral("Name")).toString(),
                                        s.value(QStringLiteral("AlbumArtist")).toString(),
                                        s.value(QStringLiteral("Album")).toString()));
        }
        emit tracksChanged();
        setDetail(QStringLiteral("搜索 %1 首").arg(m_tracks.size()));
    });
}

QString StreamSession::streamUrlFor(const QVariantMap &track) const
{
    const QString custom = track.value(QStringLiteral("streamUrl")).toString();
    if (!custom.isEmpty())
        return custom;
    const QString id = track.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return {};
    if (m_backend == QLatin1String("jellyfin")) {
        return normalizedBase() + QStringLiteral("/Audio/") + id
            + QStringLiteral("/stream?static=true&api_key=") + m_token;
    }
    QUrlQuery extra;
    extra.addQueryItem(QStringLiteral("id"), id);
    return subsonicUrl(QStringLiteral("stream.view"), extra).toString();
}

void StreamSession::playIndex(int index)
{
    if (index < 0 || index >= m_tracks.size())
        return;
    const QVariantMap item = m_tracks.at(index).toMap();
    const QString url = streamUrlFor(item);
    if (url.isEmpty())
        return;

    if (m_media && m_media->playing())
        m_media->pause();
    if (m_audio)
        m_audio->request(QStringLiteral("stream"), m_audio->radioPriority());

    m_index = index;
    m_title = item.value(QStringLiteral("title")).toString();
    m_artist = item.value(QStringLiteral("artist")).toString();
    m_album = item.value(QStringLiteral("album")).toString();
    emit trackChanged();
    setStatus(QStringLiteral("连接中…"));
    setDetail(url);
    m_player.play(QUrl(url));
    setStatus(QStringLiteral("播放中"));
}

void StreamSession::stop()
{
    m_player.stop();
    if (m_audio)
        m_audio->release(QStringLiteral("stream"));
    setStatus(m_loggedIn ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    setDetail({});
    emit playingChanged();
}

void StreamSession::toggle()
{
    if (m_player.playing()) {
        stop();
        return;
    }
    playIndex(m_index >= 0 ? m_index : 0);
}

void StreamSession::next()
{
    if (m_tracks.isEmpty())
        return;
    const int i = m_index < 0 ? 0 : (m_index + 1) % m_tracks.size();
    playIndex(i);
}

void StreamSession::previous()
{
    if (m_tracks.isEmpty())
        return;
    const int n = m_tracks.size();
    const int i = m_index < 0 ? 0 : (m_index - 1 + n) % n;
    playIndex(i);
}
