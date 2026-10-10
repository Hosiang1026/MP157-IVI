# MP157-IVI

STM32MP157 车机 UI 原型（Qt 6.8，1024×600）。桌面 Shell + 可安装 QML 应用 + 壁纸天气 Shader + Open-Meteo + **无线 CarPlay** / **Android Auto（有线 AOAP）** / **网络电台** / **文件管理** / **行车记录仪** / **地图瓦片**（Windows 演示）。目标部署为板端 Linux 全屏运行 `ivi-shell`。

业务源码：`src/core/` + `src/carplay/` + `src/androidauto/` + `src/shell/` + `apps/` + `feed/`。

当前版本：**v2.0.0.20261010**

## 更新日志

### v2.0.0.20261010

**相对 v1.0.0 的主要变化**
- 新增内置应用：`androidauto`（有线 USB AOAP）、`radio`（网络电台）、`files`（文件浏览/拷贝/局域网分享）
- 移除内置应用入口：`airplay`、`dlna`、`store`（后端 `AirPlayMirror` / `DlnaRenderer` / `feed` 安装通路仍保留）
- 新增服务：`AndroidAutoSession`、`RadioSession`、`FileBrowser`、`GpsSource`、`BluetoothMediaHub`
- Shell：开机 `BootSplash`、空闲 `LockScreen`、拼音 `VirtualKeyboard`、Ios* 控件集；方向盘/媒体键路由电台与音乐
- CarPlay 与 Android Auto 互斥；天气/CarPlay 可接 `GpsSource`；音乐支持蓝牙音源
- Linux：`ivi-shell.log` 轮转 + systemd READY/WATCHDOG

**工程与入口**
- Qt 6.8 / C++17：`src/core`、`src/carplay`、`src/androidauto`、`src/shell`、`apps`、`feed`、`assets`、`scripts`、`docs`
- `main.cpp` 注册单例：`SystemState`、`UpdateService`、`AudioFocus`、`Weather`、`VehicleState`、`CameraService`、`MediaSession`、`BluetoothMediaHub`、`NavSession`、`MapTiles`、`CallSession`、`RadioSession`、`FileBrowser`、`GpsSource`、`AppCatalog`、`WallpaperStore`、`CarPlaySession`、`AndroidAutoSession`、`AirPlayMirror`、`DlnaRenderer`
- 注册类型：`VideoScreen`、`CarPlayVideoItem`、`AirPlayVideoItem`、`DlnaVideoItem`、`CameraVideoItem`
- ImageProvider：`image://carplay`、`image://dlna`
- Linux 预置 `QT_QPA_EGLFS_PHYSICAL_*`、`QT_QPA_FB_FORCE_FULLSCREEN`；Win 字体 YaHei UI，Linux 优先 Noto Sans CJK；`QQuickStyle` Basic；组织名 MP157 / 应用名 ivi-shell

**Shell UI**
- `Main.qml`：壁纸 + `WeatherFx` + `StatusBar` + 多任务抽屉 + `HomeScreen` / `AppStage`；亮度遮罩；Linux 全屏
- `BootSplash` / `LockScreen`（空闲锁屏，媒体/导航/投屏/通话/倒车时阻止）；`VirtualKeyboard` + `pinyin.js`
- CarPlay 有视频 / 倒车影像时隐藏 StatusBar、亮度层、底条
- 底条 Home：上滑或点击关闭应用；CarPlay `hostUiRequested` 自动关应用层
- `HomeScreen`：第 0 页壁纸+Dock，后续页 5×3 网格；页码点；横向分页
- Dock/桌面长按拖拽：加移出 Dock（最多 5）、调序、边缘翻页
- `AppStage`：多应用 Loader 缓存、`running`/`background`；关 music 暂停、关 map 停导航；加载失败提示
- `StatusBar`：时钟、城市、天气温度、`travelAlert`；导航/音乐 Hub；CarPlay / Android Auto / AirPlay 链路指示；蓝牙/Wi‑Fi/电量/信号；多任务入口
- `WeatherFx`：`weather.frag.qsb`，kind→wx 1–24，随 `pagePos` 淡出，支持 `Weather.preview`
- `GlassPanel`：壁纸采样模糊圆角（Dock / 任务栏）
- `AppIcon` / `AppGlyph`：manifest 图标或 Canvas 矢量（含 carplay/androidauto/radio/files/weather 等）
- Ios*：`IosPressable` / `IosToggle` / `IosSpinner` / `IosSearchField` / `IosSegmented` / `IosGroupedCard` / `IosEmptyState` / `IosIcon`

**内置应用**
- `carplay`：Wi‑Fi/蓝牙配对选择、开始/断开、全屏画面+触控、MFi 前置校验、`carplay.log`；与 AA 互斥
- `androidauto`：有线 USB AOAP 会话（`AndroidAutoSession` / `AoapTransport`），与 CarPlay 互斥
- `radio`：网络电台列表，`RadioSession` + `FfmpegUrlPlayer`；与本地音乐互斥
- `files`：多根目录浏览/进入/删除/拷贝；可播放项交给音乐/视频；局域网 HTTP 分享
- `dashcam`：设备选择、预览、录 AVI、录像列表；`acquire/release`
- `weather`：多城市、搜索、小时/日预报、UV/风/能见度/体感/湿度，按 kind 渐变 UI
- `map`：瓦片拖拽缩放、车标/目的地、Open-Meteo 搜索、导航开始/结束（步骤演示）
- `music`：`media/` WAV+LRC，队列/列表·单曲·随机；`BluetoothMediaHub` 蓝牙音源；StatusBar 音乐 Hub
- `video`：`VideoScreen` 演示 AVI 库与播放；播时暂停音乐
- `phone`：拨号盘、联系人/最近、模拟通话/静音/免提
- `vehicle`：速度/档位 PRND、锁车/灯、胎压告警、油量续航外温等；R 档倒车
- `settings`：蓝牙/Wi‑Fi、主题、亮度音量、壁纸、GPS（demo/串口 NMEA）、版本号、**应用更新**；连点 7 次开发者模式→天气调试

**核心服务**
- `AppCatalog`/`AppListModel`：扫描 apps+feed；install 从 feed 复制；dock/homeOrder 持久化
- `SystemState`：亮度/音量/蓝牙/Wi‑Fi/主题色板/开发者模式/锁屏超时/时钟/`pref`；`appVersion` 读 `version.json`；Win WlanAPI+电量，Linux 背光 sysfs
- `UpdateService`：拉更新 manifest → 下载 tar.gz → SHA256 校验 → 解压合并 → 退出并由脚本重启；地址 `update/manifestUrl` 或 `IVI_UPDATE_MANIFEST`
- `WeatherService`：IP/GPS 定位→Open-Meteo forecast/warnings/geocoding；WMO→kind；出行态；多城；`weather-cache.json`；preview 不持久化
- `GpsSource`：demo 位或串口 NMEA；供天气/CarPlay；设置页开关
- `MapTiles`：TileServer GL（`IVI_TILE_URL`，默认 :1999）探测，回退 `maps/tiles`；pan/zoom/搜索/目的地
- `NavSession`：定时轮换演示引导文案（非真实路径）
- `MediaSession`：扫描 WAV；Win waveOut / Linux ALSA（可选）；队列与 playMode 持久化；可接蓝牙音源名
- `BluetoothMediaHub`：蓝牙设备列表/选中/配对；驱动 `MediaSession` 蓝牙源
- `RadioSession`：电台列表 + FFmpeg 拉流；AudioFocus；与 `MediaSession` 互斥
- `FileBrowser`：roots/浏览/拷贝/删除/打开；HTTP 分享；可请求打开 music/video
- `CallSession`：模拟通话；`phone/recents`；AudioFocus 优先级 10
- `VehicleState`：行车模拟；`vehicle/*` 持久化；外温同步天气
- `CameraService`：holder（dashcam/reverse）；Win MF / Linux V4L2；无设备 demoMode；MJPEG→AVI
- `AndroidAutoSession`：Win AOAP（setupapi/winusb）；会话探测；无视频帧（原型）
- `DlnaRenderer` / `AirPlayMirrorSession`：后端仍注册（无独立应用入口）
- `FfmpegUrlPlayer`：FFmpeg 拉 URL 解码（电台 / DLNA 等）
- `AudioFocus`：多客户端 request/release、ducking
- `WallpaperStore`：builtin catalog + user；当前壁纸持久化；上传仅 Windows

**CarPlay / Android Auto**
- `CarPlaySession`：`LocalMfiAuth`（offline-mfi）、`AirPlayIdentity`、`AirPlayServer`、Bonjour、iAP2；可挂 `GpsSource` / `MediaSession` / `NavSession` / `CallSession`
- Wi‑Fi：`ExistingWifi`（Win WLAN）；蓝牙：`BluetoothDevices` / `BluetoothRfcomm` / `BluetoothMediaHub`
- AirPlay 栈：Crypto/AuthSetup/ControlCipher/Bplist/ScreenStream/AudioStream/BufferedAudio/PcmPlayer/H264Decoder；Monocypher
- `AirPlayMirrorSession`：独立 Bonjour（MP157-AirPlay），与 CarPlay 互斥端口/mDNS
- `AndroidAutoSession`：有线 AOAP；与 CarPlay 互斥
- QSettings `carplay/*`；exe 旁 `carplay.log`
- Linux：OpenSSL MFi 已接；板端 BlueZ/WLAN 主机栈未接

**天气 kind / wx**
- WMO→clear/cloudy/overcast/haze/dust/sandLift/fog/rain*/freezeRain/sleet/snow*/hail/thunder
- 出行叠加：typhoon、blizzard、ponding、frost、wetRoad、wind
- Shader wx 1–24；设置 preview 含 night 等芯片

**地图脚本与资源**
- `start-tileserver.ps1` / `stop-tileserver.ps1`：WSL Docker `maptiler/tileserver-gl`，宿主 1999，容器 `ivi-tileserver`
- `mbtiles-to-xyz.py`、`gen-map-tiles.py`、`inspect-mbtiles.py`
- 默认示例 `.mbtiles`（杭州包）+ `assets/maps/tiles/` 回退 PNG

**版本与 OTA**
- 根目录 `version.json`（`version`/`channel`）；POST_BUILD / install / `pack-mp157.sh` 一并打包
- `pack-mp157.sh` 额外生成同目录更新描述 JSON；`IVI_UPDATE_BASE_URL`、`IVI_UPDATE_MANIFEST_OUT`
- 示例：`deploy/update-manifest.example.json`
- 合并项：`apps`/`feed`/`media`/`maps`/`offline-mfi`/`version.json`/`ivi-shell`/`board-probe`/`run-ivi-shell.sh`/`qt` 等
- 应用内：设置页配置 manifest URL、检查更新、下载进度、取消；非 A/B 分区、非安全启动签名链

**构建部署**
- `CMakePresets.json`：`windows`（VS2022+Qt6.8）、`mp157`（Ninja+toolchain，`/opt/ivi`，board-probe）
- POST_BUILD：拷 `apps`/`feed`/`wallpapers`/`media`/`maps`/`offline-mfi`/`skoda-oem-icon.png`/`version.json`；Win FFmpeg DLL + windeployqt
- install：壁纸/媒资/地图、`version.json`、`run-ivi-shell.sh`、`mp157-board-check.sh`、`ivi-shell.service`
- `build-mp157.sh`、`pack-mp157.sh`、`docker-build-mp157.sh`、`setup-wsl-mp157.sh`、`Dockerfile.mp157`
- `tools/board-probe`（可选 fb0）；`docs/STM32MP157-烧写与部署.md`

**持久化**
- `wallpaper`、`autoTheme`/`manualDark`、`brightness`/`volume`、`bluetooth`/`wifi`、`developerMode`、`lockTimeout`
- `dock`/`homeOrder`；`carplay/*`；`media/*`；`phone/recents`；`weather/cities`/`cityIndex`；`vehicle/*`；`camera/device`；`gps/*`
- `update/manifestUrl`；`weather-cache.json`（exe 旁）；`Weather.preview` 不落盘

**平台差异与限制**
- Windows：WLAN/蓝牙/MF/摄像头/waveOut/壁纸上传/FFmpeg DLL/TileServer（WSL）/AOAP 为完整演示路径；OTA 依赖本机 `tar`
- Linux：全屏 eglfs/linuxfb、背光 sysfs、可选 ALSA、OpenSSL MFi、systemd 看门狗；无 Win 级蓝牙/WLAN/AOAP UI；宜预置 XYZ 瓦片；OTA 可用 systemd 重启 `ivi-shell`
- 导航/电话/车身/VideoScreen/Android Auto 为演示或链路探测；CarPlay/AirPlay 非 Apple 认证；AA 非 Google 认证；MP157 eglfs 量产未验；地图商用需合规；OTA 为应用包级

### v1.0.0.20261009

初版：Shell + 多应用 + 天气 Shader + 地图瓦片 + 无线 CarPlay / AirPlay / DLNA / 行车记录仪 / 应用包 OTA。详见该版本源码树。

## 调用链

```
main.cpp
  注册单例：SystemState / UpdateService / Weather / AppCatalog / MediaSession / BluetoothMediaHub
            / NavSession / MapTiles / CallSession / RadioSession / FileBrowser / GpsSource
            / VehicleState / CameraService / CarPlaySession / AndroidAutoSession
            / AirPlayMirror / DlnaRenderer / …
  注册类型：VideoScreen / CarPlayVideoItem / AirPlayVideoItem / DlnaVideoItem / CameraVideoItem
  image://carplay、image://dlna
  → QQmlApplicationEngine 加载 IviShell/Main.qml

Main.qml
  BootSplash → WallpaperStore 壁纸 Image
   → WeatherFx（ShaderEffect + weather.frag.qsb）
   → StatusBar / HomeScreen / AppStage / LockScreen / VirtualKeyboard
   → 倒车：CameraService.reverseActive 时全屏 CameraVideoItem

WeatherService
  IP 或 GpsSource → Open-Meteo → 出行态 kind → WeatherFx / StatusBar / apps/weather

AppCatalog
  扫描 apps/ 与 feed/ → Dock 持久化；POST_BUILD 拷 apps、feed、assets、maps、offline-mfi、version.json

UpdateService（apps/settings）
  version.json → 检查 manifest → 下载包 → sha256 → tar 解压合并 → 脚本重启

CarPlaySession（apps/carplay）
  offline-mfi → Wi‑Fi + 蓝牙 → iAP2 → AirPlayServer → 画面/触控；与 AndroidAutoSession 互斥
  日志：carplay.log

AndroidAutoSession（apps/androidauto）
  USB AOAP → 会话探测（原型无视频帧）；与 CarPlay 互斥

RadioSession（apps/radio）
  电台 URL → FfmpegUrlPlayer → AudioFocus；与 MediaSession 互斥

FileBrowser（apps/files）
  roots 浏览 / 拷贝 / 分享 HTTP；可打开 music/video

CameraService（apps/dashcam + 倒车）
  采集/演示画面 → 可选录 AVI；倒车 holder 由 VehicleState 档位触发

MapTiles（apps/map）
  TileServer GL XYZ 优先，失败回退 maps/tiles/{z}/{x}/{y}.png；Open-Meteo 地点搜索
```

## 目录

| 路径 | 作用 |
| --- | --- |
| `src/core/` | `WeatherService`、`MapTiles`、`CameraService`、`RadioSession`、`FileBrowser`、`GpsSource`、`DlnaRenderer`、`FfmpegUrlPlayer`、`UpdateService`、`AppCatalog`、会话类等 |
| `src/carplay/` | CarPlay + `AirPlayMirrorSession`、iAP2、AirPlay、蓝牙/Wi‑Fi、`BluetoothMediaHub`、MFi、H.264 |
| `src/androidauto/` | `AndroidAutoSession`、`AoapTransport`（有线 AOAP） |
| `src/shell/main.cpp` | 入口、QML 注册、日志、Linux systemd 看门狗 |
| `src/shell/qml/` | `Main`、`HomeScreen`、`StatusBar`、`AppStage`、`WeatherFx`、`BootSplash`、`LockScreen`、`VirtualKeyboard`、Ios* |
| `src/shell/*VideoItem.*` | CarPlay / AirPlay / DLNA / Camera 视频表面 |
| `src/shell/shaders/weather.frag` | 天气片元着色器 |
| `apps/*/` | 内置应用 |
| `feed/*/` | 可安装商店包（可空） |
| `version.json` | 当前版本（`2.0.0.20261010`）与 channel |
| `deploy/update-manifest.example.json` | OTA 更新描述示例 |
| `assets/wallpapers/`、`assets/media/` | 壁纸 / 演示媒资 |
| `assets/carplay/offline-mfi/` | MFi（`identity.pk8`、`certificate.p7b`；默认 gitignore） |
| `assets/maps/tiles/` | 本地 XYZ 回退瓦片 |
| `scripts/start-tileserver.ps1` / `stop-tileserver.ps1` | WSL Docker TileServer GL |
| `scripts/mbtiles-to-xyz.py` | MBTiles → 本地 PNG（可选） |
| `scripts/gen-map-tiles.py` | 占位瓦片生成（可选） |
| `scripts/pack-mp157.sh` | 打 tar.gz 并生成更新 manifest |
| `third_party/ffmpeg/win64/` | Windows FFmpeg DLL（存在则 POST_BUILD 拷贝） |
| `CMakePresets.json` | Windows / `mp157` 预设 |
| `build/Debug/ivi-shell.exe` | 本地 Debug 产物 |

---

## 内置应用

| ID | 名称 | 说明 |
| --- | --- | --- |
| `carplay` | CarPlay | 无线 CarPlay：蓝牙 + Wi‑Fi + AirPlay 画面/触控 |
| `androidauto` | Android Auto | 有线 USB AOAP 会话（原型） |
| `radio` | 网络电台 | `RadioSession` 拉流播放 |
| `files` | 文件 | `FileBrowser` 浏览/拷贝/分享 |
| `dashcam` | 行车记录仪 | 摄像头预览/录制；倒车全屏由 Shell 接管 |
| `weather` | 天气 | 实况 / 小时 / 城市搜索（`WeatherService`） |
| `music` | 音乐 | 本地 WAV + 蓝牙音源，`MediaSession` / `BluetoothMediaHub` |
| `video` | 视频 | `VideoScreen` 演示 AVI |
| `map` | 地图 | TileServer/本地瓦片 + 标点 + 联网搜索 |
| `phone` | 电话 | `CallSession` |
| `vehicle` | 车辆 | `VehicleState`（含 R 档倒车联动） |
| `settings` | 设置 | 主题 / 亮度 / 壁纸 / GPS / 天气调试 / **应用更新** |

`manifest.json`：`name`、`entry`、`color`、`dock`、`dockOrder`、`builtin`、`order`。

---

## 无线 CarPlay（原型）

**前提（Windows）**  
exe 旁 `offline-mfi/`；同 Wi‑Fi + 密码；蓝牙已配对选中；FFmpeg DLL（可选放 `third_party/ffmpeg/win64/`）。

**流程**  
选手机 → Wi‑Fi →「开始 CarPlay」→ iAP2 → AirPlay `:7000` → `CarPlayVideoItem` + 触控。

**平台**  
Windows：NCrypt/WLAN/蓝牙主机 API。Linux：OpenSSL MFi 已接；板端 BlueZ 主机栈未接。非 Apple 认证量产。与 Android Auto 互斥。

---

## Android Auto（原型）

| 项 | 说明 |
| --- | --- |
| 单例 | `AndroidAutoSession` |
| 传输 | 有线 USB AOAP（`AoapTransport`，Win setupapi/winusb） |
| 行为 | 启动/停止、状态与流量计数；当前无视频帧 |
| 互斥 | 与 `CarPlaySession` 互斥 |

非 Google 认证交付物。

---

## 网络电台 / 文件

| 应用 | 单例 | 行为 |
| --- | --- | --- |
| `radio` | `RadioSession` | 预设电台 URL → FFmpeg 拉流；与本地音乐互斥 |
| `files` | `FileBrowser` | 多根目录浏览/拷贝/删除；可打开音视频；HTTP 局域网分享 |

---

## 行车记录仪 / 倒车

`CameraService`：设备列表、预览、录 AVI、演示模式。`apps/dashcam` 调用 `acquire/release`。`VehicleState` 档位为 R 时 Shell 全屏倒车画面（`reverseActive`）。

---

## 天气

**实况（Open-Meteo）**  
`weather_code`、温度、能见度、风/阵风、降水、地温等；warnings → `travelAlert`。定位：IP 或 `GpsSource`。

**出行态**（高于纯 WMO）

| kind | 条件概要 |
| --- | --- |
| `typhoon` | 台风关键词，或阵风 ≥75 且降水类 |
| `blizzard` | 降雪 + 阵风 ≥40，或低温低能见度雪 |
| `ponding` | 暴雨/雷雨或强降水低能见度 |
| `frost` | 气温 ≤2℃ 且地温 ≤1℃ |
| `wetRoad` | 近 2h 有雨、当前几乎无降水 |
| `wind` | 阵风 ≥50 或风速 ≥42 |
| `sandLift` | WMO 36；`dust` 为 30–35 |

**壁纸 wx**  
1–2 晴，3 多云，4 阴，5 雾，6–9 雨/雷，10–12 雪，13 冰雹，14 霾，15 沙尘，16 雨夹雪，17 冻雨，18 大风，19 暴雪，20 霜，21 扬沙，22 积水，23 湿滑，24 台风。

设置 → 天气调试：点种类后回桌面看壁纸；「实况」恢复拉取。

```bat
D:\Qt\6.8.3\msvc2022_64\bin\qsb.exe --glsl 100es,120,330,430,440 --hlsl 50 --msl 12 ^
  -o src\shell\weather.frag.qsb src\shell\shaders\weather.frag
cmake --build build --config Debug --target ivi-shell
```

---

## 地图与 TileServer GL

`MapTiles`：优先 TileServer GL，不可达回退 `maps/tiles/{z}/{x}/{y}.png`。搜索走 Open-Meteo；`NavSession` 仍为假引导文案。

**依赖**  
WSL2 + Docker；镜像 `maptiler/tileserver-gl:latest`；仓库根 `.mbtiles`（默认 `osm-2020-02-10-v3.11_china_hangzhou.mbtiles`）。更大区域用 [MapTiler Data](https://data.maptiler.com/downloads/) Custom；国内商用需合规底图。

```powershell
.\scripts\start-tileserver.ps1
.\scripts\stop-tileserver.ps1
```

| 项 | 值 |
| --- | --- |
| 预览 | http://127.0.0.1:1999 |
| XYZ | `http://127.0.0.1:1999/styles/basic-preview/{z}/{x}/{y}.png` |
| 容器 | `ivi-tileserver` |

```bat
set IVI_TILE_URL=http://127.0.0.1:1999/styles/basic-preview/{z}/{x}/{y}.png
```

本地回退：

```bat
python scripts\mbtiles-to-xyz.py osm-2020-02-10-v3.11_china_hangzhou.mbtiles -o assets\maps\tiles --clean --min-zoom 12 --max-zoom 14
cmake --build build --config Debug --target ivi-shell
```

板端不宜常驻 TileServer GL，宜预导出 XYZ 或授权 SDK。

---

## 应用更新（OTA）

版本文件：`version.json`（`SystemState.appVersion` / `UpdateService.currentVersion`）。

**流程**  
设置 → 填 manifest URL（或环境变量 `IVI_UPDATE_MANIFEST`）→ 检查更新 → 下载 → SHA256 → `tar` 解压合并 → 退出并由 apply 脚本重启（Linux 可 `systemctl restart ivi-shell`）。

**打包侧**

```bash
# 产出 dist/mp157-ivi.tar.gz 与同名 .json 描述
IVI_UPDATE_BASE_URL=https://example.com/ivi ./scripts/pack-mp157.sh
```

描述字段见 `deploy/update-manifest.example.json`：`version`、`url`、`sha256`、`size`、`notes`。应用包级升级，非整盘 A/B。

---

## 界面

| 区域 | 说明 |
| --- | --- |
| BootSplash | 开机启动画面 |
| LockScreen | 空闲锁屏 |
| StatusBar | 时钟、城市、天气、温度、预警、链路指示 |
| HomeScreen | 分页 + Dock 拖拽 |
| AppStage | 全屏应用；底条回桌面 |
| WeatherFx | 桌面天气特效 |
| VirtualKeyboard | 拼音软键盘 |
| GlassPanel | 玻璃面板组件 |
| 倒车层 | `CameraService.reverseActive` 时盖住桌面 |

分辨率默认 **1024×600**；Linux 可全屏。

---

## 构建与运行

**依赖**  
Qt **6.8.3**（MSVC 2022 x64）：Quick、QuickControls2、Network、Gui、Concurrent。Windows：wlanapi / 蓝牙 / MF / setupapi+winusb（AA）；Linux：OpenSSL（carplay）、可选 ALSA。FFmpeg win64 可选。

```bat
cmake --preset windows
cmake --build --preset windows --target ivi-shell
cd build\Debug
ivi-shell.exe
```

同级需有 `apps/`、`feed/`、`wallpapers/`、`media/`、`offline-mfi/`、`version.json`、可选 `maps/`。

板端：[docs/STM32MP157-烧写与部署.md](docs/STM32MP157-烧写与部署.md)。

---

## 配置与持久化

| 项 | 存储 |
| --- | --- |
| 壁纸 | QSettings `wallpaper` |
| 主题 | `autoTheme` / `manualDark` |
| Dock 顺序 | `AppCatalog` |
| CarPlay 蓝牙/Wi‑Fi/手机 IP | `carplay/*` |
| GPS | `gps/*` |
| 锁屏超时 | `lockTimeout` |
| 更新 manifest URL | `update/manifestUrl` 或 `IVI_UPDATE_MANIFEST` |
| 天气调试 preview | 不持久化 |

---

## 现状说明

可演示 IVI 原型：Shell（开机/锁屏/软键盘）、多应用、壁纸天气、地图瓦片、CarPlay/Android Auto、网络电台/文件、记录仪/倒车、**应用包 OTA**。非量产；未完成 MP157 eglfs 量产验证与真实总线；CarPlay/AirPlay **非** Apple 认证；Android Auto **非** Google 认证。

---

## 未来计划（量产车机）

### 阶段 A — 工程内可实施（优先）

| 项 | 现状 | 计划 |
| --- | --- | --- |
| MP157 部署 | toolchain + `scripts/*` / board-probe | 真机 eglfs 联调、Yocto recipe |
| 车身数据 | `VehicleState` 模拟 | SocketCAN / DBC |
| 定位与天气 | `GpsSource` demo/NMEA + Open-Meteo | 板端 GNSS；可选国内预警 API |
| 板端蓝牙 / CarPlay | Windows 原型；Linux OpenSSL MFi | 板端 BlueZ；对接通话/媒体会话 |
| Android Auto | Win AOAP 探测 | 完整会话/视频；板端 USB Host |
| 导航 | 瓦片底图 + 搜索已有；`NavSession` 假步骤 | 地图 SDK / 真实路径规划 |
| 音频 | `AudioFocus` + 各源播放 | ducking、PipeWire/Pulse 策略 |
| 镜像与交付 | 无 Yocto 层 | OpenSTLinux/Yocto recipe |
| 稳定性 | Linux systemd 看门狗 | 崩溃重启、日志与压测 |
| OTA | 应用包 manifest + SHA256 + 覆盖合并 | 安全启动签名链、A/B 分区 |

**建议顺序**：MP157 稳定 → CAN → 板端蓝牙/CarPlay → AA → 导航 SDK → 看门狗 / 量产 OTA。

### 阶段 B — 周期长

| 项 | 说明 |
| --- | --- |
| CarPlay 认证 | 正式 MFi / USB Host |
| Android Auto 认证 | Google 认证与完整协议 |
| 语音 | TTS/ASR SDK |
| 多屏 / 副驾 | DRM 多路输出 |

### 阶段 C — OEM / 实验室

| 项 | 说明 |
| --- | --- |
| ISO 26262 | 安全相关显示证据 |
| R155/R156、国标 | 安全启动、备案 |
| 无线电 / 蓝牙认证 | SRRC、CCC 等 |
| 地图与数据合规 | 审图号、隐私 |

### 当前模拟 / 占位

| 模块 | 文件 / 应用 | 当前行为 |
| --- | --- | --- |
| 导航引导 | `NavSession` | 假步骤文案 |
| 车身 | `VehicleState`、`apps/vehicle` | 本地模拟 |
| 电话 | `CallSession`、`apps/phone` | UI 模拟 |
| 视频 | `VideoScreen` | 自绘 AVI |
| 摄像头无设备 | `CameraService` | `demoMode` 演示帧 |
| Android Auto | `AndroidAutoSession` | AOAP 链路探测，无视频 |
| GPS | `GpsSource` | 默认 demo 位 |

---

## 与 CarPlay / MP157 的关系

Shell/HMI 可作为板端界面基础；`src/carplay/` 含无线 CarPlay 与 AirPlay 栈；`src/androidauto/` 含有线 AOAP 原型。量产认证、USB Host、板端 BlueZ 与 rootfs 见阶段 A/B/C 与 [docs/STM32MP157-烧写与部署.md](docs/STM32MP157-烧写与部署.md)。
