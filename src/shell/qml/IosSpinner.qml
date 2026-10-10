import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property color ink: SystemState.tint
    property bool running: true
    width: 22
    height: 22

    Canvas {
        id: ring
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            if (!ctx)
                return
            ctx.reset()
            const s = Math.min(width, height)
            const c = s / 2
            const r = s * 0.38
            ctx.lineWidth = Math.max(2, s * 0.12)
            ctx.lineCap = "round"
            ctx.strokeStyle = root.ink
            ctx.beginPath()
            ctx.arc(c, c, r, 0.15 * Math.PI, 1.55 * Math.PI)
            ctx.stroke()
        }
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    onInkChanged: ring.requestPaint()

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: 900
        loops: Animation.Infinite
        running: root.running && root.visible
    }
}
