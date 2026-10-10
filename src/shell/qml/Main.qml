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
    readonly property bool lockBlocked: MediaSession.playing
        || RadioSession.playing
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

        function pulseCarPlay(key, down) {
            if (!carPlayActive)
                return false
            CarPlaySession.sendHardKey(key, down)
            return true
        }

        function mediaToggle() {
            if (pulseCarPlay("playpause", true)) {
                pulseCarPlay("playpause", false)
                return
            }
            if (radioActive)
                RadioSession.toggle()
            else
                MediaSession.toggle()
        }

        function mediaPlay() {
            if (pulseCarPlay("play", true)) {
                pulseCarPlay("play", false)
                return
            }
            if (radioActive) {
                if (!RadioSession.playing)
                    RadioSession.toggle()
            } else {
                MediaSession.play()
            }
        }

        function mediaPause() {
            if (pulseCarPlay("pause", true)) {
                pulseCarPlay("pause", false)
                return
            }
            if (radioActive)
                RadioSession.stop()
            else
                MediaSession.pause()
        }

        function mediaNext() {
            if (pulseCarPlay("next", true)) {
                pulseCarPlay("next", false)
                return
            }
            if (radioActive)
                RadioSession.next()
            else
                MediaSession.next()
        }

        function mediaPrev() {
            if (pulseCarPlay("prev", true)) {
                pulseCarPlay("prev", false)
                return
            }
            if (radioActive)
                RadioSession.previous()
            else
                MediaSession.previous()
        }

        function cycleMode() {
            if (carPlayActive)
                return
            if (radioActive) {
                RadioSession.stop()
                MediaSession.play()
            } else {
                MediaSession.pause()
                RadioSession.toggle()
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
            if (carPlayActive && (event.key === Qt.Key_Escape || event.key === Qt.Key_Home || event.key === Qt.Key_Back)) {
                CarPlaySession.sendHardKey("home", true)
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaTogglePlayPause
                    || (carPlayActive && event.key === Qt.Key_Space)) {
                if (carPlayActive)
                    CarPlaySession.sendHardKey("playpause", true)
                else
                    mediaToggle()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPlay) {
                if (carPlayActive)
                    CarPlaySession.sendHardKey("play", true)
                else
                    mediaPlay()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPause) {
                if (carPlayActive)
                    CarPlaySession.sendHardKey("pause", true)
                else
                    mediaPause()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaNext) {
                if (carPlayActive)
                    CarPlaySession.sendHardKey("next", true)
                else
                    mediaNext()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_MediaPrevious) {
                if (carPlayActive)
                    CarPlaySession.sendHardKey("prev", true)
                else
                    mediaPrev()
                event.accepted = true
                return
            }
        }
        Keys.onReleased: function (event) {
            if (!carPlayActive)
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
            if (key.length > 0) {
                CarPlaySession.sendHardKey(key, false)
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
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
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
        anchors.top: (stage.currentId === "carplay" && CarPlaySession.hasVideo) ? parent.top : appTray.bottom
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
        visible: opacity > 0.001 && !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
        z: 1
    }

    Item {
        z: 80
        anchors.fill: parent
        visible: CameraService.reverseActive

        CameraVideoItem {
            anchors.fill: parent
            session: CameraService
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
                text: "倒车影像"
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

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            text: CameraService.demoMode ? "演示画面 · 车辆页切 R 档可测" : CameraService.status
            color: "#CCFFFFFF"
            font.pixelSize: 14
        }
    }

    Item {
        z: 6
        visible: !vkb.shown && !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
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
        allowed: !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
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
