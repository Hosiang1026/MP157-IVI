#include "BluetoothAncsClient.hpp"

#include "BluetoothMediaHub.hpp"
#include "NotificationSession.hpp"

#include <QHash>
#include <QThread>
#include <QtEndian>

#ifdef Q_OS_WIN
#include <windows.h>
#undef interface
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

#ifdef Q_OS_LINUX
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#endif

namespace {

QString normalizeMac(QString value)
{
    value = value.trimmed().toUpper().replace(QLatin1Char('-'), QLatin1Char(':'));
    return value;
}

quint64 macToU64(const QString &mac)
{
    const QStringList parts = mac.split(QLatin1Char(':'));
    if (parts.size() != 6)
        return 0;
    quint64 addr = 0;
    for (const QString &p : parts) {
        bool ok = false;
        const int v = p.toInt(&ok, 16);
        if (!ok || v < 0 || v > 255)
            return 0;
        addr = (addr << 8) | quint64(v & 0xff);
    }
    return addr;
}

} // namespace

#ifdef Q_OS_WIN
struct BluetoothAncsClient::Platform {
    winrt::Windows::Devices::Bluetooth::BluetoothLEDevice device{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic notifSource{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic controlPoint{nullptr};
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic dataSource{nullptr};
    winrt::event_token notifToken{};
    winrt::event_token dataToken{};
    bool apartment = false;
};
#elif defined(Q_OS_LINUX)
struct BluetoothAncsClient::Platform {
    QString devicePath;
    QString controlPath;
    QString notifPath;
    QString dataPath;
};
#else
struct BluetoothAncsClient::Platform {};
#endif

BluetoothAncsClient::BluetoothAncsClient(BluetoothMediaHub *hub, NotificationSession *notifications,
                                         QObject *parent)
    : QObject(parent)
    , m_hub(hub)
    , m_notifications(notifications)
    , m_platform(new Platform)
{
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, this, &BluetoothAncsClient::tick);
    if (m_hub) {
        connect(m_hub, &BluetoothMediaHub::phoneChanged, this, &BluetoothAncsClient::tick);
        connect(m_hub, &BluetoothMediaHub::devicesChanged, this, &BluetoothAncsClient::tick);
    }
    m_timer.start();
}

BluetoothAncsClient::~BluetoothAncsClient()
{
    stopSession();
    delete m_platform;
    m_platform = nullptr;
}

QString BluetoothAncsClient::friendlyApp(const QString &appId)
{
    static const QHash<QString, QString> kMap = {
        {QStringLiteral("com.tencent.xin"), QStringLiteral("微信")},
        {QStringLiteral("com.tencent.mqq"), QStringLiteral("QQ")},
        {QStringLiteral("com.tencent.qqmail"), QStringLiteral("QQ邮箱")},
        {QStringLiteral("com.tencent.weread"), QStringLiteral("微信读书")},
        {QStringLiteral("com.tencent.QQMusic"), QStringLiteral("QQ音乐")},
        {QStringLiteral("com.tencent.live4iphone"), QStringLiteral("腾讯视频")},
        {QStringLiteral("com.tencent.map"), QStringLiteral("腾讯地图")},
        {QStringLiteral("com.apple.MobileSMS"), QStringLiteral("信息")},
        {QStringLiteral("com.apple.mobilephone"), QStringLiteral("电话")},
        {QStringLiteral("com.apple.mobilemail"), QStringLiteral("邮件")},
        {QStringLiteral("com.apple.mobilecal"), QStringLiteral("日历")},
        {QStringLiteral("com.apple.Maps"), QStringLiteral("地图")},
        {QStringLiteral("com.apple.FaceTime"), QStringLiteral("FaceTime")},
        {QStringLiteral("com.apple.news"), QStringLiteral("新闻")},
        {QStringLiteral("com.laiwang.DingTalk"), QStringLiteral("钉钉")},
        {QStringLiteral("com.alibaba.iAlipay"), QStringLiteral("支付宝")},
        {QStringLiteral("com.alipay.iphoneclient"), QStringLiteral("支付宝")},
        {QStringLiteral("com.taobao.taobao4iphone"), QStringLiteral("淘宝")},
        {QStringLiteral("com.taobao.fleamarket"), QStringLiteral("闲鱼")},
        {QStringLiteral("com.360buy.jdmobile"), QStringLiteral("京东")},
        {QStringLiteral("com.xunmeng.pinduoduo"), QStringLiteral("拼多多")},
        {QStringLiteral("com.meituan.imeituan"), QStringLiteral("美团")},
        {QStringLiteral("me.ele.ios.eleme"), QStringLiteral("饿了么")},
        {QStringLiteral("com.dianping.dpscope"), QStringLiteral("大众点评")},
        {QStringLiteral("com.sina.weibo"), QStringLiteral("微博")},
        {QStringLiteral("com.ss.iphone.ugc.Aweme"), QStringLiteral("抖音")},
        {QStringLiteral("com.ss.iphone.article.News"), QStringLiteral("今日头条")},
        {QStringLiteral("com.jiangjia.gif"), QStringLiteral("快手")},
        {QStringLiteral("com.xingin.xhs"), QStringLiteral("小红书")},
        {QStringLiteral("com.zhihu.ios"), QStringLiteral("知乎")},
        {QStringLiteral("tv.danmaku.bilianime"), QStringLiteral("哔哩哔哩")},
        {QStringLiteral("com.bilibili.bilianime"), QStringLiteral("哔哩哔哩")},
        {QStringLiteral("com.netease.cloudmusic"), QStringLiteral("网易云音乐")},
        {QStringLiteral("com.kugou.kugou1002"), QStringLiteral("酷狗音乐")},
        {QStringLiteral("com.baidu.BaiduMobile"), QStringLiteral("百度")},
        {QStringLiteral("com.baidu.map"), QStringLiteral("百度地图")},
        {QStringLiteral("com.autonavi.amap"), QStringLiteral("高德地图")},
        {QStringLiteral("com.xiaojukeji.didi"), QStringLiteral("滴滴")},
        {QStringLiteral("com.didi.passenger"), QStringLiteral("滴滴")},
        {QStringLiteral("ctrip.com"), QStringLiteral("携程")},
        {QStringLiteral("com.ctrip.inner.wireless"), QStringLiteral("携程")},
        {QStringLiteral("com.iqiyi.iphone"), QStringLiteral("爱奇艺")},
        {QStringLiteral("com.youku.YouKu"), QStringLiteral("优酷")},
        {QStringLiteral("ph.telegra.Telegraph"), QStringLiteral("Telegram")},
        {QStringLiteral("com.whatsapp.WhatsApp"), QStringLiteral("WhatsApp")},
        {QStringLiteral("com.burbn.instagram"), QStringLiteral("Instagram")},
        {QStringLiteral("com.google.Gmail"), QStringLiteral("Gmail")},
        {QStringLiteral("com.google.ios.youtube"), QStringLiteral("YouTube")},
    };
    const QString id = appId.trimmed();
    if (id.isEmpty())
        return QStringLiteral("通知");
    if (kMap.contains(id))
        return kMap.value(id);
    const QString lower = id.toLower();
    for (auto it = kMap.constBegin(); it != kMap.constEnd(); ++it) {
        if (lower == it.key().toLower())
            return it.value();
    }
    if (lower.contains(QStringLiteral("wechat")) || lower.contains(QStringLiteral("tencent.xin")))
        return QStringLiteral("微信");
    if (lower.contains(QStringLiteral("alipay")))
        return QStringLiteral("支付宝");
    if (lower.contains(QStringLiteral("dingtalk")))
        return QStringLiteral("钉钉");
    if (lower.contains(QStringLiteral("taobao")))
        return QStringLiteral("淘宝");
    if (lower.contains(QStringLiteral("aweme")) || lower.contains(QStringLiteral("douyin")))
        return QStringLiteral("抖音");
    return QStringLiteral("通知");
}

void BluetoothAncsClient::publish(const QString &appId, const QString &title, const QString &message)
{
    if (!m_notifications)
        return;
    const QString app = friendlyApp(appId);
    const QString t = title.trimmed().isEmpty() ? app : title.trimmed();
    const QString b = message.trimmed().isEmpty() ? QStringLiteral("新通知") : message.trimmed();
    m_notifications->applyRemote(app, t, b);
}

void BluetoothAncsClient::tick()
{
    if (!m_hub || !m_notifications)
        return;
    if (!m_hub->phoneConnected() || m_hub->phoneAddress().isEmpty()) {
        stopSession();
        return;
    }
    const QString mac = normalizeMac(m_hub->phoneAddress());
#ifdef Q_OS_WIN
    if (mac == m_address && m_platform && m_platform->device && !m_starting.load())
        return;
#elif defined(Q_OS_LINUX)
    if (mac == m_address && m_platform && !m_platform->notifPath.isEmpty() && !m_starting.load())
        return;
#else
    if (mac == m_address && !m_starting.load())
        return;
#endif
    startSession(mac);
}

void BluetoothAncsClient::handleNotificationSource(const QByteArray &value)
{
    if (value.size() < 8)
        return;
    if (quint8(value[0]) == 2)
        return;
    if (quint8(value[2]) == 1)
        return;
    const quint32 uid = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(value.constData() + 4));
    m_dataBuf.clear();
    m_pendingUid = uid;
    m_haveApp = m_haveTitle = m_haveMessage = false;
    m_appId.clear();
    m_title.clear();
    m_message.clear();
    requestAttributes(uid);
}

void BluetoothAncsClient::handleDataSource(const QByteArray &value)
{
    if (value.isEmpty())
        return;
    m_dataBuf.append(value);
    if (m_dataBuf.size() < 5 || quint8(m_dataBuf[0]) != 0x00)
        return;
    const quint32 uid = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(m_dataBuf.constData() + 1));
    if (uid != m_pendingUid)
        return;
    int i = 5;
    while (i + 3 <= m_dataBuf.size()) {
        const quint8 attr = quint8(m_dataBuf[i]);
        const quint16 len = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(m_dataBuf.constData() + i + 1));
        if (i + 3 + len > m_dataBuf.size())
            return;
        const QString text = QString::fromUtf8(m_dataBuf.constData() + i + 3, len);
        if (attr == 0x00) {
            m_appId = text;
            m_haveApp = true;
        } else if (attr == 0x01) {
            m_title = text;
            m_haveTitle = true;
        } else if (attr == 0x03) {
            m_message = text;
            m_haveMessage = true;
        }
        i += 3 + len;
    }
    if (m_haveApp || m_haveTitle || m_haveMessage) {
        publish(m_appId, m_title, m_message);
        m_dataBuf.clear();
        m_pendingUid = 0;
    }
}

void BluetoothAncsClient::onBluezNotifProps(const QString &iface, const QVariantMap &changed,
                                            const QStringList &)
{
#ifdef Q_OS_LINUX
    if (iface != QLatin1String("org.bluez.GattCharacteristic1"))
        return;
    const QVariant v = changed.value(QStringLiteral("Value"));
    if (!v.isValid())
        return;
    handleNotificationSource(v.toByteArray());
#else
    Q_UNUSED(iface);
    Q_UNUSED(changed);
#endif
}

void BluetoothAncsClient::onBluezDataProps(const QString &iface, const QVariantMap &changed,
                                           const QStringList &)
{
#ifdef Q_OS_LINUX
    if (iface != QLatin1String("org.bluez.GattCharacteristic1"))
        return;
    const QVariant v = changed.value(QStringLiteral("Value"));
    if (!v.isValid())
        return;
    handleDataSource(v.toByteArray());
#else
    Q_UNUSED(iface);
    Q_UNUSED(changed);
#endif
}

#ifdef Q_OS_WIN

void BluetoothAncsClient::requestAttributes(quint32 uid)
{
    if (!m_platform || !m_platform->controlPoint)
        return;
    uint8_t cmd[12] = {};
    cmd[0] = 0x00;
    qToLittleEndian(uid, cmd + 1);
    cmd[5] = 0x00;
    cmd[6] = 0x01;
    cmd[7] = 64;
    cmd[8] = 0;
    cmd[9] = 0x03;
    cmd[10] = 128;
    cmd[11] = 0;
    try {
        winrt::Windows::Storage::Streams::DataWriter writer;
        writer.WriteBytes(winrt::array_view<const uint8_t>(cmd, cmd + sizeof(cmd)));
        m_platform->controlPoint.WriteValueAsync(writer.DetachBuffer()).get();
    } catch (...) {
    }
}

void BluetoothAncsClient::stopSession()
{
    m_address.clear();
    m_dataBuf.clear();
    if (!m_platform)
        return;
    try {
        if (m_platform->notifSource && m_platform->notifToken.value)
            m_platform->notifSource.ValueChanged(m_platform->notifToken);
        if (m_platform->dataSource && m_platform->dataToken.value)
            m_platform->dataSource.ValueChanged(m_platform->dataToken);
    } catch (...) {
    }
    m_platform->notifToken = {};
    m_platform->dataToken = {};
    m_platform->notifSource = nullptr;
    m_platform->controlPoint = nullptr;
    m_platform->dataSource = nullptr;
    if (m_platform->device) {
        try {
            m_platform->device.Close();
        } catch (...) {
        }
    }
    m_platform->device = nullptr;
}

void BluetoothAncsClient::startSession(const QString &address)
{
    if (m_starting.exchange(true))
        return;
    if (address == m_address && m_platform && m_platform->device) {
        m_starting = false;
        return;
    }
    stopSession();
    m_address = address;
    const quint64 addr = macToU64(address);
    if (!addr) {
        m_starting = false;
        return;
    }

    QThread *worker = QThread::create([this, addr, address] {
        try {
            if (!m_platform->apartment) {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                m_platform->apartment = true;
            }
            using winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
            using winrt::Windows::Devices::Bluetooth::BluetoothCacheMode;
            using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattClientCharacteristicConfigurationDescriptorValue;
            using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus;
            using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic;
            using winrt::Windows::Storage::Streams::DataReader;

            auto device = BluetoothLEDevice::FromBluetoothAddressAsync(addr).get();
            if (!device) {
                QMetaObject::invokeMethod(this, [this] {
                    m_address.clear();
                    m_starting = false;
                }, Qt::QueuedConnection);
                return;
            }

            const winrt::guid ancsService{0x7905F431, 0xB5CE, 0x4E99, {0xA4, 0x0F, 0x4B, 0x1E, 0x12, 0x2D, 0x00, 0xD0}};
            const winrt::guid notifUuid{0x9FBF120D, 0x6301, 0x42D9, {0x8C, 0x58, 0x25, 0xE6, 0x99, 0xA2, 0x1D, 0xBD}};
            const winrt::guid controlUuid{0x69D1D8F3, 0x45E1, 0x49A8, {0x98, 0x21, 0x9B, 0xBD, 0xFD, 0xAA, 0xD9, 0xD9}};
            const winrt::guid dataUuid{0x22EAC6E9, 0x24D6, 0x4BB5, {0xBE, 0x44, 0xB3, 0x6A, 0xCE, 0x7C, 0x7B, 0xFB}};

            auto services = device.GetGattServicesForUuidAsync(ancsService, BluetoothCacheMode::Uncached).get();
            if (services.Status() != GattCommunicationStatus::Success || services.Services().Size() == 0) {
                device.Close();
                QMetaObject::invokeMethod(this, [this] {
                    m_address.clear();
                    m_starting = false;
                }, Qt::QueuedConnection);
                return;
            }

            auto service = services.Services().GetAt(0);
            auto takeChar = [&](const winrt::guid &uuid) -> GattCharacteristic {
                auto result = service.GetCharacteristicsForUuidAsync(uuid, BluetoothCacheMode::Uncached).get();
                if (result.Status() != GattCommunicationStatus::Success || result.Characteristics().Size() == 0)
                    return nullptr;
                return result.Characteristics().GetAt(0);
            };
            GattCharacteristic notifSource = takeChar(notifUuid);
            GattCharacteristic controlPoint = takeChar(controlUuid);
            GattCharacteristic dataSource = takeChar(dataUuid);
            if (!notifSource || !controlPoint || !dataSource) {
                device.Close();
                QMetaObject::invokeMethod(this, [this] {
                    m_address.clear();
                    m_starting = false;
                }, Qt::QueuedConnection);
                return;
            }

            if (dataSource.WriteClientCharacteristicConfigurationDescriptorAsync(
                        GattClientCharacteristicConfigurationDescriptorValue::Notify)
                    .get()
                != GattCommunicationStatus::Success) {
                device.Close();
                QMetaObject::invokeMethod(this, [this] {
                    m_address.clear();
                    m_starting = false;
                }, Qt::QueuedConnection);
                return;
            }
            if (notifSource.WriteClientCharacteristicConfigurationDescriptorAsync(
                         GattClientCharacteristicConfigurationDescriptorValue::Notify)
                    .get()
                != GattCommunicationStatus::Success) {
                device.Close();
                QMetaObject::invokeMethod(this, [this] {
                    m_address.clear();
                    m_starting = false;
                }, Qt::QueuedConnection);
                return;
            }

            auto toBytes = [](auto const &args) {
                DataReader reader = DataReader::FromBuffer(args.CharacteristicValue());
                const uint32_t n = reader.UnconsumedBufferLength();
                winrt::com_array<uint8_t> arr(n);
                reader.ReadBytes(arr);
                return QByteArray(reinterpret_cast<const char *>(arr.data()), int(arr.size()));
            };

            auto dataToken = dataSource.ValueChanged([this, toBytes](auto &&, auto &&args) {
                const QByteArray bytes = toBytes(args);
                QMetaObject::invokeMethod(this, [this, bytes] { handleDataSource(bytes); },
                                          Qt::QueuedConnection);
            });
            auto notifToken = notifSource.ValueChanged([this, toBytes](auto &&, auto &&args) {
                const QByteArray bytes = toBytes(args);
                QMetaObject::invokeMethod(this, [this, bytes] { handleNotificationSource(bytes); },
                                          Qt::QueuedConnection);
            });

            QMetaObject::invokeMethod(
                this,
                [this, device, notifSource, controlPoint, dataSource, notifToken, dataToken, address] {
                    m_platform->device = device;
                    m_platform->notifSource = notifSource;
                    m_platform->controlPoint = controlPoint;
                    m_platform->dataSource = dataSource;
                    m_platform->notifToken = notifToken;
                    m_platform->dataToken = dataToken;
                    m_address = address;
                    m_starting = false;
                },
                Qt::QueuedConnection);
        } catch (...) {
            QMetaObject::invokeMethod(this, [this] {
                m_address.clear();
                m_starting = false;
            }, Qt::QueuedConnection);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

#elif defined(Q_OS_LINUX)

void BluetoothAncsClient::requestAttributes(quint32 uid)
{
    if (!m_platform || m_platform->controlPath.isEmpty())
        return;
    QByteArray cmd(12, char(0));
    cmd[0] = 0x00;
    qToLittleEndian(uid, reinterpret_cast<uchar *>(cmd.data() + 1));
    cmd[5] = 0x00;
    cmd[6] = 0x01;
    cmd[7] = 64;
    cmd[8] = 0;
    cmd[9] = 0x03;
    cmd[10] = char(128);
    cmd[11] = 0;
    QDBusInterface ch(QStringLiteral("org.bluez"), m_platform->controlPath,
                      QStringLiteral("org.bluez.GattCharacteristic1"),
                      QDBusConnection::systemBus());
    ch.call(QStringLiteral("WriteValue"), QVariant::fromValue(cmd), QVariantMap());
}

void BluetoothAncsClient::stopSession()
{
    if (m_platform) {
        if (!m_platform->notifPath.isEmpty()) {
            QDBusConnection::systemBus().disconnect(
                QStringLiteral("org.bluez"), m_platform->notifPath,
                QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                this, SLOT(onBluezNotifProps(QString,QVariantMap,QStringList)));
            QDBusInterface ch(QStringLiteral("org.bluez"), m_platform->notifPath,
                              QStringLiteral("org.bluez.GattCharacteristic1"),
                              QDBusConnection::systemBus());
            ch.call(QStringLiteral("StopNotify"));
        }
        if (!m_platform->dataPath.isEmpty()) {
            QDBusConnection::systemBus().disconnect(
                QStringLiteral("org.bluez"), m_platform->dataPath,
                QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                this, SLOT(onBluezDataProps(QString,QVariantMap,QStringList)));
            QDBusInterface ch(QStringLiteral("org.bluez"), m_platform->dataPath,
                              QStringLiteral("org.bluez.GattCharacteristic1"),
                              QDBusConnection::systemBus());
            ch.call(QStringLiteral("StopNotify"));
        }
        m_platform->devicePath.clear();
        m_platform->controlPath.clear();
        m_platform->notifPath.clear();
        m_platform->dataPath.clear();
    }
    m_address.clear();
    m_dataBuf.clear();
}

void BluetoothAncsClient::startSession(const QString &address)
{
    if (m_starting.exchange(true))
        return;
    if (address == m_address && m_platform && !m_platform->notifPath.isEmpty()) {
        m_starting = false;
        return;
    }
    stopSession();
    m_address = address;
    m_platform->devicePath =
        QStringLiteral("/org/bluez/hci0/dev_") + QString(address).replace(QLatin1Char(':'), QLatin1Char('_'));

    QDBusInterface dev(QStringLiteral("org.bluez"), m_platform->devicePath,
                       QStringLiteral("org.bluez.Device1"), QDBusConnection::systemBus());
    if (!dev.isValid()) {
        m_address.clear();
        m_starting = false;
        return;
    }
    if (!dev.property(QStringLiteral("Connected")).toBool())
        dev.call(QStringLiteral("Connect"));

    QDBusInterface mgr(QStringLiteral("org.bluez"), QStringLiteral("/"),
                       QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                       QDBusConnection::systemBus());
    const QDBusMessage reply = mgr.call(QStringLiteral("GetManagedObjects"));
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        m_address.clear();
        m_starting = false;
        return;
    }

    const QString prefix = m_platform->devicePath + QLatin1Char('/');
    const QString notif = QStringLiteral("9fbf120d-6301-42d9-8c58-25e699a21dbd");
    const QString control = QStringLiteral("69d1d8f3-45e1-49a8-9821-9bbdfdaad9d9");
    const QString data = QStringLiteral("22eac6e9-24d6-4bb5-be44-b36ace7c7bfb");

    const QDBusArgument root = reply.arguments().at(0).value<QDBusArgument>();
    root.beginMap();
    while (!root.atEnd()) {
        root.beginMapEntry();
        QDBusObjectPath path;
        root >> path;
        root.beginMap();
        while (!root.atEnd()) {
            root.beginMapEntry();
            QString iface;
            root >> iface;
            QVariantMap props;
            root.beginMap();
            while (!root.atEnd()) {
                root.beginMapEntry();
                QString key;
                QDBusVariant val;
                root >> key >> val;
                props.insert(key, val.variant());
                root.endMapEntry();
            }
            root.endMap();
            root.endMapEntry();

            const QString p = path.path();
            if (!p.startsWith(prefix))
                continue;
            if (iface == QLatin1String("org.bluez.GattCharacteristic1")) {
                const QString uuid = props.value(QStringLiteral("UUID")).toString().toLower();
                if (uuid == notif)
                    m_platform->notifPath = p;
                else if (uuid == control)
                    m_platform->controlPath = p;
                else if (uuid == data)
                    m_platform->dataPath = p;
            }
        }
        root.endMap();
        root.endMapEntry();
    }
    root.endMap();

    if (m_platform->notifPath.isEmpty() || m_platform->controlPath.isEmpty()
        || m_platform->dataPath.isEmpty()) {
        m_address.clear();
        m_starting = false;
        return;
    }

    QDBusInterface dataCh(QStringLiteral("org.bluez"), m_platform->dataPath,
                          QStringLiteral("org.bluez.GattCharacteristic1"),
                          QDBusConnection::systemBus());
    QDBusInterface notifCh(QStringLiteral("org.bluez"), m_platform->notifPath,
                           QStringLiteral("org.bluez.GattCharacteristic1"),
                           QDBusConnection::systemBus());
    dataCh.call(QStringLiteral("StartNotify"));
    notifCh.call(QStringLiteral("StartNotify"));

    QDBusConnection::systemBus().connect(
        QStringLiteral("org.bluez"), m_platform->notifPath,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
        this, SLOT(onBluezNotifProps(QString,QVariantMap,QStringList)));
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.bluez"), m_platform->dataPath,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
        this, SLOT(onBluezDataProps(QString,QVariantMap,QStringList)));
    m_starting = false;
}

#else

void BluetoothAncsClient::requestAttributes(quint32)
{
}

void BluetoothAncsClient::stopSession()
{
    m_address.clear();
    m_dataBuf.clear();
}

void BluetoothAncsClient::startSession(const QString &)
{
    m_starting = false;
}

#endif
