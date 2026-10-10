#include "AoapTransport.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <initguid.h>
#include <usbiodef.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#endif

namespace {

bool envDemo()
{
    return qEnvironmentVariableIsSet("IVI_AA_DEMO");
}

} // namespace

AoapTransport::AoapTransport(QObject *parent)
    : QObject(parent)
{
    m_watchTimer.setInterval(1200);
    connect(&m_watchTimer, &QTimer::timeout, this, &AoapTransport::tickWatch);
}

AoapTransport::~AoapTransport()
{
    stop();
}

QString AoapTransport::stateName() const
{
    switch (m_state) {
    case Aoap::State::Idle: return QStringLiteral("Idle");
    case Aoap::State::Watching: return QStringLiteral("Watching");
    case Aoap::State::DeviceFound: return QStringLiteral("DeviceFound");
    case Aoap::State::GetProtocol: return QStringLiteral("GetProtocol");
    case Aoap::State::SendStrings: return QStringLiteral("SendStrings");
    case Aoap::State::StartAccessory: return QStringLiteral("StartAccessory");
    case Aoap::State::WaitReenumerate: return QStringLiteral("WaitReenumerate");
    case Aoap::State::AccessoryReady: return QStringLiteral("AccessoryReady");
    case Aoap::State::SessionOpen: return QStringLiteral("SessionOpen");
    case Aoap::State::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

bool AoapTransport::accessoryOpen() const
{
#ifdef Q_OS_LINUX
    return m_fd >= 0;
#elif defined(Q_OS_WIN)
    return m_winUsb != nullptr;
#else
    return false;
#endif
}

void AoapTransport::startWatch()
{
    if (m_watching)
        return;
    m_watching = true;
    setDeviceLabel({});
    setState(Aoap::State::Watching);
    if (envDemo()) {
        setDetail(QStringLiteral("演示模式 IVI_AA_DEMO"));
        setDeviceLabel(QStringLiteral("Demo Phone"));
        setState(Aoap::State::AccessoryReady);
        emit accessoryReady(m_deviceLabel);
        return;
    }
    setDetail(QStringLiteral("等待手机 USB"));
    m_watchTimer.start();
    tickWatch();
}

void AoapTransport::stop()
{
    m_watching = false;
    m_watchTimer.stop();
    closeHandles();
    setDeviceLabel({});
    setState(Aoap::State::Idle);
    setDetail({});
}

void AoapTransport::setState(Aoap::State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void AoapTransport::setDetail(const QString &detail)
{
    if (m_detail == detail)
        return;
    m_detail = detail;
    emit detailChanged();
}

void AoapTransport::setDeviceLabel(const QString &label)
{
    if (m_deviceLabel == label)
        return;
    m_deviceLabel = label;
    emit deviceLabelChanged();
}

bool AoapTransport::isAccessoryPid(quint16 vid, quint16 pid) const
{
    if (vid != Aoap::kGoogleVid)
        return false;
    for (quint16 p : Aoap::kAccessoryPids) {
        if (p == pid)
            return true;
    }
    return false;
}

void AoapTransport::closeHandles()
{
#ifdef Q_OS_LINUX
    if (m_fd >= 0) {
        if (m_iface >= 0)
            ::ioctl(m_fd, USBDEVFS_RELEASEINTERFACE, &m_iface);
        ::close(m_fd);
        m_fd = -1;
    }
#endif
#ifdef Q_OS_WIN
    if (m_winUsb) {
        WinUsb_Free(static_cast<WINUSB_INTERFACE_HANDLE>(m_winUsb));
        m_winUsb = nullptr;
    }
    if (m_winHandle) {
        CloseHandle(static_cast<HANDLE>(m_winHandle));
        m_winHandle = nullptr;
    }
#endif
    m_openPath.clear();
    m_epIn = -1;
    m_epOut = -1;
    m_iface = 0;
}

void AoapTransport::tickWatch()
{
    if (!m_watching || (m_state != Aoap::State::Watching && m_state != Aoap::State::WaitReenumerate))
        return;
    if (!probeOnce())
        return;
    m_watchTimer.stop();
}

QList<AoapTransport::UsbDev> AoapTransport::enumerateDevices() const
{
    QList<UsbDev> out;
#ifdef Q_OS_LINUX
    const QDir bus(QStringLiteral("/dev/bus/usb"));
    if (!bus.exists())
        return out;
    const QFileInfoList buses = bus.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &b : buses) {
        const QDir d(b.absoluteFilePath());
        for (const QFileInfo &f : d.entryInfoList(QDir::Files)) {
            UsbDev dev;
            dev.path = f.absoluteFilePath();
            // sysfs lookup by bus/dev
            const QString busNum = b.fileName();
            const QString devNum = f.fileName();
            const QString sysGlob = QStringLiteral("/sys/bus/usb/devices");
            for (const QFileInfo &sys : QDir(sysGlob).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                QFile busF(sys.absoluteFilePath() + QStringLiteral("/busnum"));
                QFile devF(sys.absoluteFilePath() + QStringLiteral("/devnum"));
                QFile vidF(sys.absoluteFilePath() + QStringLiteral("/idVendor"));
                QFile pidF(sys.absoluteFilePath() + QStringLiteral("/idProduct"));
                if (!busF.open(QIODevice::ReadOnly) || !devF.open(QIODevice::ReadOnly)
                    || !vidF.open(QIODevice::ReadOnly) || !pidF.open(QIODevice::ReadOnly))
                    continue;
                if (QString::fromLatin1(busF.readAll()).trimmed().toInt() != busNum.toInt())
                    continue;
                if (QString::fromLatin1(devF.readAll()).trimmed().toInt() != devNum.toInt())
                    continue;
                bool okV = false;
                bool okP = false;
                dev.vid = quint16(QString::fromLatin1(vidF.readAll()).trimmed().toUInt(&okV, 16));
                dev.pid = quint16(QString::fromLatin1(pidF.readAll()).trimmed().toUInt(&okP, 16));
                if (!okV || !okP)
                    continue;
                QFile prod(sys.absoluteFilePath() + QStringLiteral("/product"));
                if (prod.open(QIODevice::ReadOnly))
                    dev.name = QString::fromUtf8(prod.readAll()).trimmed();
                break;
            }
            if (dev.vid == 0)
                continue;
            dev.accessory = isAccessoryPid(dev.vid, dev.pid);
            if (dev.name.isEmpty())
                dev.name = QStringLiteral("%1:%2").arg(dev.vid, 4, 16, QLatin1Char('0')).arg(dev.pid, 4, 16, QLatin1Char('0'));
            out.append(dev);
        }
    }
#elif defined(Q_OS_WIN)
    HDEVINFO info = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return out;
    SP_DEVICE_INTERFACE_DATA ifData{};
    ifData.cbSize = sizeof(ifData);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, i, &ifData); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &ifData, nullptr, 0, &need, nullptr);
        if (need == 0)
            continue;
        QByteArray buf(int(need), 0);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        SP_DEVINFO_DATA devInfo{};
        devInfo.cbSize = sizeof(devInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &ifData, detail, need, nullptr, &devInfo))
            continue;
        UsbDev dev;
        dev.path = QString::fromWCharArray(detail->DevicePath);
        WCHAR idBuf[256]{};
        if (SetupDiGetDeviceInstanceIdW(info, &devInfo, idBuf, 256, nullptr)) {
            const QString id = QString::fromWCharArray(idBuf).toUpper();
            const int v = id.indexOf(QStringLiteral("VID_"));
            const int p = id.indexOf(QStringLiteral("PID_"));
            if (v >= 0)
                dev.vid = quint16(id.mid(v + 4, 4).toUInt(nullptr, 16));
            if (p >= 0)
                dev.pid = quint16(id.mid(p + 4, 4).toUInt(nullptr, 16));
        }
        if (dev.vid == 0)
            continue;
        dev.accessory = isAccessoryPid(dev.vid, dev.pid);
        dev.name = QStringLiteral("%1:%2").arg(dev.vid, 4, 16, QLatin1Char('0')).arg(dev.pid, 4, 16, QLatin1Char('0'));
        out.append(dev);
    }
    SetupDiDestroyDeviceInfoList(info);
#endif
    return out;
}

bool AoapTransport::controlTransfer(const QString &path, quint8 reqType, quint8 request, quint16 value,
                                    quint16 index, QByteArray *data, int timeoutMs)
{
#ifdef Q_OS_LINUX
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return false;
    usbdevfs_ctrltransfer ctrl{};
    ctrl.bRequestType = reqType;
    ctrl.bRequest = request;
    ctrl.wValue = value;
    ctrl.wIndex = index;
    ctrl.wLength = data ? quint16(data->size()) : 0;
    ctrl.timeout = timeoutMs;
    ctrl.data = data && !data->isEmpty() ? data->data() : nullptr;
    const int rc = ::ioctl(fd, USBDEVFS_CONTROL, &ctrl);
    ::close(fd);
    return rc >= 0;
#elif defined(Q_OS_WIN)
    Q_UNUSED(timeoutMs);
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_WRITE | GENERIC_READ,
                           FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    WINUSB_INTERFACE_HANDLE winusb = nullptr;
    bool ok = false;
    if (WinUsb_Initialize(h, &winusb)) {
        WINUSB_SETUP_PACKET setup{};
        setup.RequestType = reqType;
        setup.Request = request;
        setup.Value = value;
        setup.Index = index;
        setup.Length = data ? USHORT(data->size()) : 0;
        ULONG transferred = 0;
        ok = WinUsb_ControlTransfer(winusb, setup, data ? reinterpret_cast<PUCHAR>(data->data()) : nullptr,
                                    setup.Length, &transferred, nullptr);
        WinUsb_Free(winusb);
    }
    CloseHandle(h);
    return ok;
#else
    Q_UNUSED(path);
    Q_UNUSED(reqType);
    Q_UNUSED(request);
    Q_UNUSED(value);
    Q_UNUSED(index);
    Q_UNUSED(data);
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

bool AoapTransport::sendAoapStrings(const QString &path)
{
    auto send = [&](quint16 id, const char *text) {
        QByteArray data(text);
        data.append('\0');
        return controlTransfer(path, 0x40, Aoap::kReqSendString, 0, id, &data);
    };
    setState(Aoap::State::SendStrings);
    return send(Aoap::kStringManufacturer, Aoap::kManufacturer)
        && send(Aoap::kStringModel, Aoap::kModel)
        && send(Aoap::kStringDescription, Aoap::kDescription)
        && send(Aoap::kStringVersion, Aoap::kVersion)
        && send(Aoap::kStringUri, Aoap::kUri)
        && send(Aoap::kStringSerial, Aoap::kSerial);
}

bool AoapTransport::switchToAccessory(const UsbDev &dev)
{
    setState(Aoap::State::DeviceFound);
    setDeviceLabel(dev.name);
    setDetail(QStringLiteral("查询 AOAP 协议"));
    setState(Aoap::State::GetProtocol);
    QByteArray proto(2, 0);
    if (!controlTransfer(dev.path, 0xC0, Aoap::kReqGetProtocol, 0, 0, &proto)) {
        setDetail(QStringLiteral("GET_PROTOCOL 失败（需 root/WinUSB）"));
        return false;
    }
    const int version = int(quint8(proto[0]) | (quint8(proto[1]) << 8));
    if (version < 1) {
        setDetail(QStringLiteral("设备不支持 AOAP"));
        return false;
    }
    setDetail(QStringLiteral("AOAP v%1 · 发送标识").arg(version));
    if (!sendAoapStrings(dev.path)) {
        setDetail(QStringLiteral("SEND_STRING 失败"));
        return false;
    }
    setState(Aoap::State::StartAccessory);
    QByteArray empty;
    if (!controlTransfer(dev.path, 0x40, Aoap::kReqStart, 0, 0, &empty)) {
        setDetail(QStringLiteral("START accessory 失败"));
        return false;
    }
    setState(Aoap::State::WaitReenumerate);
    setDetail(QStringLiteral("等待手机重枚举为附件"));
    return true;
}

bool AoapTransport::claimAccessoryInterface(const QString &path)
{
#ifdef Q_OS_LINUX
    closeHandles();
    m_fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0)
        return false;

    // Read device descriptor / config to find bulk endpoints (simple scan of raw descriptors via USBDEVFS_CONNECTINFO not enough).
    // Use USBDEVFS_IOCTL less; parse from sysfs endpoint files when possible.
    m_iface = 0;
    m_epIn = 0x81;
    m_epOut = 0x01;
    // Prefer discovering from /sys
    // Fallback defaults used by many AOA devices: ep 0x81 / 0x01
    if (::ioctl(m_fd, USBDEVFS_CLAIMINTERFACE, &m_iface) < 0) {
        // try iface 0 after disconnect kernel driver
        usbdevfs_ioctl command{};
        command.ifno = 0;
        command.ioctl_code = USBDEVFS_DISCONNECT;
        command.data = nullptr;
        ::ioctl(m_fd, USBDEVFS_IOCTL, &command);
        if (::ioctl(m_fd, USBDEVFS_CLAIMINTERFACE, &m_iface) < 0) {
            closeHandles();
            return false;
        }
    }
    m_openPath = path;
    return true;
#elif defined(Q_OS_WIN)
    closeHandles();
    HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_WRITE | GENERIC_READ,
                           FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    WINUSB_INTERFACE_HANDLE winusb = nullptr;
    if (!WinUsb_Initialize(h, &winusb)) {
        CloseHandle(h);
        return false;
    }
    USB_INTERFACE_DESCRIPTOR iface{};
    if (WinUsb_QueryInterfaceSettings(winusb, 0, &iface)) {
        for (UCHAR i = 0; i < iface.bNumEndpoints; ++i) {
            WINUSB_PIPE_INFORMATION pipe{};
            if (!WinUsb_QueryPipe(winusb, 0, i, &pipe))
                continue;
            if (pipe.PipeType != UsbdPipeTypeBulk)
                continue;
            if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId))
                m_epIn = pipe.PipeId;
            else
                m_epOut = pipe.PipeId;
        }
    }
    if (m_epIn < 0)
        m_epIn = 0x81;
    if (m_epOut < 0)
        m_epOut = 0x01;
    m_winHandle = h;
    m_winUsb = winusb;
    m_openPath = path;
    return true;
#else
    Q_UNUSED(path);
    return false;
#endif
}

bool AoapTransport::openAccessory(const UsbDev &dev)
{
    setState(Aoap::State::AccessoryReady);
    setDeviceLabel(dev.name);
    if (!claimAccessoryInterface(dev.path)) {
        setDetail(QStringLiteral("打开附件接口失败"));
        return false;
    }
    setDetail(QStringLiteral("附件已打开 epIn=0x%1 epOut=0x%2")
                  .arg(m_epIn, 0, 16)
                  .arg(m_epOut, 0, 16));
    return true;
}

bool AoapTransport::probeOnce()
{
    const QList<UsbDev> devices = enumerateDevices();
    if (devices.isEmpty()) {
        setDetail(QStringLiteral("未发现 USB 设备"));
        return false;
    }

    for (const UsbDev &dev : devices) {
        if (!dev.accessory)
            continue;
        if (openAccessory(dev)) {
            m_watching = true;
            setState(Aoap::State::AccessoryReady);
            emit accessoryReady(m_deviceLabel);
            return true;
        }
    }

    if (m_state == Aoap::State::WaitReenumerate) {
        setDetail(QStringLiteral("仍在等待附件重枚举…"));
        return false;
    }

    for (const UsbDev &dev : devices) {
        if (dev.accessory)
            continue;
        // Skip hubs/root
        if (dev.vid == 0x1d6b)
            continue;
        setDetail(QStringLiteral("尝试 %1").arg(dev.name));
        if (switchToAccessory(dev)) {
            m_watchTimer.start();
            return false; // wait reenumerate
        }
    }

    setDetail(QStringLiteral("未找到可切换 AOAP 的手机"));
    return false;
}

qint64 AoapTransport::writeBulk(const QByteArray &data)
{
    if (data.isEmpty() || !accessoryOpen())
        return -1;
#ifdef Q_OS_LINUX
    usbdevfs_bulktransfer bulk{};
    bulk.ep = m_epOut;
    bulk.len = data.size();
    bulk.timeout = 1000;
    bulk.data = const_cast<char *>(data.constData());
    if (::ioctl(m_fd, USBDEVFS_BULK, &bulk) < 0)
        return -1;
    emit bulkActivity(0, int(bulk.len));
    return qint64(bulk.len);
#elif defined(Q_OS_WIN)
    ULONG transferred = 0;
    if (!WinUsb_WritePipe(static_cast<WINUSB_INTERFACE_HANDLE>(m_winUsb), UCHAR(m_epOut),
                          reinterpret_cast<PUCHAR>(const_cast<char *>(data.constData())),
                          ULONG(data.size()), &transferred, nullptr))
        return -1;
    emit bulkActivity(0, int(transferred));
    return qint64(transferred);
#else
    return -1;
#endif
}

QByteArray AoapTransport::readBulk(int maxBytes, int timeoutMs)
{
    if (maxBytes <= 0 || !accessoryOpen())
        return {};
    QByteArray buf(maxBytes, Qt::Uninitialized);
#ifdef Q_OS_LINUX
    usbdevfs_bulktransfer bulk{};
    bulk.ep = m_epIn;
    bulk.len = maxBytes;
    bulk.timeout = timeoutMs;
    bulk.data = buf.data();
    const int rc = ::ioctl(m_fd, USBDEVFS_BULK, &bulk);
    if (rc < 0)
        return {};
    const int n = int(bulk.len);
    if (n <= 0 || n > maxBytes)
        return {};
    buf.resize(n);
    emit bulkActivity(n, 0);
    return buf;
#elif defined(Q_OS_WIN)
    ULONG transferred = 0;
    if (!WinUsb_ReadPipe(static_cast<WINUSB_INTERFACE_HANDLE>(m_winUsb), UCHAR(m_epIn),
                         reinterpret_cast<PUCHAR>(buf.data()), ULONG(maxBytes), &transferred, nullptr))
        return {};
    buf.resize(int(transferred));
    emit bulkActivity(int(transferred), 0);
    return buf;
#else
    Q_UNUSED(timeoutMs);
    return {};
#endif
}
