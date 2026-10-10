import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    anchors.fill: parent
    visible: opacity > 0.001
    opacity: active ? 1 : 0
    enabled: active

    property bool active: false
    signal unlockRequested()

    Behavior on opacity {
        NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Text {
        id: weatherLine
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 52
        visible: Weather.place.length > 0 || Weather.condition.length > 0
        text: {
            let s = Weather.place
            if (Weather.condition.length > 0)
                s = s.length > 0 ? (s + "  ·  " + Weather.condition) : Weather.condition
            if (Weather.place.length > 0 || Weather.condition.length > 0)
                s += "  " + Weather.temperature + "°"
            return s
        }
        color: "#8AFFFFFF"
        font.pixelSize: 17
        font.letterSpacing: 0.6
    }

    Item {
        id: dial
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -8
        width: 288
        height: 288

        Canvas {
            id: face
            anchors.fill: parent
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                const w = width
                const h = height
                const cx = w / 2
                const cy = h / 2
                const r = Math.min(w, h) / 2 - 4
                ctx.reset()
                ctx.clearRect(0, 0, w, h)

                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                ctx.fillStyle = "#14FFFFFF"
                ctx.fill()
                ctx.lineWidth = 1.5
                ctx.strokeStyle = "#40FFFFFF"
                ctx.stroke()

                for (let i = 0; i < 60; ++i) {
                    const a = (i / 60) * Math.PI * 2 - Math.PI / 2
                    const major = (i % 5) === 0
                    const r1 = r - (major ? 16 : 8)
                    const r0 = r - 3
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0)
                    ctx.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1)
                    ctx.strokeStyle = major ? "#E6FFFFFF" : "#4DFFFFFF"
                    ctx.lineWidth = major ? 2 : 1
                    ctx.stroke()
                }

                ctx.fillStyle = "#E8FFFFFF"
                ctx.font = "500 16px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                for (let n = 1; n <= 12; ++n) {
                    const a = (n / 12) * Math.PI * 2 - Math.PI / 2
                    const tr = r - 34
                    ctx.fillText(String(n), cx + Math.cos(a) * tr, cy + Math.sin(a) * tr)
                }
            }
        }

        Canvas {
            id: hands
            anchors.fill: parent
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                const w = width
                const h = height
                const cx = w / 2
                const cy = h / 2
                const r = Math.min(w, h) / 2 - 4
                const now = new Date()
                const sec = now.getSeconds() + now.getMilliseconds() / 1000
                const min = now.getMinutes() + sec / 60
                const hr = (now.getHours() % 12) + min / 60

                ctx.reset()
                ctx.clearRect(0, 0, w, h)
                ctx.lineCap = "round"

                function hand(angle, len, width, color) {
                    const a = angle * Math.PI * 2 - Math.PI / 2
                    ctx.beginPath()
                    ctx.moveTo(cx - Math.cos(a) * len * 0.12, cy - Math.sin(a) * len * 0.12)
                    ctx.lineTo(cx + Math.cos(a) * len, cy + Math.sin(a) * len)
                    ctx.strokeStyle = color
                    ctx.lineWidth = width
                    ctx.stroke()
                }

                hand(hr / 12, r * 0.46, 5.5, "#F5F5F7")
                hand(min / 60, r * 0.66, 3.5, "#FFFFFF")
                hand(sec / 60, r * 0.74, 1.5, "#FF453A")

                ctx.beginPath()
                ctx.arc(cx, cy, 5, 0, Math.PI * 2)
                ctx.fillStyle = "#FF453A"
                ctx.fill()
                ctx.beginPath()
                ctx.arc(cx, cy, 2.2, 0, Math.PI * 2)
                ctx.fillStyle = "#FFFFFF"
                ctx.fill()
            }
        }

        Timer {
            interval: 250
            running: root.active
            repeat: true
            onTriggered: hands.requestPaint()
        }

        Component.onCompleted: {
            face.requestPaint()
            hands.requestPaint()
        }
        onWidthChanged: {
            face.requestPaint()
            hands.requestPaint()
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: dial.bottom
        anchors.topMargin: 32
        spacing: 8

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: {
                void SystemState.time
                const d = new Date()
                const weeks = ["日", "一", "二", "三", "四", "五", "六"]
                return (d.getMonth() + 1) + "月" + d.getDate() + "日  星期" + weeks[d.getDay()]
            }
            color: "#66FFFFFF"
            font.pixelSize: 15
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: SystemState.time
            color: "#B3FFFFFF"
            font.pixelSize: 20
            font.family: "Menlo"
            font.letterSpacing: 1.2
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        text: "轻触解锁"
        color: "#40FFFFFF"
        font.pixelSize: 13
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.unlockRequested()
    }
}
