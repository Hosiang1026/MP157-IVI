import QtQuick

Item {
    id: root
    property string kind: "clear"
    property bool day: true
    property real t: 0

    NumberAnimation on t {
        from: 0
        to: Math.PI * 2
        duration: 4200
        loops: Animation.Infinite
        running: root.visible
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const s = Math.min(width, height)
            const cx = width / 2
            const cy = height / 2
            const k = root.kind
            const rain = k === "rain" || k === "rainMid" || k === "rainHard" || k === "thunder" || k === "ponding" || k === "sleet" || k === "freezeRain" || k === "typhoon"
            const snow = k === "snow" || k === "snowMid" || k === "snowHard" || k === "blizzard" || k === "hail"
            const cloud = k === "cloudy" || k === "overcast" || k === "fog" || k === "haze" || k === "dust" || k === "sandLift" || k === "wetRoad" || rain || snow || k === "wind"

            if (k === "clear" || (!cloud && root.day)) {
                const pulse = 1 + Math.sin(root.t) * 0.04
                ctx.fillStyle = root.day ? "#FFD60A" : "#E5E5EA"
                ctx.beginPath()
                ctx.arc(cx, cy, s * 0.18 * pulse, 0, Math.PI * 2)
                ctx.fill()
                if (root.day) {
                    ctx.strokeStyle = "#FFD60A"
                    ctx.lineWidth = s * 0.04
                    ctx.lineCap = "round"
                    for (let i = 0; i < 8; ++i) {
                        const a = root.t * 0.35 + i * Math.PI / 4
                        const r0 = s * 0.26
                        const r1 = s * 0.38
                        ctx.beginPath()
                        ctx.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0)
                        ctx.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1)
                        ctx.stroke()
                    }
                }
            } else if (!root.day && !cloud) {
                ctx.fillStyle = "#E5E5EA"
                ctx.beginPath()
                ctx.arc(cx - s * 0.04, cy, s * 0.2, 0, Math.PI * 2)
                ctx.fill()
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.arc(cx + s * 0.08, cy - s * 0.04, s * 0.18, 0, Math.PI * 2)
                ctx.fill()
                ctx.globalCompositeOperation = "source-over"
            }

            if (cloud) {
                const ox = Math.sin(root.t * 0.6) * s * 0.02
                ctx.fillStyle = k === "overcast" || k === "fog" || k === "haze" ? "#AEAEB2" : "#FFFFFF"
                ctx.beginPath()
                ctx.arc(cx - s * 0.12 + ox, cy + s * 0.02, s * 0.16, 0, Math.PI * 2)
                ctx.arc(cx + s * 0.02 + ox, cy - s * 0.04, s * 0.18, 0, Math.PI * 2)
                ctx.arc(cx + s * 0.16 + ox, cy + s * 0.02, s * 0.14, 0, Math.PI * 2)
                ctx.rect(cx - s * 0.28 + ox, cy + s * 0.02, s * 0.56, s * 0.16)
                ctx.fill()
            }

            if (rain) {
                ctx.strokeStyle = "#5AC8FA"
                ctx.lineWidth = s * 0.035
                ctx.lineCap = "round"
                for (let i = 0; i < 5; ++i) {
                    const x = cx - s * 0.18 + i * s * 0.09
                    const y = cy + s * 0.22 + ((root.t * 28 + i * 11) % (s * 0.18))
                    ctx.beginPath()
                    ctx.moveTo(x, y)
                    ctx.lineTo(x - s * 0.02, y + s * 0.1)
                    ctx.stroke()
                }
            }
            if (snow) {
                ctx.fillStyle = "#FFFFFF"
                for (let i = 0; i < 6; ++i) {
                    const x = cx - s * 0.18 + i * s * 0.08
                    const y = cy + s * 0.22 + ((root.t * 18 + i * 9) % (s * 0.2))
                    ctx.beginPath()
                    ctx.arc(x, y, s * 0.025, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
            if (k === "wind" || k === "typhoon") {
                ctx.strokeStyle = "#8E8E93"
                ctx.lineWidth = s * 0.035
                ctx.lineCap = "round"
                for (let i = 0; i < 3; ++i) {
                    const y = cy - s * 0.08 + i * s * 0.1
                    const phase = root.t * 1.4 + i
                    ctx.beginPath()
                    ctx.moveTo(cx - s * 0.25, y)
                    ctx.bezierCurveTo(cx - s * 0.05, y - Math.sin(phase) * s * 0.06, cx + s * 0.05, y + Math.sin(phase) * s * 0.06, cx + s * 0.25, y)
                    ctx.stroke()
                }
            }
        }

        Connections {
            target: root
            function onTChanged() { canvas.requestPaint() }
            function onKindChanged() { canvas.requestPaint() }
            function onDayChanged() { canvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
}
