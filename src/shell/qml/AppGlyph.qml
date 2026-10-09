import QtQuick

Canvas {
    id: canvas
    property string appId

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.fillStyle = "#FFFFFF"
        ctx.strokeStyle = "#FFFFFF"
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const s = Math.min(width, height)
        const ox = (width - s) / 2
        const oy = (height - s) / 2
        ctx.translate(ox, oy)
        if (appId === "music")
            paintMusic(ctx, s)
        else if (appId === "phone")
            paintPhone(ctx, s)
        else if (appId === "vehicle")
            paintVehicle(ctx, s)
        else if (appId === "settings")
            paintSettings(ctx, s)
        else if (appId === "store")
            paintStore(ctx, s)
        else if (appId === "radio")
            paintRadio(ctx, s)
        else if (appId === "video")
            paintVideo(ctx, s)
        else if (appId === "map")
            paintMap(ctx, s)
        else if (appId === "carplay")
            paintCarPlay(ctx, s)
    }

    onAppIdChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function paintMusic(ctx, s) {
        ctx.beginPath()
        ctx.save()
        ctx.translate(s * 0.32, s * 0.74)
        ctx.rotate(-0.45)
        ctx.scale(1.25, 1)
        ctx.arc(0, 0, s * 0.13, 0, Math.PI * 2)
        ctx.fill()
        ctx.restore()
        ctx.fillRect(s * 0.44, s * 0.20, s * 0.075, s * 0.52)
        ctx.beginPath()
        ctx.moveTo(s * 0.515, s * 0.20)
        ctx.bezierCurveTo(s * 0.86, s * 0.05, s * 0.90, s * 0.30, s * 0.74, s * 0.42)
        ctx.bezierCurveTo(s * 0.64, s * 0.32, s * 0.56, s * 0.30, s * 0.515, s * 0.36)
        ctx.closePath()
        ctx.fill()
    }

    function paintPhone(ctx, s) {
        ctx.save()
        ctx.translate(s / 2, s / 2)
        ctx.rotate(-0.55)
        ctx.translate(-s / 2, -s / 2)
        ctx.lineWidth = s * 0.11
        ctx.beginPath()
        ctx.moveTo(s * 0.30, s * 0.22)
        ctx.bezierCurveTo(s * 0.18, s * 0.22, s * 0.16, s * 0.40, s * 0.28, s * 0.48)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.70, s * 0.78)
        ctx.bezierCurveTo(s * 0.82, s * 0.78, s * 0.84, s * 0.60, s * 0.72, s * 0.52)
        ctx.stroke()
        ctx.lineWidth = s * 0.09
        ctx.beginPath()
        ctx.moveTo(s * 0.32, s * 0.46)
        ctx.lineTo(s * 0.68, s * 0.54)
        ctx.stroke()
        ctx.restore()
    }

    function paintVehicle(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.10, s * 0.62)
        ctx.lineTo(s * 0.20, s * 0.46)
        ctx.bezierCurveTo(s * 0.28, s * 0.34, s * 0.40, s * 0.32, s * 0.48, s * 0.32)
        ctx.lineTo(s * 0.62, s * 0.32)
        ctx.bezierCurveTo(s * 0.74, s * 0.34, s * 0.82, s * 0.44, s * 0.90, s * 0.52)
        ctx.lineTo(s * 0.90, s * 0.66)
        ctx.lineTo(s * 0.10, s * 0.66)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.36, s * 0.40)
        ctx.lineTo(s * 0.50, s * 0.40)
        ctx.lineTo(s * 0.62, s * 0.50)
        ctx.lineTo(s * 0.34, s * 0.50)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
        ctx.beginPath()
        ctx.arc(s * 0.30, s * 0.68, s * 0.09, 0, Math.PI * 2)
        ctx.arc(s * 0.70, s * 0.68, s * 0.09, 0, Math.PI * 2)
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.arc(s * 0.30, s * 0.68, s * 0.04, 0, Math.PI * 2)
        ctx.arc(s * 0.70, s * 0.68, s * 0.04, 0, Math.PI * 2)
        ctx.fill()
    }

    function paintSettings(ctx, s) {
        const cx = s * 0.5
        const cy = s * 0.5
        const outer = s * 0.40
        const inner = s * 0.27
        const teeth = 8
        ctx.save()
        ctx.translate(cx, cy)
        ctx.beginPath()
        for (let i = 0; i < teeth; ++i) {
            const a0 = (i / teeth) * Math.PI * 2 - Math.PI / 2
            const step = Math.PI / teeth
            const a1 = a0 + step * 0.45
            const a2 = a0 + step * 1.15
            const a3 = a0 + step * 1.6
            if (i === 0)
                ctx.moveTo(Math.cos(a0) * inner, Math.sin(a0) * inner)
            else
                ctx.lineTo(Math.cos(a0) * inner, Math.sin(a0) * inner)
            ctx.lineTo(Math.cos(a1) * outer, Math.sin(a1) * outer)
            ctx.lineTo(Math.cos(a2) * outer, Math.sin(a2) * outer)
            ctx.lineTo(Math.cos(a3) * inner, Math.sin(a3) * inner)
        }
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.arc(0, 0, s * 0.11, 0, Math.PI * 2)
        ctx.fill()
        ctx.restore()
    }

    function paintStore(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.24, s * 0.40)
        ctx.lineTo(s * 0.76, s * 0.40)
        ctx.lineTo(s * 0.70, s * 0.84)
        ctx.quadraticCurveTo(s * 0.50, s * 0.90, s * 0.30, s * 0.84)
        ctx.closePath()
        ctx.fill()
        ctx.lineWidth = s * 0.075
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.42, s * 0.16, Math.PI * 1.08, Math.PI * 1.92)
        ctx.stroke()
    }

    function paintMap(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.36, s * 0.16, 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.36, s * 0.44)
        ctx.lineTo(s * 0.50, s * 0.82)
        ctx.lineTo(s * 0.64, s * 0.44)
        ctx.fill()
    }

    function paintVideo(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.36, s * 0.22)
        ctx.lineTo(s * 0.36, s * 0.78)
        ctx.lineTo(s * 0.80, s * 0.50)
        ctx.closePath()
        ctx.fill()
    }

    function paintRadio(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.42, s * 0.62, s * 0.16, 0, Math.PI * 2)
        ctx.fill()
        ctx.lineWidth = s * 0.07
        ctx.beginPath()
        ctx.arc(s * 0.42, s * 0.62, s * 0.30, -2.2, -0.7)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.42, s * 0.62, s * 0.42, -2.05, -0.85)
        ctx.stroke()
    }

    function paintCarPlay(ctx, s) {
        ctx.lineWidth = s * 0.08
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.22)
        ctx.lineTo(s * 0.72, s * 0.22)
        ctx.lineTo(s * 0.72, s * 0.70)
        ctx.lineTo(s * 0.50, s * 0.82)
        ctx.lineTo(s * 0.28, s * 0.70)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.40, s * 0.38)
        ctx.lineTo(s * 0.40, s * 0.58)
        ctx.lineTo(s * 0.58, s * 0.48)
        ctx.closePath()
        ctx.fill()
    }
}
