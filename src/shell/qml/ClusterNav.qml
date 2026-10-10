import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    width: 280
    height: 88
    visible: NavSession.active
    opacity: visible ? 1 : 0

    Behavior on opacity { NumberAnimation { duration: 180 } }

    function turnLabel(t) {
        if (t === "left") return "左转"
        if (t === "right") return "右转"
        if (t === "arrive") return "到达"
        return "直行"
    }

    function distLabel(m) {
        if (m <= 0) return ""
        if (m >= 1000) return (m / 1000).toFixed(1) + " km"
        return m + " m"
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "#E6101216"
        border.color: "#40FFFFFF"
        border.width: 1

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Item {
                width: 56
                height: parent.height
                Canvas {
                    id: arrow
                    anchors.centerIn: parent
                    width: 44
                    height: 44
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.imageSmoothingEnabled = true
                        const accent = "#30D158"
                        ctx.strokeStyle = accent
                        ctx.fillStyle = accent
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        const t = NavSession.turn
                        if (t === "left") {
                            ctx.beginPath()
                            ctx.moveTo(34, 6)
                            ctx.lineTo(8, 22)
                            ctx.lineTo(34, 38)
                            ctx.lineTo(28, 22)
                            ctx.closePath()
                            ctx.fill()
                            return
                        }
                        if (t === "right") {
                            ctx.beginPath()
                            ctx.moveTo(10, 6)
                            ctx.lineTo(36, 22)
                            ctx.lineTo(10, 38)
                            ctx.lineTo(16, 22)
                            ctx.closePath()
                            ctx.fill()
                            return
                        }
                        if (t === "arrive") {
                            ctx.beginPath()
                            ctx.arc(22, 22, 10, 0, Math.PI * 2)
                            ctx.fill()
                            return
                        }
                        ctx.lineWidth = 4
                        ctx.beginPath()
                        ctx.moveTo(22, 36)
                        ctx.lineTo(22, 8)
                        ctx.moveTo(12, 18)
                        ctx.lineTo(22, 8)
                        ctx.lineTo(32, 18)
                        ctx.stroke()
                    }
                    Connections {
                        target: NavSession
                        function onStepChanged() { arrow.requestPaint() }
                        function onActiveChanged() { arrow.requestPaint() }
                    }
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 68
                spacing: 4
                Text {
                    text: root.distLabel(NavSession.distanceM)
                    color: "#FFFFFF"
                    font.pixelSize: 26
                    font.bold: true
                    visible: NavSession.distanceM > 0
                }
                Text {
                    width: parent.width
                    text: root.turnLabel(NavSession.turn) + " · " + NavSession.text
                    color: "#B3FFFFFF"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                Text {
                    text: "限速 " + NavSession.speedLimit + " · ETA " + NavSession.etaMin + " 分"
                    color: "#80FFFFFF"
                    font.pixelSize: 11
                    visible: NavSession.speedLimit > 0
                }
            }
        }
    }
}
