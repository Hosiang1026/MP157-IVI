import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property bool darkContent: false
    property var running: []
    property var runningAll: []
    property bool drawerOpen: false
    readonly property int barH: 40
    readonly property int trayH: 72
    readonly property int fontMain: 17
    height: barH
    signal openApp(string entry)
    signal dismissApp(string id)
    readonly property bool wifiUp: SystemState.wifi && SystemState.wifiName.length > 0
    readonly property bool lightInk: darkContent ? SystemState.dark : WallpaperStore.darkBackdrop
    readonly property color ink: darkContent
                                 ? SystemState.ink
                                 : (WallpaperStore.darkBackdrop ? "#FFFFFF" : "#000000")
    readonly property color mute: darkContent
                                  ? SystemState.secondary
                                  : (WallpaperStore.darkBackdrop ? "#EBEBF5" : "#3C3C43")
    readonly property bool raisedInk: !darkContent && WallpaperStore.darkBackdrop
    readonly property bool callLive: CallSession.active || CallSession.ringing
    readonly property bool navLive: NavSession.active
    readonly property bool vehicleAlert: VehicleState.alertCount > 0
    readonly property bool muted: SystemState.volume <= 0.001
    readonly property int batteryPct: {
        const v = VehicleState.batteryVoltage
        const lo = 11.8
        const hi = 12.6
        return Math.round(Math.max(0, Math.min(100, (v - lo) / (hi - lo) * 100)))
    }
    readonly property bool linkCarPlay: CarPlaySession.running
    readonly property bool linkAirPlay: AirPlayMirror.hasVideo
    readonly property bool linkAA: AndroidAutoSession.running
    readonly property bool musicAppOpen: {
        for (let i = 0; i < runningAll.length; ++i) {
            if (runningAll[i].appId === "music")
                return true
        }
        return false
    }
    readonly property bool musicLive: MediaSession.playing && (musicAppOpen || MediaSession.source === "carplay")
    readonly property string mode: callLive ? "call" : (navLive ? "nav" : (musicLive ? "music" : (vehicleAlert ? "alert" : "")))
    function lyricLines() {
        const src = MediaSession.lyrics
        const out = []
        if (!src || src.length === undefined)
            return out
        for (let i = 0; i < src.length; ++i) {
            const v = src[i]
            if (v === undefined || v === null)
                continue
            const s = ("" + v).trim()
            if (s.length && s !== "undefined")
                out.push(s)
        }
        return out
    }

    readonly property string hubMusicText: {
        const lines = lyricLines()
        if (lines.length > 0) {
            const n = lines.length
            const dur = Math.max(1, MediaSession.duration)
            const idx = Math.min(n - 1, Math.max(0, Math.floor(MediaSession.position * n / dur)))
            if (lines[idx] && lines[idx].length)
                return lines[idx]
        }
        const title = (MediaSession.title || "").toString()
        if (!title.length)
            return "未在播放"
        const artist = (MediaSession.artist || "").toString()
        return artist.length ? (title + " · " + artist) : title
    }
    readonly property real lyricProgress: {
        const n = lyricLines().length
        const dur = Math.max(1, MediaSession.duration)
        const pos = Math.max(0, MediaSession.position)
        if (n > 1) {
            const span = dur / n
            const t = pos - Math.min(n - 1, Math.floor(pos / span)) * span
            return Math.max(0, Math.min(1, t / span))
        }
        return Math.max(0, Math.min(1, pos / dur))
    }

    function openById(id) {
        const info = AppCatalog.appInfo(id)
        if (info.entry)
            root.openApp(info.entry)
    }

    function carPlayPulse(key) {
        if (!CarPlaySession.running)
            return false
        CarPlaySession.sendHardKey(key, true)
        CarPlaySession.sendHardKey(key, false)
        return true
    }

    function acceptCall() {
        if (root.carPlayPulse("phone_accept"))
            return
        CallSession.answer()
    }

    function endCall() {
        if (CallSession.ringing)
            root.carPlayPulse("phone_reject")
        else
            root.carPlayPulse("phone_end")
        CallSession.hangup()
    }

    onRunningAllChanged: {
        if (runningAll.length === 0)
            drawerOpen = false
    }

    Rectangle {
        anchors.fill: parent
        visible: root.darkContent
        color: SystemState.page
    }

    Item {
        id: barStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.barH

    Row {
        id: leftRow
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Text {
            id: clock
            text: SystemState.time
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.DemiBold
            style: root.raisedInk ? Text.Raised : Text.Normal
            styleColor: "#4D000000"
        }

        Text {
            id: city
            visible: Weather.place.length > 0
            text: Weather.place
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.DemiBold
            style: root.raisedInk ? Text.Raised : Text.Normal
            styleColor: "#4D000000"
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("weather")
            }
        }

        Canvas {
            id: wxIcon
            width: 22
            height: 22
            visible: Weather.condition.length > 0 || Weather.kind.length > 0
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)
                ctx.imageSmoothingEnabled = true
                const s = Math.min(width, height)
                const cx = width * 0.5
                const cy = height * 0.5
                const k = Weather.kind
                const day = Weather.day
                const rain = k === "rain" || k === "rainMid" || k === "rainHard" || k === "thunder"
                             || k === "ponding" || k === "sleet" || k === "freezeRain" || k === "typhoon"
                const snow = k === "snow" || k === "snowMid" || k === "snowHard" || k === "blizzard" || k === "hail"
                const cloud = k === "cloudy" || k === "overcast" || k === "fog" || k === "haze"
                              || k === "dust" || k === "sandLift" || k === "wetRoad" || k === "wind" || rain || snow
                if (!cloud) {
                    ctx.fillStyle = day ? "#FFD60A" : root.ink
                    ctx.strokeStyle = day ? "#FFD60A" : root.ink
                    ctx.beginPath()
                    ctx.arc(cx, cy, s * 0.22, 0, Math.PI * 2)
                    ctx.fill()
                    if (day) {
                        ctx.lineWidth = s * 0.08
                        ctx.lineCap = "round"
                        for (let i = 0; i < 8; ++i) {
                            const a = -Math.PI / 2 + i * Math.PI / 4
                            ctx.beginPath()
                            ctx.moveTo(cx + Math.cos(a) * s * 0.32, cy + Math.sin(a) * s * 0.32)
                            ctx.lineTo(cx + Math.cos(a) * s * 0.44, cy + Math.sin(a) * s * 0.44)
                            ctx.stroke()
                        }
                    } else {
                        ctx.globalCompositeOperation = "destination-out"
                        ctx.beginPath()
                        ctx.arc(cx + s * 0.12, cy - s * 0.06, s * 0.2, 0, Math.PI * 2)
                        ctx.fill()
                        ctx.globalCompositeOperation = "source-over"
                    }
                } else {
                    if (day && (k === "cloudy" || k === "wind")) {
                        ctx.fillStyle = "#FFD60A"
                        ctx.beginPath()
                        ctx.arc(cx + s * 0.18, cy - s * 0.16, s * 0.14, 0, Math.PI * 2)
                        ctx.fill()
                    }
                    ctx.fillStyle = root.ink
                    ctx.globalAlpha = root.raisedInk ? 0.92 : 0.88
                    ctx.beginPath()
                    ctx.arc(cx - s * 0.12, cy + s * 0.02, s * 0.16, 0, Math.PI * 2)
                    ctx.arc(cx + s * 0.02, cy - s * 0.04, s * 0.18, 0, Math.PI * 2)
                    ctx.arc(cx + s * 0.16, cy + s * 0.02, s * 0.14, 0, Math.PI * 2)
                    ctx.rect(cx - s * 0.28, cy + s * 0.02, s * 0.56, s * 0.16)
                    ctx.fill()
                    ctx.globalAlpha = 1
                    if (rain) {
                        ctx.strokeStyle = root.lightInk ? "#64D2FF" : "#007AFF"
                        ctx.lineWidth = s * 0.07
                        ctx.lineCap = "round"
                        for (let i = 0; i < 3; ++i) {
                            const x = cx - s * 0.12 + i * s * 0.12
                            ctx.beginPath()
                            ctx.moveTo(x, cy + s * 0.22)
                            ctx.lineTo(x - s * 0.04, cy + s * 0.38)
                            ctx.stroke()
                        }
                    } else if (snow) {
                        ctx.fillStyle = root.ink
                        for (let i = 0; i < 3; ++i) {
                            ctx.beginPath()
                            ctx.arc(cx - s * 0.1 + i * s * 0.12, cy + s * 0.3, s * 0.045, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }
            }
            Connections {
                target: Weather
                function onUpdated() { wxIcon.requestPaint() }
            }
            Connections {
                target: root
                function onInkChanged() { wxIcon.requestPaint() }
                function onLightInkChanged() { wxIcon.requestPaint() }
                function onRaisedInkChanged() { wxIcon.requestPaint() }
                function onDarkContentChanged() { wxIcon.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("weather")
            }
        }

        Text {
            id: temp
            visible: Weather.condition.length > 0 || Weather.kind.length > 0
            text: Weather.temperature + "°"
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.DemiBold
            style: root.raisedInk ? Text.Raised : Text.Normal
            styleColor: "#4D000000"
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("weather")
            }
        }

        Text {
            id: travelAlert
            visible: Weather.travelAlert.length > 0
            width: visible ? (root.mode === "" || root.mode === "alert" ? 240 : 160) : 0
            text: Weather.travelAlert
            elide: Text.ElideRight
            color: root.lightInk ? "#FFEBEE" : "#B71C1C"
            font.pixelSize: root.fontMain
            font.weight: Font.DemiBold
            style: root.raisedInk ? Text.Raised : Text.Normal
            styleColor: "#4D000000"
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("weather")
            }
        }

    }

    }

    Item {
        id: pullZone
        z: 40
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.barH + (root.drawerOpen ? 0 : 28)
        enabled: root.runningAll.length > 0
        visible: enabled

        DragHandler {
            id: barPull
            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            dragThreshold: 10
            property bool moved: false
            property real lastDy: 0
            property real lastVy: 0

            onActiveChanged: {
                if (active) {
                    moved = false
                    lastDy = 0
                    lastVy = 0
                } else if (moved) {
                    if (lastDy > 18 || lastVy > 420)
                        root.drawerOpen = true
                    else if (lastDy < -18 || lastVy < -420)
                        root.drawerOpen = false
                }
            }
            onTranslationChanged: {
                if (!active)
                    return
                lastDy = translation.y
                lastVy = centroid.velocity.y
                if (Math.abs(lastDy) > 8 && Math.abs(lastDy) > Math.abs(translation.x) * 1.1)
                    moved = true
            }
        }
    }

    Item {
        id: centerHub
        visible: root.mode !== ""
        z: 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.barH
        clip: false
        enabled: visible

        Item {
            id: musicRow
            visible: root.mode === "music"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: 24
            width: lyricPaint.width
            property bool snapWipe: false
            property real wipe: 0
            onWipeChanged: lyricPaint.requestPaint()
            Behavior on wipe {
                enabled: !musicRow.snapWipe
                NumberAnimation { duration: 420; easing.type: Easing.Linear }
            }
            onVisibleChanged: {
                snapWipe = true
                wipe = root.lyricProgress
                snapWipe = false
            }
            Canvas {
                id: lyricPaint
                height: 24
                readonly property real maxW: Math.max(40, root.width - leftRow.width - statusRight.width - 24)
                width: Math.min(lyricMetrics.contentWidth > 0 ? lyricMetrics.contentWidth : lyricMetrics.implicitWidth, maxW)
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.clearRect(0, 0, width, height)
                    const text = lyricMetrics.elidedText || lyricMetrics.text || ""
                    if (!text.length || text === "undefined")
                        return
                    ctx.font = "600 " + root.fontMain + "px sans-serif"
                    ctx.textAlign = "left"
                    ctx.textBaseline = "middle"
                    const y = height * 0.5
                    const played = root.lightInk ? "#FF375F" : "#FF2D55"
                    const rest = root.raisedInk ? "#99FFFFFF"
                               : root.lightInk ? SystemState.secondary
                               : (root.darkContent ? SystemState.secondary : "#99000000")
                    const p = Math.max(0, Math.min(1, musicRow.wipe))
                    const g = ctx.createLinearGradient(0, 0, width, 0)
                    const edge = 0.06
                    g.addColorStop(0, played)
                    g.addColorStop(Math.max(0, p - edge), played)
                    g.addColorStop(Math.min(1, p + edge), rest)
                    g.addColorStop(1, rest)
                    ctx.fillStyle = g
                    if (root.raisedInk) {
                        ctx.shadowColor = "#4D000000"
                        ctx.shadowBlur = 0
                        ctx.shadowOffsetX = 0
                        ctx.shadowOffsetY = 1
                    }
                    ctx.fillText(text, 0, y)
                }
                onWidthChanged: requestPaint()
            }
            Text {
                id: lyricMetrics
                visible: false
                text: root.hubMusicText
                font.pixelSize: root.fontMain
                font.weight: Font.DemiBold
                width: lyricPaint.maxW
                elide: Text.ElideRight
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("music")
            }
            Connections {
                target: root
                function onHubMusicTextChanged() {
                    musicRow.snapWipe = true
                    musicRow.wipe = 0
                    musicRow.snapWipe = false
                    lyricPaint.requestPaint()
                }
                function onDarkContentChanged() { lyricPaint.requestPaint() }
                function onInkChanged() { lyricPaint.requestPaint() }
                function onLightInkChanged() { lyricPaint.requestPaint() }
                function onRaisedInkChanged() { lyricPaint.requestPaint() }
                function onLyricProgressChanged() { musicRow.wipe = root.lyricProgress }
            }
        }

        Item {
            id: callRow
            visible: root.mode === "call"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: 30
            width: callChip.width
            Rectangle {
                id: callChip
                width: callInner.width + 18
                height: 30
                radius: 15
                color: SystemState.dark ? "#661C1C1E" : (root.darkContent ? "#99F2F2F7" : "#80F2F2F7")
                border.color: SystemState.separator
                border.width: 0.5
                Row {
                    id: callInner
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: CallSession.ringing ? "#FF9F0A" : "#30D158"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: {
                            const name = CallSession.contactName || CallSession.number || "通话"
                            return CallSession.ringing ? ("来电 · " + name) : ("通话中 · " + name)
                        }
                        color: root.ink
                        font.pixelSize: root.fontMain
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, root.width - leftRow.width - statusRight.width - 160)
                        style: root.raisedInk ? Text.Raised : Text.Normal
                        styleColor: "#4D000000"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.openById("phone")
                        }
                    }
                    Rectangle {
                        visible: CallSession.ringing
                        width: 52
                        height: 22
                        radius: 11
                        color: "#CC30D158"
                        border.color: "#40FFFFFF"
                        border.width: 0.5
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: "接听"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.acceptCall()
                        }
                    }
                    Rectangle {
                        width: 52
                        height: 22
                        radius: 11
                        color: "#CCFF3B30"
                        border.color: "#40FFFFFF"
                        border.width: 0.5
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: CallSession.ringing ? "拒接" : "挂断"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.endCall()
                        }
                    }
                }
            }
        }

        Row {
            id: navRow
            visible: root.mode === "nav"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Item {
                id: navTap
                height: 24
                width: 26 + 8 + navText.width
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Canvas {
                        id: arrow
                        width: 26
                        height: 24
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.imageSmoothingEnabled = true
                            const accent = root.lightInk ? "#30D158" : "#248A3D"
                            ctx.strokeStyle = accent
                            ctx.fillStyle = accent
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            const t = NavSession.turn
                            if (t === "left") {
                                ctx.beginPath()
                                ctx.moveTo(20, 3)
                                ctx.lineTo(4.5, 12)
                                ctx.lineTo(20, 20.5)
                                ctx.lineTo(16.2, 12)
                                ctx.closePath()
                                ctx.fill()
                                return
                            }
                            if (t === "right") {
                                ctx.beginPath()
                                ctx.moveTo(6, 3)
                                ctx.lineTo(21.5, 12)
                                ctx.lineTo(6, 20.5)
                                ctx.lineTo(9.8, 12)
                                ctx.closePath()
                                ctx.fill()
                                return
                            }
                            if (t === "arrive") {
                                ctx.beginPath()
                                ctx.arc(13, 12, 5.5, 0, Math.PI * 2)
                                ctx.fill()
                                return
                            }
                            ctx.lineWidth = 2.8
                            ctx.beginPath()
                            ctx.moveTo(13, 20)
                            ctx.lineTo(13, 4)
                            ctx.moveTo(7.2, 10)
                            ctx.lineTo(13, 4)
                            ctx.lineTo(18.8, 10)
                            ctx.stroke()
                        }
                        Connections {
                            target: NavSession
                            function onStepChanged() { arrow.requestPaint() }
                        }
                        Connections {
                            target: root
                            function onDarkContentChanged() { arrow.requestPaint() }
                            function onLightInkChanged() { arrow.requestPaint() }
                            function onInkChanged() { arrow.requestPaint() }
                        }
                    }
                    Text {
                        id: navText
                        width: Math.min(implicitWidth, root.width - leftRow.width - statusRight.width - 56)
                        text: NavSession.text
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        color: root.ink
                        font.pixelSize: root.fontMain
                        font.weight: Font.DemiBold
                        style: root.raisedInk ? Text.Raised : Text.Normal
                        styleColor: "#4D000000"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.openById("map")
                }
            }
        }

        Item {
            id: alertRow
            visible: root.mode === "alert"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            height: root.barH
            readonly property real maxW: Math.max(40, root.width - leftRow.width - statusRight.width - 32)
            width: Math.min(alertLabel.implicitWidth, maxW)

            Text {
                id: alertLabel
                anchors.centerIn: parent
                width: parent.width
                text: VehicleState.alertText
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: VehicleState.alertColor
                font.pixelSize: root.fontMain
                font.weight: Font.DemiBold
                style: root.raisedInk ? Text.Raised : Text.Normal
                styleColor: "#4D000000"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("vehicle")
            }
        }
    }

    Row {
        id: statusRight
        z: 1
        anchors.right: barStrip.right
        anchors.rightMargin: 18
        anchors.top: barStrip.top
        height: root.barH
        spacing: 9

        Row {
            visible: root.navLive
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter

            Item {
                id: speedGauge
                width: 44
                height: 32
                anchors.verticalCenter: parent.verticalCenter
                readonly property bool over: VehicleState.speed > NavSession.speedLimit
                readonly property real ratio: {
                    const lim = Math.max(30, NavSession.speedLimit)
                    return Math.max(0, Math.min(1.15, VehicleState.speed / lim))
                }

                Canvas {
                    id: speedArc
                    anchors.fill: parent
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.clearRect(0, 0, width, height)
                        ctx.imageSmoothingEnabled = true
                        const cx = width * 0.5
                        const cy = height * 0.72
                        const r = 15.5
                        const a0 = Math.PI * 1.12
                        const a1 = Math.PI * 1.88
                        const span = a1 - a0
                        ctx.lineCap = "round"
                        ctx.lineWidth = 3.2
                        ctx.strokeStyle = root.mute
                        ctx.globalAlpha = root.raisedInk ? 0.35 : 0.55
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, a0, a1)
                        ctx.stroke()
                        ctx.globalAlpha = 1
                        const p = Math.max(0, Math.min(1, speedGauge.ratio))
                        const accent = speedGauge.over ? "#FF3B30" : (root.lightInk ? "#30D158" : "#248A3D")
                        ctx.strokeStyle = accent
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, a0, a0 + span * p)
                        ctx.stroke()
                    }
                    Connections {
                        target: VehicleState
                        function onChanged() { speedArc.requestPaint() }
                    }
                    Connections {
                        target: NavSession
                        function onStepChanged() { speedArc.requestPaint() }
                    }
                    Connections {
                        target: root
                        function onInkChanged() { speedArc.requestPaint() }
                        function onMuteChanged() { speedArc.requestPaint() }
                        function onLightInkChanged() { speedArc.requestPaint() }
                        function onRaisedInkChanged() { speedArc.requestPaint() }
                        function onDarkContentChanged() { speedArc.requestPaint() }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    text: "" + VehicleState.speed
                    color: speedGauge.over ? "#FF3B30" : root.ink
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    style: root.raisedInk ? Text.Raised : Text.Normal
                    styleColor: "#4D000000"
                }
            }

            Item {
                width: 28
                height: 28
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "#FFFFFF"
                    border.color: speedGauge.over ? "#FF3B30" : "#E53935"
                    border.width: 2.6
                }
                Text {
                    anchors.centerIn: parent
                    text: "" + NavSession.speedLimit
                    color: "#111111"
                    font.pixelSize: NavSession.speedLimit >= 100 ? 11 : 13
                    font.weight: Font.Bold
                }
            }
        }

        Canvas {
            id: cpIcon
            width: 20
            height: 18
            visible: root.linkCarPlay
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)
                const s = Math.min(width, height)
                const ox = (width - s) / 2
                const oy = (height - s) / 2
                ctx.translate(ox, oy)
                ctx.fillStyle = root.ink
                ctx.beginPath()
                ctx.moveTo(s * 0.18, s * 0.56)
                ctx.lineTo(s * 0.26, s * 0.38)
                ctx.quadraticCurveTo(s * 0.34, s * 0.24, s * 0.50, s * 0.22)
                ctx.quadraticCurveTo(s * 0.66, s * 0.24, s * 0.74, s * 0.38)
                ctx.lineTo(s * 0.82, s * 0.56)
                ctx.quadraticCurveTo(s * 0.84, s * 0.62, s * 0.78, s * 0.66)
                ctx.lineTo(s * 0.22, s * 0.66)
                ctx.quadraticCurveTo(s * 0.16, s * 0.62, s * 0.18, s * 0.56)
                ctx.closePath()
                ctx.fill()
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.moveTo(s * 0.34, s * 0.36)
                ctx.quadraticCurveTo(s * 0.40, s * 0.30, s * 0.50, s * 0.29)
                ctx.quadraticCurveTo(s * 0.60, s * 0.30, s * 0.66, s * 0.36)
                ctx.lineTo(s * 0.62, s * 0.48)
                ctx.lineTo(s * 0.38, s * 0.48)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.arc(s * 0.30, s * 0.58, s * 0.04, 0, Math.PI * 2)
                ctx.arc(s * 0.70, s * 0.58, s * 0.04, 0, Math.PI * 2)
                ctx.fill()
                ctx.globalCompositeOperation = "source-over"
                ctx.beginPath()
                ctx.moveTo(s * 0.22, s * 0.66)
                ctx.lineTo(s * 0.26, s * 0.78)
                ctx.quadraticCurveTo(s * 0.30, s * 0.84, s * 0.38, s * 0.84)
                ctx.lineTo(s * 0.62, s * 0.84)
                ctx.quadraticCurveTo(s * 0.70, s * 0.84, s * 0.74, s * 0.78)
                ctx.lineTo(s * 0.78, s * 0.66)
                ctx.closePath()
                ctx.fill()
            }
            Connections {
                target: root
                function onInkChanged() { cpIcon.requestPaint() }
                function onLinkCarPlayChanged() { cpIcon.requestPaint() }
                function onDarkContentChanged() { cpIcon.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("carplay")
            }
        }

        Canvas {
            id: aaIcon
            width: 18
            height: 18
            visible: root.linkAA
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)
                const s = Math.min(width, height)
                ctx.fillStyle = root.ink
                ctx.beginPath()
                ctx.moveTo(s * 0.50, s * 0.10)
                ctx.lineTo(s * 0.86, s * 0.72)
                ctx.quadraticCurveTo(s * 0.88, s * 0.80, s * 0.78, s * 0.80)
                ctx.lineTo(s * 0.22, s * 0.80)
                ctx.quadraticCurveTo(s * 0.12, s * 0.80, s * 0.14, s * 0.72)
                ctx.closePath()
                ctx.fill()
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.moveTo(s * 0.50, s * 0.28)
                ctx.lineTo(s * 0.70, s * 0.64)
                ctx.lineTo(s * 0.30, s * 0.64)
                ctx.closePath()
                ctx.fill()
                ctx.globalCompositeOperation = "source-over"
                ctx.beginPath()
                ctx.arc(s * 0.34, s * 0.90, s * 0.05, 0, Math.PI * 2)
                ctx.arc(s * 0.66, s * 0.90, s * 0.05, 0, Math.PI * 2)
                ctx.fill()
            }
            Connections {
                target: root
                function onInkChanged() { aaIcon.requestPaint() }
                function onLinkAAChanged() { aaIcon.requestPaint() }
                function onDarkContentChanged() { aaIcon.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("androidauto")
            }
        }

        Canvas {
            id: airLink
            width: 16
            height: 16
            visible: root.linkAirPlay
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = root.ink
                ctx.fillStyle = root.ink
                ctx.lineWidth = 1.6
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.moveTo(2.5, 6.2)
                ctx.quadraticCurveTo(8, 1.2, 13.5, 6.2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(4.5, 8.2)
                ctx.quadraticCurveTo(8, 4.6, 11.5, 8.2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(8, 14.5)
                ctx.lineTo(4.2, 9.6)
                ctx.quadraticCurveTo(8, 10.2, 11.8, 9.6)
                ctx.closePath()
                ctx.fill()
            }
            Connections {
                target: root
                function onInkChanged() { airLink.requestPaint() }
                function onDarkContentChanged() { airLink.requestPaint() }
                function onLinkAirPlayChanged() { airLink.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("airplay")
            }
        }

        Canvas {
            id: muteIcon
            width: 18
            height: 16
            visible: root.muted
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = root.ink
                ctx.strokeStyle = root.ink
                ctx.lineWidth = 1.7
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.moveTo(1.5, 5.2)
                ctx.lineTo(5.2, 5.2)
                ctx.lineTo(10.2, 2.2)
                ctx.lineTo(10.2, 13.8)
                ctx.lineTo(5.2, 10.8)
                ctx.lineTo(1.5, 10.8)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(12.8, 4.2)
                ctx.lineTo(16.5, 11.8)
                ctx.moveTo(16.5, 4.2)
                ctx.lineTo(12.8, 11.8)
                ctx.stroke()
            }
            Connections {
                target: root
                function onInkChanged() { muteIcon.requestPaint() }
                function onDarkContentChanged() { muteIcon.requestPaint() }
                function onMutedChanged() { muteIcon.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("settings")
            }
        }

        Canvas {
            id: wifi
            width: 24
            height: 18
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.imageSmoothingEnabled = true
                const cx = width * 0.5
                const cy = height - 1.2
                const active = root.ink
                const idle = root.mute
                const up = root.wifiUp
                const level = up ? SystemState.wifiSignal : 0
                const a0 = Math.PI * 1.22
                const a1 = Math.PI * 1.78
                ctx.lineCap = "round"
                ctx.lineWidth = 2.2
                const arcs = [5.0, 8.8, 12.6]
                const dim = root.raisedInk ? (SystemState.wifi ? 0.32 : 0.45) : (SystemState.wifi ? 0.75 : 0.55)
                for (let i = 0; i < arcs.length; ++i) {
                    const lit = up && level >= i + 1
                    ctx.strokeStyle = lit ? active : idle
                    ctx.globalAlpha = lit ? 1 : dim
                    ctx.beginPath()
                    ctx.arc(cx, cy, arcs[i], a0, a1)
                    ctx.stroke()
                }
                ctx.globalAlpha = 1
                const dotOn = up && level >= 1
                ctx.fillStyle = dotOn ? active : idle
                ctx.globalAlpha = dotOn ? 1 : dim
                ctx.beginPath()
                ctx.arc(cx, cy, 1.7, 0, Math.PI * 2)
                ctx.fill()
                ctx.globalAlpha = 1
            }
            Connections {
                target: SystemState
                function onWifiChanged() { wifi.requestPaint() }
                function onWifiNameChanged() { wifi.requestPaint() }
                function onWifiSignalChanged() { wifi.requestPaint() }
            }
            Connections {
                target: root
                function onDarkContentChanged() { wifi.requestPaint() }
                function onInkChanged() { wifi.requestPaint() }
                function onMuteChanged() { wifi.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                onClicked: root.openById("settings")
            }
        }

        Canvas {
            id: bt
            width: 16
            height: 20
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.imageSmoothingEnabled = true
                const on = SystemState.bluetooth
                const active = root.ink
                const idle = root.mute
                ctx.strokeStyle = on ? active : idle
                ctx.globalAlpha = on ? 1 : (root.raisedInk ? 0.42 : 0.7)
                ctx.lineWidth = 1.9
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                const cx = 8
                ctx.beginPath()
                ctx.moveTo(cx, 1.4)
                ctx.lineTo(cx, 18.6)
                ctx.moveTo(cx, 5.6)
                ctx.lineTo(12.6, 2.2)
                ctx.moveTo(cx, 5.6)
                ctx.lineTo(12.6, 9.1)
                ctx.moveTo(cx, 14.2)
                ctx.lineTo(12.6, 17.6)
                ctx.moveTo(cx, 14.2)
                ctx.lineTo(12.6, 10.6)
                ctx.moveTo(cx, 5.6)
                ctx.lineTo(3.4, 2.2)
                ctx.moveTo(cx, 5.6)
                ctx.lineTo(3.4, 9.1)
                ctx.moveTo(cx, 14.2)
                ctx.lineTo(3.4, 17.6)
                ctx.moveTo(cx, 14.2)
                ctx.lineTo(3.4, 10.6)
                ctx.stroke()
                ctx.globalAlpha = 1
            }
            Connections {
                target: SystemState
                function onBluetoothChanged() { bt.requestPaint() }
            }
            Connections {
                target: root
                function onDarkContentChanged() { bt.requestPaint() }
                function onInkChanged() { bt.requestPaint() }
                function onMuteChanged() { bt.requestPaint() }
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                onClicked: root.openById("settings")
            }
        }

        Item {
            width: 34
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                width: 30
                height: 15
                radius: 3.5
                anchors.verticalCenter: parent.verticalCenter
                color: "transparent"
                border.color: root.ink
                border.width: 1.4
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(1, 22 * root.batteryPct / 100)
                    height: 9
                    radius: 1.5
                    color: (root.batteryPct <= 20 || VehicleState.batteryLow) ? "#FF3B30" : root.ink
                }
            }
            Rectangle {
                width: 2
                height: 5.5
                radius: 0.7
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: root.ink
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                onClicked: root.openById("vehicle")
            }
        }

    }

}
