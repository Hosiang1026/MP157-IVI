# MP157-IVI

STM32MP157 车机 UI 原型（Qt 6.8，1024×600）。桌面 Shell + 可安装 QML 应用 + 壁纸天气 Shader + Open-Meteo 实况与出行态推断。Windows 上开发与演示；目标部署为板端 Linux 全屏运行 `ivi-shell`。

业务源码：`src/core/`（服务层）+ `src/shell/`（`ivi-shell` + QML）+ `apps/`（内置应用）+ `feed/`（应用商店包）。

## 调用链

```
main.cpp
  注册单例：SystemState / Weather / AppCatalog / MediaSession / NavSession / CallSession / …
  → QQmlApplicationEngine 加载 IviShell/Main.qml

Main.qml
  WallpaperStore 壁纸 Image
  → WeatherFx（ShaderEffect + weather.frag.qsb，随 Weather.kind 切换 wx）
  → StatusBar（时间 / 城市 / 实况 / 温度 / travelAlert）
  → HomeScreen（分页图标 + Dock 拖拽排序）
  → AppStage（Loader 加载 apps/{id}/Main.qml）

WeatherService（无 preview 时）
  IP 定位 → 逆地理 → Open-Meteo forecast（48h hourly）
  + 延迟拉 Open-Meteo warnings（有则 StatusBar travelAlert；国内常为空）
  → 每分钟 updateFromHourly：能见度 / 风·阵风 / 降水 / 地温 + 近 2h 降水
  → resolveDrivingKind 覆盖 WMO 基础码（台风 / 暴雪 / 积水 / 霜 / 湿滑 / 大风 / 扬沙等）
  → emit updated → WeatherFx / StatusBar 刷新

AppCatalog
  扫描 apps/ 与 feed/ → installed / dock / available
  → install / uninstall / Dock 顺序持久化（QSettings）
  POST_BUILD 将 apps、feed、assets 拷到 exe 旁
```

## 目录

| 路径 | 作用 |
| --- | --- |
| `src/core/` | C++ 服务：`WeatherService`、`AppCatalog`、`SystemState`、`MediaSession`、`NavSession`、`VehicleState` 等 |
| `src/shell/main.cpp` | 入口、QML 模块注册 |
| `src/shell/qml/` | `Main`、`HomeScreen`、`StatusBar`、`AppStage`、`WeatherFx` |
| `src/shell/shaders/weather.frag` | 壁纸天气片元着色器源码 |
| `src/shell/weather.frag.qsb` | Qt Shader Tools 编译产物（打进 qrc） |
| `apps/*/` | 内置应用：`manifest.json` + `Main.qml` |
| `feed/*/` | 商店可安装包（如 radio） |
| `assets/wallpapers/` | 内置壁纸 catalog |
| `assets/media/` | 演示用音乐 / 视频文件 |
| `CMakePresets.json` | Windows MSVC + Qt 6.8.3 预设 |
| `build/Debug/ivi-shell.exe` | 本地运行产物（Debug） |

---

## 内置应用

| ID | 名称 | 说明 |
| --- | --- | --- |
| `music` | 音乐 | 本地 WAV 播放，`MediaSession` |
| `video` | 视频 | `VideoScreen` 播演示 AVI |
| `map` | 地图 | 导航占位，`NavSession` |
| `phone` | 电话 | 通话 UI，`CallSession` |
| `vehicle` | 车辆 | 车身状态，`VehicleState` |
| `settings` | 设置 | 主题 / 亮度 / 壁纸 / **天气调试** |
| `store` | 商店 | 从 `feed/` 安装应用 |

`manifest.json` 字段：`name`、`entry`、`color`、`dock`、`dockOrder`、`builtin`、`order`。

---

## 天气

**实况数据（Open-Meteo）**  
hourly / current：`weather_code`、`temperature_2m`、`visibility`、`wind_speed_10m`、`wind_gusts_10m`、`precipitation`、`soil_temperature_0cm`（地温代理路面结冰推断）。

**预警**  
`GET https://api.open-meteo.com/v1/warnings` → `travelAlert`（headline 截断）；无国内蓝黄橙红文案，未接省台 API。

**出行态推断（优先级高于纯 WMO）**

| kind | 条件概要 |
| --- | --- |
| `typhoon` | 预警含台风类关键词，或阵风 ≥75 km/h 且处于降水类 |
| `blizzard` | 降雪类 + 阵风 ≥40，或低温低能见度降雪 |
| `ponding` | 暴雨 / 雷雨，或强降水 / 低能见度雨 |
| `frost` | 气温 ≤2℃ 且地温 ≤1℃，晴到阴 |
| `wetRoad` | 近 2h 有雨、当前几乎无降水，晴~阴 |
| `wind` | 阵风 ≥50 或风速 ≥42 |
| `sandLift` | WMO 36；`dust` 为 30–35 沙尘暴 |

**壁纸 wx（WeatherFx → shader code）**  
1–2 晴/晴夜，3 多云，4 阴，5 雾，6–9 雨级/雷雨，10–12 雪级，13 冰雹，14 霾，15 沙尘，16 雨夹雪，17 冻雨，18 大风，19 暴雪，20 霜，21 扬沙，22 积水，23 湿滑，24 台风。

**调试**  
设置 → 天气调试：点种类后**退回桌面第一屏**看壁纸（设置页 opaque 会挡住壁纸）；选「实况」恢复 Open-Meteo。

**改 shader 后**

```bat
D:\Qt\6.8.3\msvc2022_64\bin\qsb.exe --glsl 100es,120,330,430,440 --hlsl 50 --msl 12 ^
  -o src\shell\weather.frag.qsb src\shell\shaders\weather.frag
cmake --build build --config Debug --target ivi-shell
```

---

## 界面

| 区域 | 说明 |
| --- | --- |
| StatusBar | 时钟、城市、天气文字、温度、预警摘要；应用内为浅色底 |
| HomeScreen | 左右滑页 + 底 Dock；长按图标拖入/拖出 Dock，`AppCatalog` 持久化 |
| AppStage | 全屏应用；底部条关闭回桌面 |
| WeatherFx | 仅桌面可见（或设置 preview 非空时）；`pagePos` 随翻页淡出 |

分辨率默认 **1024×600**；Linux 启动时 `Main.qml` 可切全屏。

---

## 构建与运行

**依赖**  
Qt **6.8.3**（MSVC 2022 x64）：Quick、QuickControls2、Network。预设里 `CMAKE_PREFIX_PATH` 指向本机 Qt 安装，可按环境修改 `CMakePresets.json`。

**配置 / 编译**

```bat
cmake --preset windows
cmake --build --preset windows --target ivi-shell
```

**运行**

```bat
cd build\Debug
ivi-shell.exe
```

工作目录须在 `ivi-shell.exe` 同级（含 `apps/`、`feed/`、`wallpapers/`、`media/`、`translations/`）。POST_BUILD 已自动复制资源；改 `apps/` 或 `assets/` 后需重新 build `ivi-shell` 或手动拷到 `build/Debug/`。

**Linux / MP157 目标**  
烧写 OpenSTLinux、交叉编译与上板部署见 **[docs/STM32MP157-烧写与部署.md](docs/STM32MP157-烧写与部署.md)**（从零开始，含 CubeProgrammer、目录布局、systemd）。

---

## 配置与持久化

| 项 | 存储 |
| --- | --- |
| 壁纸当前项 | QSettings `wallpaper` |
| 自动/手动深色 | QSettings `autoTheme` / `manualDark` |
| Dock / 桌面图标顺序 | QSettings（`AppCatalog`） |
| 天气调试 preview | **不持久化**，仅当次会话 |

---

## 现状说明

当前为 **可演示的 IVI Shell 原型**，非量产车机。已有桌面框架、多应用、壁纸天气、模拟车控/导航/电话；**未**在 MP157 上完成 eglfs 量产验证，**未**接真实总线、蓝牙、地图 SDK、CarPlay/AA 认证体系。

---

## 未来计划（量产车机）

### 阶段 A — 工程内可实施（优先）

| 项 | 现状 | 计划 |
| --- | --- | --- |
| MP157 部署 | `cmake/stm32mp157-toolchain.cmake` + `scripts/build-mp157.sh` / `pack-mp157.sh` / `board-probe` | 真机 eglfs 联调、Yocto recipe |
| 车身数据 | `VehicleState` 定时模拟 | SocketCAN / 厂商 DBC，车速/档位/门/灯/胎压等接真实信号 |
| 定位与天气 | IP 定位 + Open-Meteo | GNSS 串口/USB；可选国内预警 API |
| 蓝牙 | 未实现 | BlueZ：HFP 电话、A2DP 音乐，对接 `CallSession` / `MediaSession` |
| 导航 | `NavSession` 假步骤 | 接车机地图 SDK（高德/百度/Mapbox 等，含授权）或离线瓦片 |
| 音频 | WAV + 简单 `AudioFocus` | 倒车/导航 ducking、多源混音、与 PipeWire/Pulse 策略 |
| 镜像与交付 | 无 Yocto 层 | OpenSTLinux/Yocto recipe，固定内核与 rootfs |
| 稳定性 | 无 | 看门狗、崩溃重启、日志采集、Shader/内存压测 |
| OTA | 无 | 签名校验、A/B 或分区升级（应用层可先简易包） |

**建议实施顺序**：MP157 稳定运行 → CAN → 蓝牙 → 导航 SDK → OTA/看门狗 → 再评估手机互联。

### 阶段 B — 可实施但周期长（协议 / 授权 / 联调）

| 项 | 说明 |
| --- | --- |
| CarPlay | MFi 或 USB dongle 方案；Host 模式、H.264、触控/旋钮规范 |
| Android Auto | Google 认证与 USB 协议栈 |
| 语音 | 量产 TTS/ASR SDK（离线包、唤醒词） |
| 在线 DRM 视频 | 多依赖投屏，车机侧一般不自建 |
| 多屏 / 副驾 | 依赖硬件与 DRM 多路输出 |

### 阶段 C — 依赖 OEM / 实验室（非本仓库可单独闭环）

| 项 | 说明 |
| --- | --- |
| 功能安全 ISO 26262 | 若 HMI 涉及安全相关显示，需整车体系与证据 |
| 网络安全 R155/R156、国标 | 安全启动、密钥、渗透与备案 |
| 无线电 / 蓝牙整机认证 | SRRC、CCC 等，模块已认证仍可能要做整机 |
| 地图与数据合规 | 审图号、个人信息与车机日志合规 |

### 当前模拟 / 占位（计划替换为真实实现）

| 模块 | 文件 / 应用 | 当前行为 |
| --- | --- | --- |
| 导航 | `NavSession`、`apps/map` | 定时切换假引导文案 |
| 车身 | `VehicleState`、`apps/vehicle` | 本地模拟车速等 |
| 电话 | `CallSession`、`apps/phone` | UI 级模拟 |
| 视频 | `VideoScreen` | 自绘 AVI，非硬件解码管线 |

---

## 与 CarPlay / MP157 的关系

本仓库 **Shell 与 HMI** 可作为 MP157 上车界面基础；CarPlay/Android Auto、认证与 rootfs 与上述 **阶段 B/C** 同步规划，见 [docs/STM32MP157-烧写与部署.md](docs/STM32MP157-烧写与部署.md)。
