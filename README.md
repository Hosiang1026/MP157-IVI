# MP157-IVI

STM32MP157 车机 UI 原型（Qt 6.8，1024×600）。桌面 Shell + 可安装 QML 应用 + 壁纸天气 Shader + Open-Meteo + **无线 CarPlay** / **Android Auto（有线 AOAP）** / **网络电台·播客·听书** / **流媒体音乐** / **文件管理** / **行车记录仪** / **地图瓦片**（Windows 演示）。目标部署为板端 Linux 全屏运行 `ivi-shell`。

业务源码：`src/core/` + `src/carplay/` + `src/androidauto/` + `src/shell/` + `apps/` + `feed/`。

当前版本：**v2.0.0.20261010**

![v1.0.0](..\MP157-IVI\screen\v1.0.0.gif)



![v2.0.0](..\MP157-IVI\screen\v2.0.0.gif)

## 更新日志

### v2.0.0.20261010

**相对 v1.0.0 的主要变化**
- 新增内置应用：`androidauto`（有线 USB AOAP）、`radio`（网络电台/播客/听书）、`files`（文件浏览/拷贝/局域网分享）
- 移除内置应用入口：`airplay`、`dlna`、`store`（后端 `AirPlayMirror` / `DlnaRenderer` / `feed` 安装通路仍保留）
- 新增服务：`AndroidAutoSession`、`RadioSession`、`PodcastSession`、`StreamSession`、`NotificationSession`、`FileBrowser`、`GpsSource`、`BluetoothMediaHub`
- 蓝牙手机通知：`BluetoothAncsClient`（ANCS）+ `BluetoothMapClient`（MAP）→ `NotificationSession` → StatusBar
- Shell：开机 `BootSplash`、空闲 `LockScreen`、拼音 `VirtualKeyboard`、仪表导航条 `ClusterNav`、倒车雷达条；方向盘/媒体键路由电台/播客/流媒体/音乐
- CarPlay 与 Android Auto 互斥；天气/CarPlay 可接 `GpsSource`；音乐支持蓝牙音源 + Subsonic/Jellyfin
- Linux：`ivi-shell.log` 轮转 + systemd READY/WATCHDOG

**工程与入口**
- Qt 6.8 / C++17：`src/core`、`src/carplay`、`src/androidauto`、`src/shell`、`apps`、`feed`、`assets`、`scripts`、`docs`
- `main.cpp` 注册单例：`SystemState`、`UpdateService`、`AudioFocus`、`Weather`、`VehicleState`、`CameraService`、`MediaSession`、`BluetoothMediaHub`、`NavSession`、`MapTiles`、`CallSession`、`NotificationSession`、`RadioSession`、`PodcastSession`、`StreamSession`、`FileBrowser`、`GpsSource`、`AppCatalog`、`WallpaperStore`、`CarPlaySession`、`AndroidAutoSession`、`AirPlayMirror`、`DlnaRenderer`
- 注册类型：`VideoScreen`、`CarPlayVideoItem`、`AirPlayVideoItem`、`DlnaVideoItem`、`CameraVideoItem`
- ImageProvider：`image://carplay`、`image://dlna`
- Linux 预置 `QT_QPA_EGLFS_PHYSICAL_*`、`QT_QPA_FB_FORCE_FULLSCREEN`；Win 字体 YaHei UI，Linux 优先 Noto Sans CJK；`QQuickStyle` Basic；组织名 MP157 / 应用名 ivi-shell

**Shell UI**
- `Main.qml`：壁纸 + `WeatherFx` + `StatusBar` + 多任务抽屉 + `HomeScreen` / `AppStage`；亮度遮罩；Linux 全屏
- `BootSplash` / `LockScreen`（空闲锁屏，媒体/导航/投屏/通话/倒车时阻止）；`VirtualKeyboard` + `pinyin.js`
- `ClusterNav`：导航激活时仪表条（转向/距离/限速/文案）；倒车层叠加 `parkRl/Rcl/Rcr/Rr` 雷达条
- CarPlay 有视频 / 倒车影像时隐藏 StatusBar、亮度层、底条
- 底条 Home：上滑或点击关闭应用；CarPlay `hostUiRequested` 自动关应用层
- `HomeScreen`：第 0 页壁纸+Dock，后续页 5×3 网格；页码点；横向分页
- Dock/桌面长按拖拽：加移出 Dock（最多 5）、调序、边缘翻页
- `AppStage`：多应用 Loader 缓存、`running`/`background`；关 music 暂停、关 map 停导航；加载失败提示
- `StatusBar`：时钟、城市、天气温度、`travelAlert`；导航/音乐/电台/播客/流媒体 Hub；iPhone 通知条；CarPlay / Android Auto / AirPlay 链路指示；蓝牙/Wi‑Fi/电量/信号；多任务入口
- `WeatherFx`：`weather.frag.qsb`，kind→wx 1–24，随 `pagePos` 淡出，支持 `Weather.preview`
- `GlassPanel`：壁纸采样模糊圆角（Dock / 任务栏）
- `AppIcon` / `AppGlyph`：manifest 图标或 Canvas 矢量（含 carplay/androidauto/radio/files/weather 等）
- Ios*：`IosPressable` / `IosToggle` / `IosSpinner` / `IosSearchField` / `IosSegmented` / `IosGroupedCard` / `IosEmptyState` / `IosIcon`

**内置应用**
- `carplay`：Wi‑Fi/蓝牙配对选择、开始/断开、全屏画面+触控、MFi 前置校验、`carplay.log`；与 AA 互斥
- `androidauto`：有线 USB AOAP + **演示投影**（`startDemo` / `IVI_AA_DEMO`），与 CarPlay 互斥
- `radio`：分段「电台 / 播客 / 听书」；电台收藏过滤；`RadioSession` + `PodcastSession`
- `files`：多根目录浏览/进入/删除/拷贝；可播放项交给音乐/视频；局域网 HTTP 分享
- `dashcam`：设备选择、预览、录 AVI、录像列表；`acquire/release`
- `weather`：多城市、搜索、小时/日预报、UV/风/能见度/体感/湿度，按 kind 渐变 UI
- `map`：瓦片拖拽缩放、车标/目的地、Open-Meteo 搜索、导航开始/结束（步骤演示）
- `music`：本地 WAV+LRC / 蓝牙音源 / **流媒体**（Subsonic·Navidrome / Jellyfin，`StreamSession`）
- `video`：`VideoScreen` 演示 AVI 库与播放；播时暂停音乐
- `phone`：拨号盘、联系人/最近、模拟通话；可选绑定蓝牙手机（`BluetoothMediaHub`）
- `vehicle`：速度/档位 PRND、锁车/灯、胎压告警、油量续航外温；R 档倒车+雷达；导航激活时仪表指引
- `settings`：蓝牙/Wi‑Fi、主题、亮度音量、壁纸、GPS、**状态栏通知**、版本号、**应用更新**；连点 7 次开发者模式→天气调试

**核心服务**
- `AppCatalog`/`AppListModel`：扫描 apps+feed；install 从 feed 复制；dock/homeOrder 持久化
- `SystemState`：亮度/音量/蓝牙/Wi‑Fi/主题色板/开发者模式/锁屏超时/时钟/`pref`；`appVersion` 读 `version.json`；Win WlanAPI+电量，Linux 背光 sysfs
- `UpdateService`：拉更新 manifest → 下载 tar.gz → SHA256 校验 → 解压合并 → 退出并由脚本重启；地址 `update/manifestUrl` 或 `IVI_UPDATE_MANIFEST`
- `WeatherService`：IP/GPS 定位→Open-Meteo forecast/warnings/geocoding；WMO→kind；出行态；多城；`weather-cache.json`；preview 不持久化
- `GpsSource`：demo 位或串口 NMEA；供天气/CarPlay；设置页开关
- `MapTiles`：TileServer GL（`IVI_TILE_URL`，默认 :1999）探测，回退 `maps/tiles`；pan/zoom/搜索/目的地
- `NavSession`：演示引导 / CarPlay `applyRemote`（文案/转向/`distanceM`/限速）；驱动 `ClusterNav` 与车辆仪表
- `MediaSession`：扫描 WAV；Win waveOut / Linux ALSA（可选）；队列与 playMode 持久化；可接蓝牙音源名
- `BluetoothMediaHub`：设备列表/选中/配对；`phoneConnected`；驱动 `MediaSession` 蓝牙源
- `BluetoothAncsClient` / `BluetoothMapClient`：连手机后推送通知/短信到 `NotificationSession`
- `NotificationSession`：状态栏通知条；`statusBarEnabled` 持久化；可 `dismiss` / `applyRemote`
- `RadioSession`：电台列表 + 收藏/`filterMode` + 重连；FFmpeg 拉流；AudioFocus；与本地音乐互斥
- `PodcastSession`：RSS 播客/听书（`library`）；FFmpeg 播单集；与电台/本地音乐互斥
- `StreamSession`：Subsonic / Jellyfin 登录·专辑·搜索·拉流；与电台/本地音乐互斥
- `FileBrowser`：roots/浏览/拷贝/删除/打开；HTTP 分享；可请求打开 music/video
- `CallSession`：模拟通话；`phone/recents`；AudioFocus 优先级 10
- `VehicleState`：行车模拟；倒车雷达 `parkRl/Rcl/Rcr/Rr`/`parkAlert`；`vehicle/*` 持久化；外温同步天气
- `CameraService`：holder（dashcam/reverse）；Win MF / Linux V4L2；无设备 demoMode；MJPEG→AVI
- `AndroidAutoSession`：Win AOAP（setupapi/winusb）；会话探测；`startDemo` 假全屏；无 TLS/真实视频帧
- `DlnaRenderer` / `AirPlayMirrorSession`：后端仍注册（无独立应用入口）
- `FfmpegUrlPlayer`：FFmpeg 拉 URL 解码（电台 / 播客 / 流媒体 / DLNA 等）
- `AudioFocus`：多客户端 request/release、ducking
- `WallpaperStore`：builtin catalog + user；当前壁纸持久化；上传仅 Windows

**CarPlay / Android Auto**
- `CarPlaySession`：`LocalMfiAuth`（offline-mfi）、`AirPlayIdentity`、`AirPlayServer`、Bonjour、iAP2；可挂 `GpsSource` / `MediaSession` / `NavSession` / `CallSession` / `NotificationSession`
- Wi‑Fi：`ExistingWifi`（Win WLAN）；蓝牙：`BluetoothDevices` / `BluetoothRfcomm` / `BluetoothMediaHub` / ANCS / MAP
- AirPlay 栈：Crypto/AuthSetup/ControlCipher/Bplist/ScreenStream/AudioStream/BufferedAudio/PcmPlayer/H264Decoder；Monocypher；导航 info → `NavSession`
- `AirPlayMirrorSession`：独立 Bonjour（MP157-AirPlay），与 CarPlay 互斥端口/mDNS
- `AndroidAutoSession`：有线 AOAP；演示投影；与 CarPlay 互斥
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
- `radio/favorites`；`podcast/feeds`/`podcast/bookFeeds`；`stream/*`（后端/地址/账号）；`notification/statusBarEnabled`
- `update/manifestUrl`；`weather-cache.json`（exe 旁）；`Weather.preview` 不落盘

**平台差异与限制**
- Windows：WLAN/蓝牙/MF/摄像头/waveOut/壁纸上传/FFmpeg DLL/TileServer（WSL）/AOAP/ANCS·MAP 为完整演示路径；OTA 依赖本机 `tar`
- Linux：全屏 eglfs/linuxfb、背光 sysfs、可选 ALSA、OpenSSL MFi、systemd 看门狗；无 Win 级蓝牙/WLAN/AOAP UI；宜预置 XYZ 瓦片；OTA 可用 systemd 重启 `ivi-shell`
- 导航/电话/车身/VideoScreen/Android Auto/倒车雷达为演示或链路探测；CarPlay/AirPlay 非 Apple 认证；AA 非 Google 认证；MP157 eglfs 量产未验；地图商用需合规；OTA 为应用包级

### v1.0.0.20261009

初版：Shell + 多应用 + 天气 Shader + 地图瓦片 + 无线 CarPlay / AirPlay / DLNA / 行车记录仪 / 应用包 OTA。详见该版本源码树。

## 调用链

```
main.cpp
  注册单例：SystemState / UpdateService / Weather / AppCatalog / MediaSession / BluetoothMediaHub
            / NavSession / MapTiles / CallSession / NotificationSession
            / RadioSession / PodcastSession / StreamSession / FileBrowser / GpsSource
            / VehicleState / CameraService / CarPlaySession / AndroidAutoSession
            / AirPlayMirror / DlnaRenderer / …
  后台：BluetoothAncsClient / BluetoothMapClient → NotificationSession
  注册类型：VideoScreen / CarPlayVideoItem / AirPlayVideoItem / DlnaVideoItem / CameraVideoItem
  image://carplay、image://dlna
  → QQmlApplicationEngine 加载 IviShell/Main.qml

Main.qml
  BootSplash → WallpaperStore 壁纸 Image
   → WeatherFx（ShaderEffect + weather.frag.qsb）
   → StatusBar / HomeScreen / AppStage / LockScreen / VirtualKeyboard / ClusterNav
   → 倒车：CameraService.reverseActive 时全屏 CameraVideoItem + 雷达条

WeatherService
  IP 或 GpsSource → Open-Meteo → 出行态 kind → WeatherFx / StatusBar / apps/weather

AppCatalog
  扫描 apps/ 与 feed/ → Dock 持久化；POST_BUILD 拷 apps、feed、assets、maps、offline-mfi、version.json

UpdateService（apps/settings）
  version.json → 检查 manifest → 下载包 → sha256 → tar 解压合并 → 脚本重启

CarPlaySession（apps/carplay）
  offline-mfi → Wi‑Fi + 蓝牙 → iAP2 → AirPlayServer → 画面/触控/导航 info；与 AndroidAutoSession 互斥
  日志：carplay.log

AndroidAutoSession（apps/androidauto）
  USB AOAP → 会话探测；或 startDemo / IVI_AA_DEMO 假全屏；与 CarPlay 互斥

RadioSession / PodcastSession（apps/radio）
  电台 URL 或 RSS → FfmpegUrlPlayer → AudioFocus；与 MediaSession / StreamSession 互斥

StreamSession（apps/music 流媒体页）
  Subsonic / Jellyfin → 专辑/搜索 → FfmpegUrlPlayer → AudioFocus

NotificationSession（StatusBar + settings）
  ANCS / MAP / CarPlay → 状态栏通知条

FileBrowser（apps/files）
  roots 浏览 / 拷贝 / 分享 HTTP；可打开 music/video

CameraService（apps/dashcam + 倒车）
  采集/演示画面 → 可选录 AVI；倒车 holder 由 VehicleState 档位触发

MapTiles / NavSession（apps/map + ClusterNav + vehicle）
  TileServer GL XYZ 优先，失败回退 maps/tiles；导航步骤或 CarPlay applyRemote
```

## 目录

| 路径 | 作用 |
| --- | --- |
| `src/core/` | `WeatherService`、`MapTiles`、`CameraService`、`RadioSession`、`PodcastSession`、`StreamSession`、`NotificationSession`、`FileBrowser`、`GpsSource`、`DlnaRenderer`、`FfmpegUrlPlayer`、`UpdateService`、`AppCatalog`、会话类等 |
| `src/carplay/` | CarPlay + `AirPlayMirrorSession`、iAP2、AirPlay、蓝牙/Wi‑Fi、`BluetoothMediaHub`、ANCS/MAP、MFi、H.264 |
| `src/androidauto/` | `AndroidAutoSession`、`AoapTransport`（有线 AOAP + 演示投影） |
| `src/shell/main.cpp` | 入口、QML 注册、日志、Linux systemd 看门狗 |
| `src/shell/qml/` | `Main`、`HomeScreen`、`StatusBar`、`AppStage`、`WeatherFx`、`BootSplash`、`LockScreen`、`VirtualKeyboard`、`ClusterNav`、Ios* |
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
| `androidauto` | Android Auto | 有线 USB AOAP + 演示投影（原型） |
| `radio` | 电台 | 网络电台 / 播客 / 听书（`RadioSession` / `PodcastSession`） |
| `files` | 文件 | `FileBrowser` 浏览/拷贝/分享 |
| `dashcam` | 行车记录仪 | 摄像头预览/录制；倒车全屏由 Shell 接管 |
| `weather` | 天气 | 实况 / 小时 / 城市搜索（`WeatherService`） |
| `music` | 音乐 | 本地 WAV + 蓝牙 + Subsonic/Jellyfin（`MediaSession` / `StreamSession`） |
| `video` | 视频 | `VideoScreen` 演示 AVI |
| `map` | 地图 | TileServer/本地瓦片 + 标点 + 联网搜索 |
| `phone` | 电话 | `CallSession` + 蓝牙手机绑定 |
| `vehicle` | 车辆 | `VehicleState`（R 档倒车/雷达 + 导航仪表指引） |
| `settings` | 设置 | 主题 / 亮度 / 壁纸 / GPS / 通知 / 天气调试 / **应用更新** |

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
| 行为 | 启动/停止、状态与流量计数；无 TLS/真实视频帧 |
| 演示 | `startDemo()` 或环境变量 `IVI_AA_DEMO` → 假全屏投影 |
| 互斥 | 与 `CarPlaySession` 互斥 |

非 Google 认证交付物。

---

## 电台 / 播客 / 流媒体 / 文件

| 应用 | 单例 | 行为 |
| --- | --- | --- |
| `radio` 电台页 | `RadioSession` | 预设 URL → FFmpeg；收藏/`filterMode`；重连；与本地音乐互斥 |
| `radio` 播客/听书 | `PodcastSession` | RSS feed → 单集列表 → FFmpeg；`library=podcast\|book` |
| `music` 流媒体 | `StreamSession` | Subsonic·Navidrome / Jellyfin 登录·专辑·搜索·拉流 |
| `files` | `FileBrowser` | 多根目录浏览/拷贝/删除；可打开音视频；HTTP 局域网分享 |

---

## 通知（ANCS / MAP）

| 项 | 说明 |
| --- | --- |
| 单例 | `NotificationSession` |
| 来源 | `BluetoothAncsClient`（iPhone ANCS）、`BluetoothMapClient`（MAP 短信）、CarPlay |
| UI | StatusBar 通知条；设置「状态栏显示通知」；依赖 `BluetoothMediaHub.phoneConnected` |

---

## 行车记录仪 / 倒车 / 仪表导航

`CameraService`：设备列表、预览、录 AVI、演示模式。`apps/dashcam` 调用 `acquire/release`。`VehicleState` 档位为 R 时 Shell 全屏倒车画面（`reverseActive`）+ 正弦演示雷达条（`parkRl/Rcl/Rcr/Rr`）。`NavSession.active` 时 `ClusterNav` 与车辆页显示转向/距离/限速指引。

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
| StatusBar | 时钟、城市、天气、温度、预警、媒体 Hub、通知条、链路指示 |
| ClusterNav | 导航激活时仪表条 |
| HomeScreen | 分页 + Dock 拖拽 |
| AppStage | 全屏应用；底条回桌面 |
| WeatherFx | 桌面天气特效 |
| VirtualKeyboard | 拼音软键盘 |
| GlassPanel | 玻璃面板组件 |
| 倒车层 | `CameraService.reverseActive` 时盖住桌面 + 雷达条 |

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
| 电台收藏 | `radio/favorites` |
| 播客/听书 feed | `podcast/feeds`、`podcast/bookFeeds` |
| 流媒体账号 | `stream/*` |
| 状态栏通知 | `notification/statusBarEnabled` |
| 锁屏超时 | `lockTimeout` |
| 更新 manifest URL | `update/manifestUrl` 或 `IVI_UPDATE_MANIFEST` |
| 天气调试 preview | 不持久化 |

---

## 现状说明

可演示 IVI 原型：Shell（开机/锁屏/软键盘/仪表导航/倒车雷达）、多应用、壁纸天气、地图瓦片、CarPlay/Android Auto、电台·播客·听书/流媒体/文件、手机通知、记录仪/倒车、**应用包 OTA**。非量产；未完成 MP157 eglfs 量产验证与真实总线；CarPlay/AirPlay **非** Apple 认证；Android Auto **非** Google 认证。

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

### 真车控相关概念（CAN / SOME/IP / ISO 26262）

当前 `VehicleState` / `apps/vehicle` 仅为**本地模拟**（速度、档位、门锁、胎压等），不连接实车总线。要做屏上真车控（读车速、控空调/座椅、联动倒车灯等），需理解并对接下列三块；它们不是 UI 文档，而是整车电子与功能安全基础。

#### CAN 网关

- 车内各模块（发动机/电驱、车门、空调、仪表等）通过 **CAN 总线**交换信号（报文 ID + 数据字节）。
- **网关（Gateway）** 相当于车内路由器：不同域（动力、车身、娱乐）之间的报文经网关转发与过滤，车机（IVI）一般**不能**直接向安全相关节点乱发指令。
- 量产对接常见路径：IVI ↔（以太网/串口/CAN）↔ **网关** ↔ 各 ECU；信号定义见 OEM 提供的 **DBC**（或 ARXML），板端可用 **SocketCAN** 收发。
- 本仓库现状：无 SocketCAN / DBC / 网关协议；车辆页按钮只改本地状态机。上板真车控时，应用层仍走 `VehicleState` 一类接口，底层替换为总线读写。

#### SOME/IP

- **SOME/IP**（Scalable service-Oriented MiddlewarE over IP）是较新的车载通信方式，多跑在**车载以太网**上，以「服务 / 方法 / 事件」暴露能力（例如读车速、设空调温度），而不是只按 CAN 报文 ID 拼字节。
- 相对传统纯 CAN，更偏接口化，便于 IVI、ADAS、域控制器之间扩展；在不少高端车型与 AUTOSAR 自适应平台中常见。
- 与 CAN 的关系：实车往往 **CAN + 以太网/SOME/IP 并存**；IVI 可能只连网关的以太网侧，由网关再桥到 CAN 域。具体以整车架构为准。
- 本仓库现状：未实现 SOME/IP 客户端；量产需按 OEM 服务清单做发现、订阅与调用（并处理权限与诊断）。

#### ISO 26262

- **ISO 26262** 是道路车辆**功能安全**标准，按危害与风险定 **ASIL** 等级（A～D，另有 QM），要求从需求、设计、实现到验证有可追溯证据。
- **刹车、转向、气囊**等安全相关功能 ASIL 高，禁止娱乐主机随意下发控制；IVI 侧错误不得导致不可接受的整车风险。
- 屏上控**空调、座椅、氛围灯、车窗**等通常 ASIL 要求较低（多为 QM 或低 ASIL），但仍需按 OEM 安全概念划分：哪些信号只读、哪些可写、失败时默认态、与网关鉴权如何配合。
- 阶段 C 中的「ISO 26262」指量产时要补齐安全相关显示与控制的证据链，而非本原型已具备认证。
- 本仓库现状：无功能安全流程与安全机制；演示页的车控**不得**当作实车安全控制使用。

#### 与本项目的对应关系

| 概念 | 本项目现状 | 量产方向 |
| --- | --- | --- |
| CAN / 网关 | `VehicleState` 模拟；无总线 | SocketCAN + DBC；经网关收发；方向盘键 / 倒车灯 / 雷达实信号 |
| SOME/IP | 无 | 按 OEM 服务接口订阅车况、下发车身舒适域指令 |
| ISO 26262 | 无 | 安全分析与 ASIL 分配；IVI 仅控允许的信号；高安全域不经娱乐屏直控 |

量产协议优先级、原理、接车打通与 **2018 款柯迪亚克（MQB）硬件型号** 见：[docs/量产车机协议-柯迪亚克2018.md](docs/量产车机协议-柯迪亚克2018.md)。

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

### 当前模拟 / 占位（上板后需接真能力）

桌面可点的「演示 / 模拟」只验证 UI 与状态机，**不等于**量产功能。上板后按下列项替换为真实实现：

| 模块 | 现状（假 / 演示） | 上板后 |
| --- | --- | --- |
| 导航 | `NavSession` 定时假步骤；`ClusterNav` / 车辆页仪表指引跟假数据；无路网规划 | 地图 SDK / 真实路径规划 + 引导距离/转向 |
| 电话 | `CallSession` + `apps/phone` UI 模拟拨打/接听 | HFP / ofono / 蓝牙通话实链 |
| 收音 | `RadioSession` 网络电台拉流，非 AM/FM | 硬件 Tuner 或保留网络电台并接天线方案 |
| 车身联动 | `VehicleState` 本地模拟速度/档位/门锁/胎压等；R 档触发倒车 | SocketCAN / DBC；方向盘键、倒车灯/雷达实信号 |
| 倒车雷达叠加 | `parkRl/Rcl/Rcr/Rr` 正弦演示条 + 轨迹线 UI | 超声波雷达 / APA 数据驱动条形与告警 |
| Android Auto | AOAP 链路探测；**演示投影**（`startDemo` / `IVI_AA_DEMO`）假全屏，无 TLS/视频帧 | USB Host + TLS/视频/音频完整会话（非 Google 认证前仅工程验证） |
| CarPlay / 板端无线 | Win WLAN/蓝牙主机可用；Linux MFi(OpenSSL) 已接，**BlueZ/WLAN 主机栈未接** | 板端 BlueZ + SoftAP/WLAN；通话/媒体/导航会话对接 |
| GPS | `GpsSource` 默认 demo 位；可选串口 NMEA | 板端 GNSS 模组 |
| 摄像头 | 无设备时 `CameraService.demoMode` 演示帧 | V4L2 实摄像头（倒车 / 记录仪） |
| 本地媒体 | `media/` 演示 WAV/AVI；非完整 USB/SD 库 | 扫描外置存储；编解码与音频路由 |
| 语音 | 无 | TTS/ASR（阶段 B） |
| CarLife | 无 | 按需再接 |

入口提示：`apps/androidauto`「演示投影」、车辆页切 **R**（倒车+雷达条）、地图「开始导航」（假引导 + 仪表条）。

---

## 与 CarPlay / MP157 的关系

Shell/HMI 可作为板端界面基础；`src/carplay/` 含无线 CarPlay 与 AirPlay 栈；`src/androidauto/` 含有线 AOAP 原型。量产认证、USB Host、板端 BlueZ 与 rootfs 见阶段 A/B/C 与 [docs/STM32MP157-烧写与部署.md](docs/STM32MP157-烧写与部署.md)。
