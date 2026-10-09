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

    onRunningAllChanged: {
        if (runningAll.length === 0)
            drawerOpen = false
    }
    readonly property string lyricLine: {
        const table = {
            "夜路": ["这条夜路没有灯", "只有车灯往前", "远光切开浓雾"],
            "城市灯火": ["城市灯火一盏盏", "都落在车窗上", "红灯把影子拉长"],
            "回程": ["回程的路变短了", "歌还在单曲循环", "电台只剩沙沙声"],
            "晴空": ["晴空把影子拉长", "风从侧窗进来", "云缝漏下一束光"],
            "江岸": ["江岸的风很轻", "把后视镜吹凉", "水纹推着旧时光"]
        }
        const lines = table[MediaSession.title] || [MediaSession.title || "未在播放"]
        if (lines.length === 0)
            return ""
        const pos = root.musicLive ? MediaSession.position : 0
        return lines[Math.floor(pos / 3) % lines.length]
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
            font.family: "Segoe UI"
            font.pixelSize: root.fontMain
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
        }

        Text {
            id: city
            visible: Weather.place.length > 0
            text: Weather.place
            color: root.ink
            font.pixelSize: root.fontMain
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
        }

        Text {
            id: sky
            visible: Weather.condition.length > 0
            text: Weather.condition
            color: root.ink
            font.pixelSize: root.fontMain
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
        }

        Text {
            id: temp
            visible: Weather.condition.length > 0
            text: Weather.temperature + "°"
            color: root.ink
            font.pixelSize: root.fontMain
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
        }

        Text {
            id: travelAlert
            visible: Weather.travelAlert.length > 0 && root.mode === ""
            width: visible ? 240 : 0
            text: Weather.travelAlert
            elide: Text.ElideRight
            color: root.darkContent ? "#B71C1C" : "#FFEBEE"
            font.pixelSize: root.fontSmall
            font.bold: true
            style: root.darkContent ? Text.Normal : Text.Raised
            styleColor: "#66000000"
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
        }
    }

    }

    MouseArea {
        anchors.fill: parent
        visible: root.runningAll.length > 0
        z: 40
        propagateComposedEvents: true
        property real pressY: 0
        property real pressX: 0
        property bool dragging: false
        onPressed: function (m) {
            pressY = m.y
            pressX = m.x
            dragging = false
            mouse.accepted = false
        }
        onPositionChanged: function (m) {
            if (!pressed)
                return
            const dy = m.y - pressY
            const dx = m.x - pressX
            if (!dragging && Math.abs(dy) > 10 && Math.abs(dy) > Math.abs(dx) * 1.15)
                dragging = true
            if (dragging)
                mouse.accepted = true
        }
        onReleased: function (m) {
            if (dragging) {
                const dy = m.y - pressY
                if (dy > 8)
                    root.drawerOpen = true
                else if (dy < -8)
                    root.drawerOpen = false
                mouse.accepted = true
            } else {
                mouse.accepted = false
            }
            dragging = false
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
            width: lyricText.width
            Text {
                id: lyricText
                width: Math.min(implicitWidth, root.width - leftRow.width - statusRight.width - 24)
                text: root.lyricLine
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: root.ink
                font.pixelSize: root.fontHub
                font.bold: true
                style: root.darkContent ? Text.Normal : Text.Raised
                styleColor: "#66000000"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    const info = AppCatalog.appInfo("music")
                    if (info.entry)
                        root.openApp(info.entry)
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
                        font.bold: true
                        style: root.darkContent ? Text.Normal : Text.Raised
                        styleColor: "#66000000"
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        const info = AppCatalog.appInfo("map")
                        if (info.entry)
                            root.openApp(info.entry)
                    }
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
                font.bold: true
                style: root.darkContent ? Text.Normal : Text.Raised
                styleColor: "#66000000"
            }
            Text {
                text: "限速" + NavSession.speedLimit
                color: root.ink
                font.pixelSize: root.fontMain
                font.bold: true
                style: root.darkContent ? Text.Normal : Text.Raised
                styleColor: "#66000000"
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
        }
    }

}
