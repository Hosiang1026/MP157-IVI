import QtQuick
import QtQuick.Window
import Ivi.Services 1.0

Window {
    id: window
    width: 1024
    height: 600
    visible: true
    title: "IVI"
    color: "#000000"

    property bool lockActive: false
    readonly property bool projectionFullscreen: (stage.currentId === "carplay" && CarPlaySession.hasVideo)
        || (stage.currentId === "androidauto" && AndroidAutoSession.hasVideo)
    readonly property bool lockBlocked: MediaSession.playing
        || RadioSession.playing
        || PodcastSession.playing
        || StreamSession.playing
        || DlnaRenderer.playing
        || NavSession.active
        || CarPlaySession.running
        || AndroidAutoSession.running
        || AirPlayMirror.running
        || CallSession.active
        || CallSession.ringing
        || CameraService.reverseActive
        || AudioFocus.owner === "video"
        || bootSplash.visible
        || SystemState.lockTimeout <= 0

    function bumpIdle() {
        if (lockActive || lockBlocked)
            return
        idleTimer.restart()
    }

    function showLock() {
        if (lockBlocked)
            return
        lockActive = true
        idleTimer.stop()
    }

    function dismissLock() {
        lockActive = false
        if (!lockBlocked && SystemState.lockTimeout > 0)
            idleTimer.restart()
    }

    onLockBlockedChanged: {
        if (lockBlocked) {
            if (lockActive)
                lockActive = false
            idleTimer.stop()
        } else if (!lockActive && SystemState.lockTimeout > 0) {
            idleTimer.restart()
        }
    }

    Timer {
        id: idleTimer
        interval: Math.max(1, SystemState.lockTimeout) * 60 * 1000
        running: !lockActive && !lockBlocked && SystemState.lockTimeout > 0
        repeat: false
        onTriggered: window.showLock()
    }

    Connections {
        target: SystemState
        function onLockTimeoutChanged() {
            if (lockActive || lockBlocked || SystemState.lockTimeout <= 0) {
                idleTimer.stop()
                return
            }
            idleTimer.restart()
        }
    }

    Connections {
        target: stage
        function onOpenedChanged() { window.bumpIdle() }
        function onCurrentIdChanged() { window.bumpIdle() }
    }

    Item {
        id: desktopBg
        anchors.fill: parent

        Image {
            id: wall
            anchors.fill: parent
            source: WallpaperStore.current
            fillMode: Image.PreserveAspectCrop
            cache: true
            asynchronous: true
            mipmap: true
            onStatusChanged: {
                if (status === Image.Ready) {
                    if (home)
                        home.refreshGlass()
                    if (stage && stage.opened)
                        stage.refreshGlass()
                }
            }
        }

        WeatherFx {
            anchors.fill: parent
            wallpaper: wall
            pagePos: home.pagePos
            active: !stage.opened || Weather.preview.length > 0
        }
    }

    Component.onCompleted: {
        if (Qt.platform.os === "linux")
            visibility = Window.FullScreen
        CarPlaySession.nightMode = SystemState.dark
        keyScope.forceActiveFocus()
    }

    Connections {
        target: SystemState
        function onDarkChanged() {
            CarPlaySession.nightMode = SystemState.dark
        }
    }

    Item {
        id: keyScope
        anchors.fill: parent
        focus: true
        z: 0
        property real volumeBeforeMute: 0.5
        readonly property bool carPlayActive: CarPlaySession.running
        readonly property bool androidAutoActive: AndroidAutoSession.running
        readonly property bool projectionActive: carPlayActive || androidAutoActive
        readonly property bool streamActive: StreamSession.playing
        readonly property bool podcastActive: PodcastSession.playing
        readonly property bool radioActive: RadioSession.playing

        function toggleMute() {
            if (SystemState.volume > 0.001) {
                volumeBeforeMute = SystemState.volume
                SystemState.volume = 0
            } else {
                SystemState.volume = volumeBeforeMute > 0.001 ? volumeBeforeMute : 0.5
            }
        }

        function acceptCall() {
            if (carPlayActive) {
                CarPlaySession.sendHardKey("phone_accept", true)
                CarPlaySession.sendHardKey("phone_accept", false)
            }
            CallSession.answer()
        }

        function endCall() {
            if (carPlayActive) {
                const k = CallSession.ringing ? "phone_reject" : "phone_end"
                CarPlaySession.sendHardKey(k, true)
                CarPlaySession.sendHardKey(k, false)
            }
            CallSession.hangup()
        }

        function pulseProjection(key, down) {
            if (carPlayActive) {
                CarPlaySession.sendHardKey(key, down)
                return true
            }
            if (androidAutoActive) {
                AndroidAutoSession.sendHardKey(key, down)
                return true
            }
            return false
        }

        function triggerVoice() {
            if (carPlayActive) {
                CarPlaySession.sendHardKey("siri", true)
                CarPlaySession.sendHardKey("siri", false)
                return true
            }
            if (androidAutoActive) {
                AndroidAutoSession.sendHardKey("voice", true)
                AndroidAutoSession.sendHardKey("voice", false)
                return true
            }
            return false
        }

        function mediaToggle() {
            if (pulseProjection("playpause", true)) {
                pulseProjection("playpause", false)
                return
            }
            if (streamActive)
                StreamSession.toggle()
            else if (podcastActive)
                PodcastSession.toggle()
            else if (radioActive)
                RadioSession.toggle()
            else
                MediaSession.toggle()
        }

        function mediaPlay() {
            if (pulseProjection("play", true)) {
                pulseProjection("play", false)
                return
            }
            if (streamActive) {
                if (!StreamSession.playing)
                    StreamSession.toggle()
            } else if (podcastActive) {
                if (!PodcastSession.playing)
                    PodcastSession.toggle()
            } else if (radioActive) {
                if (!RadioSession.playing)
                    RadioSession.toggle()
            } else {
                MediaSession.play()
            }
        }

        function mediaPause() {
            if (pulseProjection("pause", true)) {
                pulseProjection("pause", false)
                return
            }
            if (streamActive)
                StreamSession.stop()
            else if (podcastActive)
                PodcastSession.stop()
            else if (radioActive)
                RadioSession.stop()
            else
                MediaSession.pause()
        }

        function mediaNext() {
            if (pulseProjection("next", true)) {
                pulseProjection("next", false)
                return
            }
            if (streamActive)
                StreamSession.next()
            else if (podcastActive)
                PodcastSession.next()
            else if (radioActive)
                RadioSession.next()
            else
                MediaSession.next()
        }

        function mediaPrev() {
            if (pulseProjection("prev", true)) {
                pulseProjection("prev", false)
                return
            }
            if (streamActive)
                StreamSession.previous()
            else if (podcastActive)
                PodcastSession.previous()
            else if (radioActive)
                RadioSession.previous()
            else
                MediaSession.previous()
        }

        function cycleMode() {
            if (projectionActive)
                return
            if (streamActive) {
                StreamSession.stop()
                PodcastSession.toggle()
            } else if (podcastActive) {
                PodcastSession.stop()
                RadioSession.toggle()
            } else if (radioActive) {
                RadioSession.stop()
                MediaSession.play()
            } else {
                MediaSession.pause()
                StreamSession.toggle()
            }
        }

        Keys.enabled: true
        Keys.onPressed: function (event) {
            window.bumpIdle()
            if (lockActive) {
                window.dismissLock()
                event.accepted = true
                return
            }
            if (CameraService.reverseActive) {
                if (event.key === Qt.Key_Escape) {
                    CameraService.dismissReverse()
                    event.accepted = true
                }
                return
            }
            if (event.key === Qt.Key_VolumeUp || event.key === Qt.Key_Plus) {
                if (SystemState.volume <= 0.001 && volumeBeforeMute > 0.001)
                    SystemState.volume = volumeBeforeMute
                else
                    SystemState.volume = Math.min(1, SystemState.volume + 0.05)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_VolumeDown || event.key === Qt.Key_Minus) {
                SystemState.volume = Math.max(0, SystemState.volume - 0.05)
                if (SystemState.volume > 0.001)
                    volumeBeforeMute = SystemState.volume
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_VolumeMute) {
                toggleMute()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_Call || event.key === Qt.Key_ToggleCallHangup) {
                if (CallSession.ringing) {
                    acceptCall()
                    event.accepted = true
                    return
                }
                if (CallSession.active && event.key === Qt.Key_ToggleCallHangup) {
                    endCall()
                    event.accepted = true
                    return
                }
            }
            if (event.key === Qt.Key_Hangup) {
                if (CallSession.ringing || CallSession.active) {
                    endCall()
                    event.accepted = true
                    return
                }
            }
            if (event.key === Qt.Key_Mode_switch) {
                cycleMode()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_VoiceDial || event.key === Qt.Key_F2) {
                triggerVoice()
                event.accepted = true
                return
            }
            if (projectionActive && (event.key === Qt.Key_Escape || event.key === Qt.Key_Home || event.key === Qt.Key_Back)) {
                pulseProjection("home", true)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaTogglePlayPause
                    || (projectionActive && event.key === Qt.Key_Space)) {
                if (projectionActive)
                    pulseProjection("playpause", true)
                else
                    mediaToggle()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPlay) {
                if (projectionActive)
                    pulseProjection("play", true)
                else
                    mediaPlay()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPause) {
                if (projectionActive)
                    pulseProjection("pause", true)
                else
                    mediaPause()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaNext) {
                if (projectionActive)
                    pulseProjection("next", true)
                else
                    mediaNext()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPrevious) {
                if (projectionActive)
                    pulseProjection("prev", true)
                else
                    mediaPrev()
                event.accepted = true
                return
            }
        }
        Keys.onReleased: function (event) {
            if (!projectionActive)
                return
            let key = ""
            if (event.key === Qt.Key_MediaTogglePlayPause || event.key === Qt.Key_Space)
                key = "playpause"
            else if (event.key === Qt.Key_MediaPlay)
                key = "play"
            else if (event.key === Qt.Key_MediaPause)
                key = "pause"
            else if (event.key === Qt.Key_MediaNext)
                key = "next"
            else if (event.key === Qt.Key_MediaPrevious)
                key = "prev"
            else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Home || event.key === Qt.Key_Back)
                key = "home"
            if (key.length > 0) {
                pulseProjection(key, false)
                event.accepted = true
            }
        }
    }

    StatusBar {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBar.barH
        z: 5
        visible: !window.projectionFullscreen && !CameraService.reverseActive
        darkContent: stage.opened
        running: stage.background
        runningAll: stage.running
        onOpenApp: function (entry) { stage.open(entry) }
        onDismissApp: function (id) { stage.dismiss(id) }
    }

    Connections {
        target: FileBrowser
        function onRequestOpenApp(appId) {
            const info = AppCatalog.appInfo(appId)
            if (info.entry)
                stage.open(info.entry)
        }
    }

    Item {
        id: appTray
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBar.drawerOpen && stage.running.length > 0 ? statusBar.trayH : 0
        visible: height > 0.5
        clip: true
        z: 5

        Behavior on height {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }

        GlassPanel {
            anchors.fill: parent
            radius: 0
            sourceItem: wall
            visible: !stage.opened
            fill: SystemState.dark ? "#A61C1C1E" : "#99F2F2F7"
            stroke: SystemState.dark ? "#59FFFFFF" : "#66FFFFFF"
            strokeWidth: 1 / Screen.devicePixelRatio
            blurAmount: 1.0
            blurMax: 56
        }

        Rectangle {
            anchors.fill: parent
            visible: stage.opened
            color: SystemState.dark ? "#CC1C1C1E" : "#CCF2F2F7"
            border.color: SystemState.separator
            border.width: 1
        }

        DragHandler {
            id: trayPull
            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            dragThreshold: 8
            property bool moved: false
            property real lastDy: 0
            property real lastVy: 0

            onActiveChanged: {
                if (active) {
                    moved = false
                    lastDy = 0
                    lastVy = 0
                } else if (moved) {
                    if (lastDy < -16 || lastVy < -380)
                        statusBar.drawerOpen = false
                }
            }
            onTranslationChanged: {
                if (!active)
                    return
                lastDy = translation.y
                lastVy = centroid.velocity.y
                if (lastDy < -6 && Math.abs(lastDy) > Math.abs(translation.x))
                    moved = true
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 14
            z: 1
            Repeater {
                model: stage.running
                delegate: Rectangle {
                    width: 48
                    height: 48
                    radius: 11
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.lighter(modelData.color, 1.22) }
                        GradientStop { position: 1.0; color: modelData.color }
                    }
                    border.width: 1
                    border.color: "#33FFFFFF"
                    AppGlyph {
                        anchors.fill: parent
                        anchors.margins: 9
                        appId: modelData.appId
                        visible: true
                    }
                    MouseArea {
                        property bool held: false
                        anchors.fill: parent
                        onPressed: held = false
                        onPressAndHold: {
                            held = true
                            stage.dismiss(modelData.appId)
                        }
                        onClicked: {
                            if (!held)
                                stage.open(modelData.entry)
                        }
                        onReleased: held = false
                    }
                }
            }
        }
    }

    Item {
        anchors.top: window.projectionFullscreen ? parent.top : appTray.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        HomeScreen {
            id: home
            anchors.fill: parent
            visible: !stage.opened
            glassSource: wall
            onOpenApp: function(entry) { stage.open(entry) }
        }

        AppStage {
            id: stage
            anchors.fill: parent
            glassSource: wall
        }

        MouseArea {
            anchors.fill: parent
            enabled: statusBar.drawerOpen
            z: 100
            onPressed: function (mouse) {
                statusBar.drawerOpen = false
                mouse.accepted = false
            }
        }

        Connections {
            target: CarPlaySession
            function onHostUiRequested() {
                stage.close()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: (1 - SystemState.brightness) * 0.55
        enabled: false
        visible: opacity > 0.001 && !window.projectionFullscreen && !CameraService.reverseActive
        z: 1
    }

    ClusterNav {
        z: 7
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16
        anchors.bottomMargin: 48
        visible: NavSession.active && !CameraService.reverseActive && !window.projectionFullscreen && !lockActive && !bootSplash.visible
    }

    Item {
        id: reverseHud
        z: 80
        anchors.fill: parent
        visible: CameraService.reverseActive

        function parkValue(i) {
            if (i === 0) return VehicleState.parkRl
            if (i === 1) return VehicleState.parkRcl
            if (i === 2) return VehicleState.parkRcr
            return VehicleState.parkRr
        }
        function parkLabel(i) {
            if (i === 0) return "左外"
            if (i === 1) return "左内"
            if (i === 2) return "右内"
            return "右外"
        }
        function parkColor(v) {
            if (v >= 0.72) return "#FF453A"
            if (v >= 0.42) return "#FFD60A"
            if (v >= 0.18) return "#30D158"
            return "#33FFFFFF"
        }
        function parkBars(v) {
            if (v >= 0.72) return 4
            if (v >= 0.48) return 3
            if (v >= 0.28) return 2
            if (v >= 0.12) return 1
            return 0
        }

        CameraVideoItem {
            anchors.fill: parent
            session: CameraService
        }

        Canvas {
            anchors.fill: parent
            opacity: 0.55
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width
                const h = height
                ctx.strokeStyle = "#99FFFFFF"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(w * 0.28, h * 0.42)
                ctx.quadraticCurveTo(w * 0.5, h * 0.78, w * 0.72, h * 0.42)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(w * 0.34, h * 0.48)
                ctx.quadraticCurveTo(w * 0.5, h * 0.72, w * 0.66, h * 0.48)
                ctx.stroke()
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            width: badge.implicitWidth + 24
            height: 36
            radius: 8
            color: "#CCFF453A"
            Text {
                id: badge
                anchors.centerIn: parent
                text: VehicleState.parkAlert ? "注意障碍" : "倒车影像"
                color: "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            width: backLabel.implicitWidth + 28
            height: 36
            radius: 8
            color: "#99000000"
            border.color: "#66FFFFFF"
            border.width: 1
            Text {
                id: backLabel
                anchors.centerIn: parent
                text: "返回"
                color: "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: CameraService.dismissReverse()
            }
        }

        Row {
            id: radarRow
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 44
            spacing: 10
            Repeater {
                model: 4
                delegate: Column {
                    required property int index
                    readonly property real level: reverseHud.parkValue(index)
                    spacing: 6
                    width: 52
                    Item {
                        width: parent.width
                        height: 72
                        Column {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 3
                            Repeater {
                                model: 4
                                Rectangle {
                                    required property int index
                                    width: 40 - index * 4
                                    height: 12
                                    radius: 3
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    readonly property int active: reverseHud.parkBars(level)
                                    opacity: (4 - index) <= active ? 1 : 0.22
                                    color: (4 - index) <= active ? reverseHud.parkColor(level) : "#33FFFFFF"
                                }
                            }
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: reverseHud.parkLabel(index)
                        color: "#CCFFFFFF"
                        font.pixelSize: 11
                    }
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            text: CameraService.demoMode ? "演示画面 · 车辆页切 R 档可测" : CameraService.status
            color: "#CCFFFFFF"
            font.pixelSize: 14
        }
    }

    Item {
        z: 6
        visible: !vkb.shown && !window.projectionFullscreen && !CameraService.reverseActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        width: 200
        height: 36

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            width: 134
            height: 5
            radius: 2.5
            color: !stage.opened ? "#E6FFFFFF" : (SystemState.dark ? "#66FFFFFF" : "#4D000000")
            opacity: 0.9
        }

        MouseArea {
            anchors.fill: parent
            enabled: stage.opened
            property real pressY: 0
            onPressed: function (m) { pressY = m.y }
            onReleased: function (m) {
                if (pressY - m.y > 16)
                    stage.close()
            }
            onClicked: stage.close()
        }
    }

    VirtualKeyboard {
        id: vkb
        z: 90
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        allowed: !window.projectionFullscreen && !CameraService.reverseActive
    }

    Timer {
        id: vkbHideDelay
        interval: 80
        onTriggered: {
            const item = window.activeFocusItem
            if (vkb.bindFrom(item))
                return
            if (item && vkbContains(item))
                return
            vkb.hide()
        }
    }

    function vkbContains(item) {
        let p = item
        while (p) {
            if (p === vkb)
                return true
            p = p.parent
        }
        return false
    }

    onActiveFocusItemChanged: {
        const item = activeFocusItem
        if (vkb.bindFrom(item)) {
            vkbHideDelay.stop()
            return
        }
        if (item && vkbContains(item)) {
            if (vkb.target)
                vkb.target.forceActiveFocus()
            return
        }
        if (vkb.open)
            vkbHideDelay.restart()
    }

    MouseArea {
        anchors.fill: parent
        z: 70
        enabled: !lockActive
        acceptedButtons: Qt.AllButtons
        propagateComposedEvents: true
        onPressed: function (mouse) {
            window.bumpIdle()
            mouse.accepted = false
        }
        onWheel: function (wheel) {
            window.bumpIdle()
            wheel.accepted = false
        }
    }

    LockScreen {
        id: lockScreen
        z: 95
        active: window.lockActive
        onUnlockRequested: window.dismissLock()
    }

    BootSplash {
        id: bootSplash
        anchors.fill: parent
    }
}
