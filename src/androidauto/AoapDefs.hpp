#pragma once

#include <QtGlobal>

namespace Aoap {

constexpr quint16 kGoogleVid = 0x18d1;
constexpr quint16 kAccessoryPids[] = {0x2d00, 0x2d01, 0x2d02, 0x2d03, 0x2d04, 0x2d05};

constexpr quint8 kReqGetProtocol = 51;
constexpr quint8 kReqSendString = 52;
constexpr quint8 kReqStart = 53;

constexpr quint16 kStringManufacturer = 0;
constexpr quint16 kStringModel = 1;
constexpr quint16 kStringDescription = 2;
constexpr quint16 kStringVersion = 3;
constexpr quint16 kStringUri = 4;
constexpr quint16 kStringSerial = 5;

inline constexpr const char *kManufacturer = "Android";
inline constexpr const char *kModel = "Android Auto";
inline constexpr const char *kDescription = "MP157 Wired Android Auto host";
inline constexpr const char *kVersion = "0.1.0";
inline constexpr const char *kUri = "https://github.com/MP157-IVI";
inline constexpr const char *kSerial = "MP157-AA-0001";

enum class State {
    Idle = 0,
    Watching,
    DeviceFound,
    GetProtocol,
    SendStrings,
    StartAccessory,
    WaitReenumerate,
    AccessoryReady,
    SessionOpen,
    Failed,
};

} // namespace Aoap
