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
    readonly property int fontHub: 18
    readonly property int fontSmall: 14
    height: barH
    signal openApp(string entry)
    signal dismissApp(string id)
    readonly property bool wifiUp: SystemState.wifi && SystemState.wifiName.length > 0
    readonly property color ink: darkContent ? "#000000" : "#FFFFFF"
    readonly property bool navLive: NavSession.active
    readonly property bool musicAppOpen: {
        for (let i = 0; i < runningAll.length; ++i) {
            if (runningAll[i].appId === "music")
                return true
        }
        return false
    }
    readonly property bool musicLive: MediaSession.playing && musicAppOpen
    readonly property string mode: navLive ? "nav" : (musicLive ? "music" : "")
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
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#4D000000"
        }

        Text {
            id: city
            visible: root.mode === "" && Weather.place.length > 0
            text: Weather.place
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.Medium
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#4D000000"
        }

        Text {
            id: sky
            visible: root.mode === "" && Weather.condition.length > 0
            text: Weather.condition
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.Medium
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#4D000000"
        }

        Text {
            id: temp
            visible: root.mode === "" && Weather.condition.length > 0
            text: Weather.temperature + "°"
            color: root.ink
            font.pixelSize: root.fontMain
            font.weight: Font.Medium
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#4D000000"
        }

        Text {
            id: travelAlert
            visible: Weather.travelAlert.length > 0
            width: visible ? (root.mode === "" ? 240 : 160) : 0
            text: Weather.travelAlert
            elide: Text.ElideRight
            color: root.darkContent ? "#B71C1C" : "#FFEBEE"
            font.pixelSize: root.fontSmall
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("weather")
            }
        }

        Rectangle {
            visible: CallSession.active
            width: visible ? callText.width + 18 : 0
            height: 22
            radius: 11
            color: "#30D158"
            Text {
                id: callText
                anchors.centerIn: parent
                text: "通话"
                color: "#FFFFFF"
                font.pixelSize: 13
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("phone")
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
                    ctx.font = "600 " + root.fontHub + "px sans-serif"
                    ctx.textAlign = "left"
                    ctx.textBaseline = "middle"
                    const y = height * 0.5
                    const played = root.darkContent ? "#FF2D55" : "#FF375F"
                    const rest = root.darkContent ? "#3A3A3C" : "#99FFFFFF"
                    const p = Math.max(0, Math.min(1, musicRow.wipe))
                    const g = ctx.createLinearGradient(0, 0, width, 0)
                    const edge = 0.06
                    g.addColorStop(0, played)
                    g.addColorStop(Math.max(0, p - edge), played)
                    g.addColorStop(Math.min(1, p + edge), rest)
                    g.addColorStop(1, rest)
                    ctx.fillStyle = g
                    ctx.fillText(text, 0, y)
                }
                onWidthChanged: requestPaint()
            }
            Text {
                id: lyricMetrics
                visible: false
                text: root.hubMusicText
                font.pixelSize: root.fontHub
                font.weight: Font.DemiBold
                width: lyricPaint.maxW
                elide: Text.ElideRight
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
                function onLyricProgressChanged() { musicRow.wipe = root.lyricProgress }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.openById("music")
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
                            const accent = root.darkContent ? "#248A3D" : "#30D158"
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
                        }
                    }
                    Text {
                        id: navText
                        width: Math.min(implicitWidth, root.width - leftRow.width - statusRight.width - 56)
                        text: NavSession.text
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        color: root.ink
                        font.pixelSize: root.fontHub
                        font.weight: Font.DemiBold
                        style: root.darkContent ? Text.Normal : Text.Raised
                        styleColor: "#4D000000"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.openById("map")
                }
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
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: VehicleState.speed + " km/h "
                color: VehicleState.speed > NavSession.speedLimit ? "#FF3B30" : root.ink
                font.pixelSize: root.fontMain
                font.weight: Font.DemiBold
                style: root.darkContent ? Text.Normal : Text.Raised
                styleColor: "#4D000000"
            }
            Text {
                text: "限速" + NavSession.speedLimit
                color: root.ink
                font.pixelSize: root.fontMain
                font.weight: Font.DemiBold
                style: root.darkContent ? Text.Normal : Text.Raised
                styleColor: "#4D000000"
            }
        }

        Canvas {
            id: loc
            width: 14
            height: 17
            visible: NavSession.active
            anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.imageSmoothingEnabled = true
                const cx = width * 0.5
                const bulbY = height * 0.45
                const r = width * 0.34
                ctx.fillStyle = root.ink
                ctx.globalAlpha = 0.92
                ctx.beginPath()
                ctx.moveTo(cx, height - 0.2)
                ctx.lineTo(cx - r, bulbY)
                ctx.arc(cx, bulbY, r, Math.PI, 0)
                ctx.lineTo(cx + r, bulbY)
                ctx.closePath()
                ctx.fill()
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.arc(cx, bulbY - 0.05, r * 0.34, 0, Math.PI * 2)
                ctx.fill()
                ctx.globalCompositeOperation = "source-over"
                ctx.globalAlpha = 1
            }
            Connections {
                target: Weather
                function onUpdated() { loc.requestPaint() }
            }
            Connections {
                target: root
                function onDarkContentChanged() { loc.requestPaint() }
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
                const idle = root.darkContent ? "#3C3C43" : "#EBEBF5"
                const up = root.wifiUp
                const level = up ? SystemState.wifiSignal : 0
                const a0 = Math.PI * 1.22
                const a1 = Math.PI * 1.78
                ctx.lineCap = "round"
                ctx.lineWidth = 2.2
                const arcs = [5.0, 8.8, 12.6]
                for (let i = 0; i < arcs.length; ++i) {
                    const lit = up && level >= i + 1
                    ctx.strokeStyle = lit ? active : idle
                    ctx.globalAlpha = lit ? 1 : (SystemState.wifi ? 0.32 : 0.45)
                    ctx.beginPath()
                    ctx.arc(cx, cy, arcs[i], a0, a1)
                    ctx.stroke()
                }
                ctx.globalAlpha = 1
                const dotOn = up && level >= 1
                ctx.fillStyle = dotOn ? active : idle
                ctx.globalAlpha = dotOn ? 1 : (SystemState.wifi ? 0.32 : 0.45)
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
            }
            Connections {
                target: SystemState
                function onDarkChanged() { wifi.requestPaint() }
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
                const idle = root.darkContent ? "#3C3C43" : "#EBEBF5"
                ctx.strokeStyle = on ? active : idle
                ctx.globalAlpha = on ? 1 : 0.42
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
            }
            Connections {
                target: SystemState
                function onDarkChanged() { bt.requestPaint() }
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
                    width: Math.max(1, 22 * (SystemState.battery < 0 ? 100 : SystemState.battery) / 100)
                    height: 9
                    radius: 1.5
                    color: SystemState.battery >= 0 && SystemState.battery <= 20 ? "#FF3B30" : root.ink
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
                onClicked: root.openById("settings")
            }
        }
    }

}
