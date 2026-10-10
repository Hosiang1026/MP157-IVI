import QtQuick

Canvas {
    id: canvas
    property string appId
    antialiasing: true
    renderTarget: Canvas.Image
    renderStrategy: Canvas.Cooperative

    function schedulePaint() {
        if (width > 1 && height > 1)
            requestPaint()
    }

    onPaint: {
        const ctx = getContext("2d")
        if (!ctx)
            return
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.globalCompositeOperation = "source-over"
        ctx.globalAlpha = 1
        ctx.imageSmoothingEnabled = true
        ctx.fillStyle = "#FFFFFF"
        ctx.strokeStyle = "#FFFFFF"
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const s = Math.min(width, height)
        if (s < 2 || appId.length === 0)
            return
        const ox = (width - s) / 2
        const oy = (height - s) / 2
        ctx.translate(ox, oy)
        const pad = s * 0.035
        ctx.translate(pad, pad)
        const gs = s - pad * 2
        ctx.lineWidth = Math.max(1.2, gs * 0.052)
        if (appId === "music")
            paintMusic(ctx, gs)
        else if (appId === "phone")
            paintPhone(ctx, gs)
        else if (appId === "vehicle")
            paintVehicle(ctx, gs)
        else if (appId === "settings")
            paintSettings(ctx, gs)
        else if (appId === "store")
            paintStore(ctx, gs)
        else if (appId === "radio")
            paintRadio(ctx, gs)
        else if (appId === "podcast")
            paintPodcast(ctx, gs)
        else if (appId === "stream")
            paintStream(ctx, gs)
        else if (appId === "video")
            paintVideo(ctx, gs)
        else if (appId === "map")
            paintMap(ctx, gs)
        else if (appId === "carplay")
            paintCarPlay(ctx, gs)
        else if (appId === "androidauto")
            paintAndroidAuto(ctx, gs)
        else if (appId === "weather")
            paintWeather(ctx, gs)
        else if (appId === "airplay")
            paintAirPlay(ctx, gs)
        else if (appId === "dlna")
            paintDlna(ctx, gs)
        else if (appId === "dashcam")
            paintDashcam(ctx, gs)
        else if (appId === "files")
            paintFiles(ctx, gs)
    }

    onAppIdChanged: schedulePaint()
    onWidthChanged: schedulePaint()
    onHeightChanged: schedulePaint()
    onVisibleChanged: if (visible) schedulePaint()
    Component.onCompleted: Qt.callLater(schedulePaint)

    function paintMusic(ctx, s) {
        const stemX = s * 0.46
        const stemTop = s * 0.14
        const stemH = s * 0.56
        const stemW = s * 0.062
        ctx.save()
        ctx.translate(s * 0.34, s * 0.70)
        ctx.rotate(-0.38)
        ctx.scale(1.28, 1)
        ctx.beginPath()
        ctx.arc(0, 0, s * 0.105, 0, Math.PI * 2)
        ctx.fill()
        ctx.restore()
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
        const w = s * 0.105
        ctx.lineWidth = w
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.18)
        ctx.bezierCurveTo(s * 0.12, s * 0.18, s * 0.10, s * 0.42, s * 0.26, s * 0.50)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.72, s * 0.82)
        ctx.bezierCurveTo(s * 0.88, s * 0.82, s * 0.90, s * 0.58, s * 0.74, s * 0.50)
        ctx.stroke()
        ctx.lineWidth = w * 0.82
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
        ctx.lineWidth = Math.max(1.2, s * 0.05)
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
        const cx = s * 0.50
        const cy = s * 0.58
        ctx.beginPath()
        ctx.arc(cx, cy, s * 0.12, 0, Math.PI * 2)
        ctx.fill()
        ctx.lineWidth = Math.max(1.2, s * 0.05)
        ctx.beginPath()
        ctx.arc(cx, cy, s * 0.26, -2.25, -0.9)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(cx, cy, s * 0.40, -2.15, -1.0)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(cx, s * 0.16)
        ctx.lineTo(cx, cy - s * 0.12)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(cx, s * 0.14, s * 0.045, 0, Math.PI * 2)
        ctx.fill()
    }

    function paintPodcast(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.38, s * 0.14, 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.36, s * 0.50)
        ctx.quadraticCurveTo(s * 0.50, s * 0.72, s * 0.64, s * 0.50)
        ctx.lineTo(s * 0.64, s * 0.78)
        ctx.quadraticCurveTo(s * 0.50, s * 0.88, s * 0.36, s * 0.78)
        ctx.closePath()
        ctx.fill()
        ctx.lineWidth = Math.max(1.2, s * 0.05)
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.38, s * 0.26, 0.35, Math.PI - 0.35)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.50, s * 0.38, s * 0.38, 0.45, Math.PI - 0.45)
        ctx.stroke()
    }

    function paintStream(ctx, s) {
        ctx.lineWidth = Math.max(1.4, s * 0.055)
        for (let i = 0; i < 5; ++i) {
            const x = s * (0.22 + i * 0.14)
            const h = s * (0.18 + ((i % 3) + 1) * 0.12)
            ctx.beginPath()
            ctx.moveTo(x, s * 0.72)
            ctx.lineTo(x, s * 0.72 - h)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(x, s * 0.72 - h, s * 0.035, 0, Math.PI * 2)
            ctx.fill()
        }
        ctx.beginPath()
        ctx.moveTo(s * 0.18, s * 0.82)
        ctx.lineTo(s * 0.82, s * 0.82)
        ctx.stroke()
    }

    function paintAndroidAuto(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.50, s * 0.14)
        ctx.lineTo(s * 0.84, s * 0.72)
        ctx.quadraticCurveTo(s * 0.86, s * 0.78, s * 0.78, s * 0.78)
        ctx.lineTo(s * 0.22, s * 0.78)
        ctx.quadraticCurveTo(s * 0.14, s * 0.78, s * 0.16, s * 0.72)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.50, s * 0.30)
        ctx.lineTo(s * 0.68, s * 0.62)
        ctx.lineTo(s * 0.32, s * 0.62)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
        ctx.beginPath()
        ctx.arc(s * 0.34, s * 0.86, s * 0.05, 0, Math.PI * 2)
        ctx.arc(s * 0.66, s * 0.86, s * 0.05, 0, Math.PI * 2)
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
        ctx.lineWidth = Math.max(1.2, s * 0.05)
        for (let i = 0; i < 8; ++i) {
            const a = -Math.PI / 2 + i * Math.PI / 4
            ctx.beginPath()
            ctx.moveTo(cx + Math.cos(a) * s * 0.28, cy + Math.sin(a) * s * 0.28)
            ctx.lineTo(cx + Math.cos(a) * s * 0.40, cy + Math.sin(a) * s * 0.40)
            ctx.stroke()
        }
    }

    function paintAirPlay(ctx, s) {
        ctx.lineWidth = Math.max(1.2, s * 0.048)
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

    function paintFiles(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.14, s * 0.34)
        ctx.lineTo(s * 0.14, s * 0.28)
        ctx.quadraticCurveTo(s * 0.14, s * 0.22, s * 0.20, s * 0.22)
        ctx.lineTo(s * 0.40, s * 0.22)
        ctx.lineTo(s * 0.48, s * 0.30)
        ctx.lineTo(s * 0.80, s * 0.30)
        ctx.quadraticCurveTo(s * 0.86, s * 0.30, s * 0.86, s * 0.36)
        ctx.lineTo(s * 0.86, s * 0.78)
        ctx.quadraticCurveTo(s * 0.86, s * 0.84, s * 0.80, s * 0.84)
        ctx.lineTo(s * 0.20, s * 0.84)
        ctx.quadraticCurveTo(s * 0.14, s * 0.84, s * 0.14, s * 0.78)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "destination-out"
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.48)
        ctx.lineTo(s * 0.72, s * 0.48)
        ctx.lineTo(s * 0.72, s * 0.56)
        ctx.lineTo(s * 0.28, s * 0.56)
        ctx.closePath()
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.62)
        ctx.lineTo(s * 0.58, s * 0.62)
        ctx.lineTo(s * 0.58, s * 0.70)
        ctx.lineTo(s * 0.28, s * 0.70)
        ctx.closePath()
        ctx.fill()
        ctx.globalCompositeOperation = "source-over"
    }
}
