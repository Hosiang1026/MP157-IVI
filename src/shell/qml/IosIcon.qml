import QtQuick

Canvas {
    id: root
    property string name: "play"
    property color ink: "#000000"
    antialiasing: true
    renderTarget: Canvas.Image
    renderStrategy: Canvas.Cooperative

    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        if (!ctx)
            return
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        const s = Math.min(width, height)
        if (s < 4)
            return
        const ox = (width - s) / 2
        const oy = (height - s) / 2
        ctx.translate(ox, oy)
        ctx.strokeStyle = root.ink
        ctx.fillStyle = root.ink
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.lineWidth = Math.max(1.5, s * 0.09)
        if (name === "play")
            paintPlay(ctx, s)
        else if (name === "pause")
            paintPause(ctx, s)
        else if (name === "next")
            paintSkip(ctx, s, true)
        else if (name === "prev")
            paintSkip(ctx, s, false)
        else if (name === "search")
            paintSearch(ctx, s)
        else if (name === "xmark")
            paintX(ctx, s)
        else if (name === "chevron")
            paintChevron(ctx, s)
        else if (name === "phone")
            paintPhone(ctx, s)
        else if (name === "phoneDown")
            paintPhoneDown(ctx, s)
        else if (name === "mic")
            paintMic(ctx, s)
        else if (name === "speaker")
            paintSpeaker(ctx, s)
        else if (name === "locate")
            paintLocate(ctx, s)
        else if (name === "plus")
            paintPlus(ctx, s)
        else if (name === "minus")
            paintMinus(ctx, s)
        else if (name === "folder")
            paintFolder(ctx, s)
        else if (name === "doc")
            paintDoc(ctx, s)
        else if (name === "trash")
            paintTrash(ctx, s)
        else if (name === "copy")
            paintCopy(ctx, s)
        else if (name === "back")
            paintBack(ctx, s)
        else if (name === "repeat")
            paintRepeat(ctx, s)
        else if (name === "repeat1")
            paintRepeat1(ctx, s)
        else if (name === "shuffle")
            paintShuffle(ctx, s)
        else if (name === "keypad")
            paintKeypad(ctx, s)
        else if (name === "person")
            paintPerson(ctx, s)
        else if (name === "clock")
            paintClock(ctx, s)
        else if (name === "delete")
            paintDelete(ctx, s)
    }

    function paintPlay(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.34, s * 0.22)
        ctx.lineTo(s * 0.78, s * 0.5)
        ctx.lineTo(s * 0.34, s * 0.78)
        ctx.closePath()
        ctx.fill()
    }

    function paintPause(ctx, s) {
        const w = s * 0.14
        const h = s * 0.52
        const y = s * 0.24
        ctx.fillRect(s * 0.30, y, w, h)
        ctx.fillRect(s * 0.56, y, w, h)
    }

    function paintSkip(ctx, s, forward) {
        ctx.save()
        if (!forward) {
            ctx.translate(s, 0)
            ctx.scale(-1, 1)
        }
        ctx.beginPath()
        ctx.moveTo(s * 0.22, s * 0.26)
        ctx.lineTo(s * 0.58, s * 0.5)
        ctx.lineTo(s * 0.22, s * 0.74)
        ctx.closePath()
        ctx.fill()
        ctx.fillRect(s * 0.62, s * 0.26, s * 0.12, s * 0.48)
        ctx.restore()
    }

    function paintSearch(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.42, s * 0.42, s * 0.26, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.60, s * 0.60)
        ctx.lineTo(s * 0.82, s * 0.82)
        ctx.stroke()
    }

    function paintX(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.28)
        ctx.lineTo(s * 0.72, s * 0.72)
        ctx.moveTo(s * 0.72, s * 0.28)
        ctx.lineTo(s * 0.28, s * 0.72)
        ctx.stroke()
    }

    function paintChevron(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.38, s * 0.22)
        ctx.lineTo(s * 0.66, s * 0.5)
        ctx.lineTo(s * 0.38, s * 0.78)
        ctx.stroke()
    }

    function paintBack(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.62, s * 0.22)
        ctx.lineTo(s * 0.34, s * 0.5)
        ctx.lineTo(s * 0.62, s * 0.78)
        ctx.stroke()
    }

    function paintPhone(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.36)
        ctx.quadraticCurveTo(s * 0.22, s * 0.28, s * 0.32, s * 0.24)
        ctx.lineTo(s * 0.44, s * 0.30)
        ctx.quadraticCurveTo(s * 0.48, s * 0.34, s * 0.44, s * 0.40)
        ctx.lineTo(s * 0.40, s * 0.48)
        ctx.quadraticCurveTo(s * 0.52, s * 0.62, s * 0.64, s * 0.56)
        ctx.lineTo(s * 0.70, s * 0.52)
        ctx.quadraticCurveTo(s * 0.76, s * 0.50, s * 0.78, s * 0.56)
        ctx.lineTo(s * 0.82, s * 0.68)
        ctx.quadraticCurveTo(s * 0.80, s * 0.78, s * 0.68, s * 0.76)
        ctx.quadraticCurveTo(s * 0.40, s * 0.72, s * 0.28, s * 0.36)
        ctx.closePath()
        ctx.fill()
    }

    function paintPhoneDown(ctx, s) {
        ctx.save()
        ctx.translate(s * 0.5, s * 0.5)
        ctx.rotate(Math.PI * 0.72)
        ctx.translate(-s * 0.5, -s * 0.5)
        paintPhone(ctx, s)
        ctx.restore()
    }

    function paintMic(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.38, s * 0.28)
        ctx.arcTo(s * 0.38, s * 0.18, s * 0.5, s * 0.18, s * 0.12)
        ctx.arcTo(s * 0.62, s * 0.18, s * 0.62, s * 0.28, s * 0.12)
        ctx.lineTo(s * 0.62, s * 0.48)
        ctx.arcTo(s * 0.62, s * 0.60, s * 0.5, s * 0.60, s * 0.12)
        ctx.arcTo(s * 0.38, s * 0.60, s * 0.38, s * 0.48, s * 0.12)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.30, s * 0.48)
        ctx.quadraticCurveTo(s * 0.30, s * 0.70, s * 0.5, s * 0.70)
        ctx.quadraticCurveTo(s * 0.70, s * 0.70, s * 0.70, s * 0.48)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.5, s * 0.70)
        ctx.lineTo(s * 0.5, s * 0.82)
        ctx.stroke()
    }

    function paintSpeaker(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.22, s * 0.40)
        ctx.lineTo(s * 0.38, s * 0.40)
        ctx.lineTo(s * 0.54, s * 0.26)
        ctx.lineTo(s * 0.54, s * 0.74)
        ctx.lineTo(s * 0.38, s * 0.60)
        ctx.lineTo(s * 0.22, s * 0.60)
        ctx.closePath()
        ctx.fill()
        ctx.beginPath()
        ctx.arc(s * 0.58, s * 0.5, s * 0.14, -0.8, 0.8)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.58, s * 0.5, s * 0.26, -0.8, 0.8)
        ctx.stroke()
    }

    function paintLocate(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.5, s * 0.18, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.5, s * 0.06, 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.5, s * 0.12)
        ctx.lineTo(s * 0.5, s * 0.26)
        ctx.moveTo(s * 0.5, s * 0.74)
        ctx.lineTo(s * 0.5, s * 0.88)
        ctx.moveTo(s * 0.12, s * 0.5)
        ctx.lineTo(s * 0.26, s * 0.5)
        ctx.moveTo(s * 0.74, s * 0.5)
        ctx.lineTo(s * 0.88, s * 0.5)
        ctx.stroke()
    }

    function paintPlus(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.5, s * 0.24)
        ctx.lineTo(s * 0.5, s * 0.76)
        ctx.moveTo(s * 0.24, s * 0.5)
        ctx.lineTo(s * 0.76, s * 0.5)
        ctx.stroke()
    }

    function paintMinus(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.24, s * 0.5)
        ctx.lineTo(s * 0.76, s * 0.5)
        ctx.stroke()
    }

    function paintFolder(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.16, s * 0.34)
        ctx.lineTo(s * 0.16, s * 0.28)
        ctx.quadraticCurveTo(s * 0.16, s * 0.22, s * 0.22, s * 0.22)
        ctx.lineTo(s * 0.42, s * 0.22)
        ctx.lineTo(s * 0.50, s * 0.30)
        ctx.lineTo(s * 0.78, s * 0.30)
        ctx.quadraticCurveTo(s * 0.84, s * 0.30, s * 0.84, s * 0.36)
        ctx.lineTo(s * 0.84, s * 0.74)
        ctx.quadraticCurveTo(s * 0.84, s * 0.80, s * 0.78, s * 0.80)
        ctx.lineTo(s * 0.22, s * 0.80)
        ctx.quadraticCurveTo(s * 0.16, s * 0.80, s * 0.16, s * 0.74)
        ctx.closePath()
        ctx.stroke()
    }

    function paintDoc(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.30, s * 0.18)
        ctx.lineTo(s * 0.58, s * 0.18)
        ctx.lineTo(s * 0.72, s * 0.32)
        ctx.lineTo(s * 0.72, s * 0.82)
        ctx.lineTo(s * 0.30, s * 0.82)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.58, s * 0.18)
        ctx.lineTo(s * 0.58, s * 0.32)
        ctx.lineTo(s * 0.72, s * 0.32)
        ctx.stroke()
    }

    function paintTrash(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.28, s * 0.32)
        ctx.lineTo(s * 0.72, s * 0.32)
        ctx.moveTo(s * 0.36, s * 0.32)
        ctx.lineTo(s * 0.40, s * 0.22)
        ctx.lineTo(s * 0.60, s * 0.22)
        ctx.lineTo(s * 0.64, s * 0.32)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.34, s * 0.36)
        ctx.lineTo(s * 0.38, s * 0.80)
        ctx.lineTo(s * 0.62, s * 0.80)
        ctx.lineTo(s * 0.66, s * 0.36)
        ctx.stroke()
    }

    function paintCopy(ctx, s) {
        ctx.strokeRect(s * 0.28, s * 0.34, s * 0.40, s * 0.48)
        ctx.beginPath()
        ctx.moveTo(s * 0.38, s * 0.34)
        ctx.lineTo(s * 0.38, s * 0.24)
        ctx.lineTo(s * 0.78, s * 0.24)
        ctx.lineTo(s * 0.78, s * 0.64)
        ctx.lineTo(s * 0.68, s * 0.64)
        ctx.stroke()
    }

    function paintRepeat(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.5, s * 0.28, 0.4, Math.PI * 1.6)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.66, s * 0.22)
        ctx.lineTo(s * 0.78, s * 0.34)
        ctx.lineTo(s * 0.62, s * 0.38)
        ctx.fill()
    }

    function paintRepeat1(ctx, s) {
        paintRepeat(ctx, s)
        ctx.font = "bold " + Math.round(s * 0.28) + "px sans-serif"
        ctx.textAlign = "center"
        ctx.textBaseline = "middle"
        ctx.fillText("1", s * 0.5, s * 0.52)
    }

    function paintShuffle(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.18, s * 0.32)
        ctx.lineTo(s * 0.38, s * 0.32)
        ctx.lineTo(s * 0.62, s * 0.68)
        ctx.lineTo(s * 0.78, s * 0.68)
        ctx.moveTo(s * 0.18, s * 0.68)
        ctx.lineTo(s * 0.38, s * 0.68)
        ctx.lineTo(s * 0.50, s * 0.54)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.70, s * 0.56)
        ctx.lineTo(s * 0.82, s * 0.68)
        ctx.lineTo(s * 0.70, s * 0.80)
        ctx.fill()
        ctx.beginPath()
        ctx.moveTo(s * 0.62, s * 0.32)
        ctx.lineTo(s * 0.78, s * 0.32)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.70, s * 0.20)
        ctx.lineTo(s * 0.82, s * 0.32)
        ctx.lineTo(s * 0.70, s * 0.44)
        ctx.fill()
    }

    function paintKeypad(ctx, s) {
        const r = s * 0.07
        for (let row = 0; row < 3; ++row) {
            for (let col = 0; col < 3; ++col) {
                ctx.beginPath()
                ctx.arc(s * (0.28 + col * 0.22), s * (0.28 + row * 0.22), r, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }

    function paintPerson(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.34, s * 0.16, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.92, s * 0.30, Math.PI * 1.15, Math.PI * 1.85)
        ctx.stroke()
    }

    function paintClock(ctx, s) {
        ctx.beginPath()
        ctx.arc(s * 0.5, s * 0.5, s * 0.32, 0, Math.PI * 2)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.5, s * 0.5)
        ctx.lineTo(s * 0.5, s * 0.32)
        ctx.moveTo(s * 0.5, s * 0.5)
        ctx.lineTo(s * 0.66, s * 0.56)
        ctx.stroke()
    }

    function paintDelete(ctx, s) {
        ctx.beginPath()
        ctx.moveTo(s * 0.22, s * 0.5)
        ctx.lineTo(s * 0.40, s * 0.28)
        ctx.lineTo(s * 0.82, s * 0.28)
        ctx.lineTo(s * 0.82, s * 0.72)
        ctx.lineTo(s * 0.40, s * 0.72)
        ctx.closePath()
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(s * 0.52, s * 0.40)
        ctx.lineTo(s * 0.70, s * 0.60)
        ctx.moveTo(s * 0.70, s * 0.40)
        ctx.lineTo(s * 0.52, s * 0.60)
        ctx.stroke()
    }
}
