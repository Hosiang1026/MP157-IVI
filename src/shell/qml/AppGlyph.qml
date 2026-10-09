import QtQuick

Canvas {
    id: canvas
    property string appId
    antialiasing: true
    renderTarget: Canvas.FramebufferObject
    renderStrategy: Canvas.Cooperative

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.imageSmoothingEnabled = true
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
        else if (appId === "weather")
            paintWeather(ctx, s)
        else if (appId === "airplay")
            paintAirPlay(ctx, s)
        else if (appId === "dlna")
            paintDlna(ctx, s)
        else if (appId === "dashcam")
            paintDashcam(ctx, s)
    }

    onAppIdChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function paintMusic(ctx, s) {
        ctx.save()
        ctx.translate(s * 0.30, s * 0.72)
        ctx.rotate(-0.38)
        ctx.scale(1.35, 1)
        ctx.beginPath()
        ctx.arc(0, 0, s * 0.105, 0, Math.PI * 2)
        ctx.fill()
        ctx.restore()
        const stemX = s * 0.48
        const stemTop = s * 0.16
        const stemH = s * 0.50
        const stemW = s * 0.075
        ctx.beginPath()
        ctx.moveTo(stemX, stemTop)
        ctx.lineTo(stemX + stemW, stemTop)
        ctx.lineTo(stemX + stemW, stemTop + stemH)
        ctx.lineTo(stemX, stemTop + stemH)
        ctx.closePath()
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(stemX + stemW, stemTop)
        ctx.bezierCurveTo(s * 0.78, s * 0.08, s * 0.92, s * 0.22, s * 0.86, s * 0.36)
        ctx.bezierCurveTo(s * 0.82, s * 0.44, s * 0.72, s * 0.42, stemX + stemW, s * 0.34)
        ctx.closePath()
        ctx.fill()
    }

    function paintPhone(ctx, s) {
        ctx.save()
        ctx.translate(s * 0.5, s * 0.5)
        ctx.rotate(-0.52)
        ctx.translate(-s * 0.5, -s * 0.5)
        const w = s * 0.13
        ctx.lineWidth = w
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.18)
        ctx.bezierCurveTo(s * 0.12, s * 0.18, s * 0.10, s * 0.42, s * 0.26, s * 0.50)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.72, s * 0.82)
        ctx.bezierCurveTo(s * 0.88, s * 0.82, s * 0.90, s * 0.58, s * 0.74, s * 0.50)
        ctx.stroke()
        ctx.lineWidth = w * 0.85
        ctx.beginPath()
        ctx.moveTo(s * 0.30, s * 0.48)
        ctx.lineTo(s * 0.70, s * 0.52)
        ctx.stroke()
        ctx.restore()
    }

    function paintVehicle(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.08, s * 0.60)
        ctx.lineTo(s * 0.16, s * 0.48)
        ctx.bezierCurveTo(s * 0.24, s * 0.34, s * 0.36, s * 0.28, s * 0.48, s * 0.28)
        ctx.lineTo(s * 0.64, s * 0.28)
        ctx.bezierCurveTo(s * 0.78, s * 0.30, s * 0.88, s * 0.42, s * 0.94, s * 0.52)
        ctx.lineTo(s * 0.94, s * 0.66)
        ctx.quadraticCurveTo(s * 0.94, s * 0.70, s * 0.88, s * 0.70)
        ctx.lineTo(s * 0.12, s * 0.70)
        ctx.quadraticCurveTo(s * 0.06, s * 0.70, s * 0.06, s * 0.66)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.34, s * 0.36)
        ctx.lineTo(s * 0.54, s * 0.36)
        ctx.quadraticCurveTo(s * 0.62, s * 0.36, s * 0.66, s * 0.48)
        ctx.lineTo(s * 0.32, s * 0.48)
        ctx.quadraticCurveTo(s * 0.30, s * 0.36, s * 0.34, s * 0.36)
        ctx.closePath()
        ctx.fill()
        ctx.beginPath()
        ctx.arc(s * 0.28, s * 0.70, s * 0.095, 0, Math.PI * 2)
        ctx.arc(s * 0.72, s * 0.70, s * 0.095, 0, Math.PI * 2)
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
        ctx.beginPath()
        ctx.arc(s * 0.28, s * 0.70, s * 0.038, 0, Math.PI * 2)
        ctx.arc(s * 0.72, s * 0.70, s * 0.038, 0, Math.PI * 2)
        ctx.fill()
    }

    function paintSettings(ctx, s) {
        const cx = s * 0.5
        const cy = s * 0.5
        const outer = s * 0.42
        const mid = s * 0.30
        const teeth = 8
        ctx.save()
        ctx.translate(cx, cy)
        ctx.beginPath()
        for (let i = 0; i < teeth; ++i) {
            const a0 = (i / teeth) * Math.PI * 2 - Math.PI / 2
            const step = (Math.PI * 2) / teeth
            const a1 = a0 + step * 0.18
            const a2 = a0 + step * 0.38
            const a3 = a0 + step * 0.62
            const a4 = a0 + step * 0.82
            if (i === 0)
                ctx.moveTo(Math.cos(a0) * mid, Math.sin(a0) * mid)
            else
                ctx.lineTo(Math.cos(a0) * mid, Math.sin(a0) * mid)
            ctx.lineTo(Math.cos(a1) * mid, Math.sin(a1) * mid)
            ctx.lineTo(Math.cos(a2) * outer, Math.sin(a2) * outer)
            ctx.lineTo(Math.cos(a3) * outer, Math.sin(a3) * outer)
            ctx.lineTo(Math.cos(a4) * mid, Math.sin(a4) * mid)
        }
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.arc(0, 0, s * 0.125, 0, Math.PI * 2)
        ctx.fill()
        ctx.restore()
    }

    function paintStore(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.22, s * 0.38)
        ctx.lineTo(s * 0.78, s * 0.38)
        ctx.lineTo(s * 0.72, s * 0.86)
        ctx.quadraticCurveTo(s * 0.50, s * 0.92, s * 0.28, s * 0.86)
        ctx.closePath()
        ctx.fill()
        ctx.lineWidth = s * 0.07
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.40, s * 0.18, Math.PI * 1.05, Math.PI * 1.95)
        ctx.stroke()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.42, s * 0.50)
        ctx.lineTo(s * 0.42, s * 0.72)
        ctx.lineTo(s * 0.62, s * 0.61)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
    }

    function paintMap(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.34, s * 0.175, 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.32, s * 0.42)
        ctx.quadraticCurveTo(s * 0.50, s * 0.48, s * 0.68, s * 0.42)
        ctx.lineTo(s * 0.50, s * 0.88)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.34, s * 0.07, 0, Math.PI * 2)
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
    }

    function paintVideo(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.30, s * 0.18)
        ctx.lineTo(s * 0.30, s * 0.82)
        ctx.lineTo(s * 0.82, s * 0.50)
        ctx.closePath()
        ctx.fill()
    }

    function paintRadio(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.40, s * 0.64, s * 0.15, 0, Math.PI * 2)
        ctx.fill()
        ctx.lineWidth = s * 0.065
        ctx.beginPath()
        ctx.arc(s * 0.40, s * 0.64, s * 0.28, -2.15, -0.75)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.40, s * 0.64, s * 0.40, -2.05, -0.85)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.40, s * 0.64, s * 0.06, 0, Math.PI * 2)
        ctx.fill()
    }

    function paintCarPlay(ctx, s) {
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

    function paintWeather(ctx, s) {
        const cx = s * 0.5
        const cy = s * 0.5
        ctx.beginPath()
        ctx.arc(cx, cy, s * 0.18, 0, Math.PI * 2)
        ctx.fill()
        ctx.lineWidth = s * 0.07
        for (let i = 0; i < 8; ++i) {
            const a = -Math.PI / 2 + i * Math.PI / 4
            ctx.beginPath()
            ctx.moveTo(cx + Math.cos(a) * s * 0.28, cy + Math.sin(a) * s * 0.28)
            ctx.lineTo(cx + Math.cos(a) * s * 0.40, cy + Math.sin(a) * s * 0.40)
            ctx.stroke()
        }
    }

    function paintAirPlay(ctx, s) {
        ctx.lineWidth = s * 0.065
        ctx.beginPath()
        ctx.moveTo(s * 0.24, s * 0.40)
        ctx.quadraticCurveTo(s * 0.50, s * 0.08, s * 0.76, s * 0.40)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.34, s * 0.50)
        ctx.quadraticCurveTo(s * 0.50, s * 0.26, s * 0.66, s * 0.50)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.50, s * 0.82)
        ctx.lineTo(s * 0.30, s * 0.56)
        ctx.quadraticCurveTo(s * 0.50, s * 0.60, s * 0.70, s * 0.56)
        ctx.closePath()
        ctx.fill()
    }

    function paintDlna(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.18, s * 0.28)
        ctx.lineTo(s * 0.82, s * 0.28)
        ctx.quadraticCurveTo(s * 0.88, s * 0.28, s * 0.88, s * 0.34)
        ctx.lineTo(s * 0.88, s * 0.62)
        ctx.quadraticCurveTo(s * 0.88, s * 0.68, s * 0.82, s * 0.68)
        ctx.lineTo(s * 0.18, s * 0.68)
        ctx.quadraticCurveTo(s * 0.12, s * 0.68, s * 0.12, s * 0.62)
        ctx.lineTo(s * 0.12, s * 0.34)
        ctx.quadraticCurveTo(s * 0.12, s * 0.28, s * 0.18, s * 0.28)
        ctx.closePath()
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.36, s * 0.74)
        ctx.lineTo(s * 0.64, s * 0.74)
        ctx.lineTo(s * 0.58, s * 0.68)
        ctx.lineTo(s * 0.42, s * 0.68)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.40, s * 0.36)
        ctx.lineTo(s * 0.40, s * 0.58)
        ctx.lineTo(s * 0.64, s * 0.47)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
    }

    function paintDashcam(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.20, s * 0.40)
        ctx.lineTo(s * 0.34, s * 0.40)
        ctx.lineTo(s * 0.40, s * 0.28)
        ctx.lineTo(s * 0.78, s * 0.28)
        ctx.quadraticCurveTo(s * 0.86, s * 0.28, s * 0.86, s * 0.36)
        ctx.lineTo(s * 0.86, s * 0.68)
        ctx.quadraticCurveTo(s * 0.86, s * 0.76, s * 0.78, s * 0.76)
        ctx.lineTo(s * 0.20, s * 0.76)
        ctx.quadraticCurveTo(s * 0.12, s * 0.76, s * 0.12, s * 0.68)
        ctx.lineTo(s * 0.12, s * 0.48)
        ctx.quadraticCurveTo(s * 0.12, s * 0.40, s * 0.20, s * 0.40)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.arc(s * 0.54, s * 0.52, s * 0.145, 0, Math.PI * 2)
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
        ctx.beginPath()
        ctx.arc(s * 0.54, s * 0.52, s * 0.065, 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.arc(s * 0.24, s * 0.50, s * 0.035, 0, Math.PI * 2)
        ctx.fill()
    }
}
