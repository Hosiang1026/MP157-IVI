import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    function tireColor(value) {
        return value < 2.3 ? SystemState.warning : SystemState.ink
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: body.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar {
            id: vbar
            policy: ScrollBar.AsNeeded
            width: 3
            padding: 1
            contentItem: Rectangle {
                implicitWidth: 3
                radius: 1.5
                color: SystemState.secondary
                opacity: vbar.active || vbar.hovered ? 0.45 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
            }
            background: Item {}
        }

        Column {
            id: body
            x: 14
            y: 12
            width: flick.width - 28
            spacing: 10

        Rectangle {
            width: parent.width
            height: 40
            radius: 10
            color: SystemState.card
            border.color: SystemState.separator
            border.width: 1 / Screen.devicePixelRatio
            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 16
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "CAR"
                    color: SystemState.tint
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: VehicleState.locked ? "已上锁" : "未上锁"
                    color: VehicleState.locked ? SystemState.success : SystemState.warning
                    font.pixelSize: 13
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: VehicleState.lights ? "灯光开" : "灯光关"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "续航 " + VehicleState.rangeKm + " km"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: VehicleState.tireAlert ? "胎压告警" : "胎压正常"
                    color: VehicleState.tireAlert ? SystemState.warning : SystemState.secondary
                    font.pixelSize: 13
                }
            }
        }

        Row {
            width: parent.width
            height: 290
            spacing: 12

            Rectangle {
                width: 240
                height: parent.height
                radius: 14
                color: SystemState.card
                border.color: SystemState.separator
                border.width: 1 / Screen.devicePixelRatio

                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: NavSession.active ? -48 : -28
                    spacing: 4
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: VehicleState.speed
                        color: SystemState.ink
                        font.pixelSize: 72
                        font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "km/h"
                        color: SystemState.secondary
                        font.pixelSize: 14
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    anchors.bottomMargin: 58
                    height: 44
                    radius: 10
                    color: SystemState.fill
                    visible: NavSession.active
                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: NavSession.turn === "left" ? "←"
                                  : NavSession.turn === "right" ? "→"
                                  : NavSession.turn === "arrive" ? "●" : "↑"
                            color: SystemState.tint
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 36
                            spacing: 2
                            Text {
                                width: parent.width
                                text: (NavSession.distanceM >= 1000
                                       ? ((NavSession.distanceM / 1000).toFixed(1) + " km")
                                       : (NavSession.distanceM + " m"))
                                      + " · 限速 " + NavSession.speedLimit
                                color: SystemState.ink
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: NavSession.text
                                color: SystemState.secondary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 16
                    spacing: 8
                    Repeater {
                        model: ["P", "R", "N", "D"]
                        delegate: IosPressable {
                            required property string modelData
                            width: 46
                            height: 38
                            onClicked: VehicleState.setGear(modelData)
                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: VehicleState.gear === modelData ? SystemState.selected : SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: VehicleState.gear === modelData ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 16
                                    font.bold: VehicleState.gear === modelData
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width - 468
                height: parent.height
                radius: 14
                color: SystemState.card
                border.color: SystemState.separator
                border.width: 1 / Screen.devicePixelRatio

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    text: "胎压 bar"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                Item {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 44
                    anchors.bottomMargin: 18

                    Column {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: 64
                        spacing: 4
                        Text { text: "左前"; color: SystemState.secondary; font.pixelSize: 12 }
                        Text { text: VehicleState.tireFl.toFixed(1); color: tireColor(VehicleState.tireFl); font.pixelSize: 24; font.bold: true }
                    }
                    Column {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        width: 64
                        spacing: 4
                        Text { anchors.right: parent.right; text: "右前"; color: SystemState.secondary; font.pixelSize: 12 }
                        Text { anchors.right: parent.right; text: VehicleState.tireFr.toFixed(1); color: tireColor(VehicleState.tireFr); font.pixelSize: 24; font.bold: true }
                    }
                    Column {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        width: 64
                        spacing: 4
                        Text { text: VehicleState.tireRl.toFixed(1); color: tireColor(VehicleState.tireRl); font.pixelSize: 24; font.bold: true }
                        Text { text: "左后"; color: SystemState.secondary; font.pixelSize: 12 }
                    }
                    Column {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        width: 64
                        spacing: 4
                        Text { anchors.right: parent.right; text: VehicleState.tireRr.toFixed(1); color: tireColor(VehicleState.tireRr); font.pixelSize: 24; font.bold: true }
                        Text { anchors.right: parent.right; text: "右后"; color: SystemState.secondary; font.pixelSize: 12 }
                    }

                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 88
                        height: Math.min(148, parent.height - 16)
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            width: 52
                            height: 20
                            radius: 8
                            color: VehicleState.lights ? SystemState.tint : SystemState.fill
                        }
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 16
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            width: 72
                            radius: 22
                            color: SystemState.fill
                            border.color: VehicleState.locked ? SystemState.success : SystemState.warning
                            border.width: 3
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: -10
                                width: 40
                                height: 24
                                radius: 7
                                color: SystemState.card
                            }
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 12
                                width: 40
                                height: 18
                                radius: 5
                                color: SystemState.card
                            }
                        }
                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 30
                            radius: 3
                            color: SystemState.fill
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 30
                            radius: 3
                            color: SystemState.fill
                        }
                    }
                }
            }

            Rectangle {
                width: 204
                height: parent.height
                radius: 14
                color: SystemState.card
                border.color: SystemState.separator
                border.width: 1 / Screen.devicePixelRatio

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 0

                    Item {
                        width: parent.width
                        height: parent.height * 0.34
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Text { text: "油量"; color: SystemState.secondary; font.pixelSize: 12 }
                            Row {
                                spacing: 8
                                Text { text: VehicleState.fuel + "%"; color: SystemState.ink; font.pixelSize: 22; font.bold: true; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: VehicleState.rangeKm + " km"; color: SystemState.secondary; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                            }
                            Rectangle {
                                width: parent.width
                                height: 6
                                radius: 3
                                color: SystemState.fill
                                Rectangle {
                                    width: parent.width * VehicleState.fuel / 100
                                    height: parent.height
                                    radius: 3
                                    color: VehicleState.fuel < 20 ? SystemState.warning : SystemState.success
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: SystemState.separator }

                    Item {
                        width: parent.width
                        height: parent.height * 0.32
                        Row {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 20
                            Column {
                                spacing: 4
                                Text { text: "车外"; color: SystemState.secondary; font.pixelSize: 12 }
                                Text { text: VehicleState.outsideTemp + "°"; color: SystemState.ink; font.pixelSize: 22; font.bold: true }
                            }
                            Column {
                                spacing: 4
                                Text { text: "水温"; color: SystemState.secondary; font.pixelSize: 12 }
                                Text { text: VehicleState.coolant + "°"; color: SystemState.ink; font.pixelSize: 22; font.bold: true }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: SystemState.separator }

                    Item {
                        width: parent.width
                        height: parent.height * 0.34
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Text { text: "总里程"; color: SystemState.secondary; font.pixelSize: 12 }
                            Text { text: VehicleState.odometer + " km"; color: SystemState.ink; font.pixelSize: 18; font.bold: true }
                            Row {
                                spacing: 10
                                Text { text: "小计 " + VehicleState.trip + " km"; color: SystemState.secondary; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    text: "清零"
                                    color: SystemState.tint
                                    font.pixelSize: 13
                                    anchors.verticalCenter: parent.verticalCenter
                                    MouseArea { anchors.fill: parent; onClicked: VehicleState.resetTrip() }
                                }
                            }
                        }
                    }
                }
            }
        }

        Row {
            id: ctrlRow
            width: parent.width
            height: 60
            spacing: 8
            readonly property real btnW: (width - 40) / 6

            component TelltaleBtn: IosPressable {
                property string glyph: ""
                property bool active: false
                property bool warn: false
                readonly property color face: warn ? SystemState.warning
                                               : (active ? SystemState.tint : SystemState.fill)
                readonly property color glyphInk: (warn || active) ? "#FFFFFF" : SystemState.ink
                width: ctrlRow.btnW
                height: 60
                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    color: face
                    border.color: (warn || active) ? "transparent" : SystemState.separator
                    border.width: (warn || active) ? 0 : 0.5
                    Canvas {
                        id: icon
                        width: 36
                        height: 34
                        anchors.centerIn: parent
                        property string g: glyph
                        property bool on: active
                        property bool bad: warn
                        property color ink: glyphInk
                        onGChanged: requestPaint()
                        onOnChanged: requestPaint()
                        onBadChanged: requestPaint()
                        onInkChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.clearRect(0, 0, width, height)
                            ctx.imageSmoothingEnabled = true
                            ctx.strokeStyle = ink
                            ctx.fillStyle = ink
                            ctx.lineWidth = 2.6
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            if (g === "lock") {
                                ctx.beginPath()
                                ctx.moveTo(8, 16)
                                ctx.lineTo(28, 16)
                                ctx.lineTo(28, 30)
                                ctx.lineTo(8, 30)
                                ctx.closePath()
                                ctx.fill()
                                ctx.beginPath()
                                ctx.arc(18, 16, 8, Math.PI, 0)
                                ctx.stroke()
                                if (!on) {
                                    ctx.clearRect(20, 2, 14, 14)
                                    ctx.beginPath()
                                    ctx.arc(24, 12, 6.5, Math.PI * 0.85, Math.PI * 1.95)
                                    ctx.stroke()
                                }
                                ctx.fillStyle = face
                                ctx.beginPath()
                                ctx.arc(18, 23, 2.2, 0, Math.PI * 2)
                                ctx.fill()
                            } else if (g === "lights") {
                                ctx.beginPath()
                                ctx.moveTo(4, 11)
                                ctx.lineTo(14, 11)
                                ctx.lineTo(20, 5)
                                ctx.lineTo(20, 29)
                                ctx.lineTo(14, 23)
                                ctx.lineTo(4, 23)
                                ctx.closePath()
                                ctx.fill()
                                ctx.lineWidth = 2.8
                                ctx.beginPath()
                                ctx.moveTo(24, 9); ctx.lineTo(32, 5)
                                ctx.moveTo(24, 17); ctx.lineTo(32, 17)
                                ctx.moveTo(24, 25); ctx.lineTo(32, 29)
                                ctx.stroke()
                            } else if (g === "belt") {
                                ctx.lineWidth = 3.2
                                ctx.beginPath()
                                ctx.moveTo(7, 4); ctx.lineTo(29, 30)
                                ctx.moveTo(29, 4); ctx.lineTo(7, 30)
                                ctx.stroke()
                                ctx.fillRect(12, 13, 12, 7)
                            } else if (g === "brake") {
                                ctx.lineWidth = 3
                                ctx.beginPath()
                                ctx.arc(18, 17, 13, 0, Math.PI * 2)
                                ctx.stroke()
                                ctx.font = "bold 15px sans-serif"
                                ctx.textAlign = "center"
                                ctx.textBaseline = "middle"
                                ctx.fillText("P", 18, 18)
                            } else if (g === "door") {
                                ctx.lineWidth = 2.8
                                ctx.strokeRect(5, 3, 16, 28)
                                if (bad) {
                                    ctx.beginPath()
                                    ctx.moveTo(21, 5)
                                    ctx.lineTo(31, 9)
                                    ctx.lineTo(31, 27)
                                    ctx.lineTo(21, 23)
                                    ctx.closePath()
                                    ctx.fill()
                                } else {
                                    ctx.beginPath()
                                    ctx.arc(17, 17, 2, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                            } else if (g === "hood") {
                                ctx.beginPath()
                                ctx.moveTo(2, 20)
                                ctx.quadraticCurveTo(18, bad ? 3 : 10, 34, 20)
                                ctx.lineTo(30, 27)
                                ctx.lineTo(6, 27)
                                ctx.closePath()
                                ctx.fill()
                                ctx.lineWidth = 2.8
                                ctx.beginPath()
                                ctx.moveTo(10, 27); ctx.lineTo(10, 31)
                                ctx.moveTo(26, 27); ctx.lineTo(26, 31)
                                ctx.stroke()
                            }
                        }
                        Connections {
                            target: SystemState
                            function onDarkChanged() { icon.requestPaint() }
                        }
                    }
                }
            }

            TelltaleBtn {
                glyph: "lock"
                active: VehicleState.locked
                onClicked: VehicleState.toggleLocked()
            }
            TelltaleBtn {
                glyph: "lights"
                active: VehicleState.lights
                onClicked: VehicleState.toggleLights()
            }
            TelltaleBtn {
                glyph: "belt"
                active: VehicleState.seatbeltOn
                warn: !VehicleState.seatbeltOn
                onClicked: VehicleState.toggleSeatbelt()
            }
            TelltaleBtn {
                glyph: "brake"
                active: VehicleState.handbrakeOn
                warn: VehicleState.handbrakeOn
                onClicked: VehicleState.toggleHandbrake()
            }
            TelltaleBtn {
                glyph: "door"
                active: !VehicleState.doorAjar
                warn: VehicleState.doorAjar
                onClicked: VehicleState.toggleDoorAjar()
            }
            TelltaleBtn {
                glyph: "hood"
                active: !(VehicleState.hoodOpen || VehicleState.trunkOpen)
                warn: VehicleState.hoodOpen || VehicleState.trunkOpen
                onClicked: VehicleState.cycleHoodTrunk()
            }
        }

        Rectangle {
            width: parent.width
            height: 220
            radius: 14
            color: SystemState.card
            border.color: SystemState.separator
            border.width: 1 / Screen.devicePixelRatio

            Row {
                id: battHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 12

                Column {
                    spacing: 1
                    Text { text: "蓄电池"; color: SystemState.secondary; font.pixelSize: 12 }
                    Text {
                        text: VehicleState.batteryVoltage.toFixed(2) + " V"
                        color: VehicleState.batteryLow ? SystemState.danger : SystemState.ink
                        font.pixelSize: 20
                        font.bold: true
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: VehicleState.batteryHistory.length + "点 · " + VehicleState.batteryHistoryMin.toFixed(1) + "–" + VehicleState.batteryHistoryMax.toFixed(1) + " V"
                    color: SystemState.secondary
                    font.pixelSize: 12
                }

                Item { width: Math.max(12, battHeader.width - 470); height: 1 }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    Repeater {
                        model: [1, 7, 20]
                        delegate: IosPressable {
                            required property int modelData
                            width: 48
                            height: 28
                            onClicked: VehicleState.batteryRangeDays = modelData
                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: VehicleState.batteryRangeDays === modelData ? SystemState.selected : SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData + "天"
                                    color: VehicleState.batteryRangeDays === modelData ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 12
                                    font.bold: VehicleState.batteryRangeDays === modelData
                                }
                            }
                        }
                    }
                }
            }

            Canvas {
                id: battChart
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                height: parent.height - 64

                property var points: VehicleState.batteryHistory
                property var times: VehicleState.batteryHistoryTimes
                property real vmin: VehicleState.batteryHistoryMin
                property real vmax: VehicleState.batteryHistoryMax
                property int rangeDays: VehicleState.batteryRangeDays
                property color axis: SystemState.secondary
                property color grid: SystemState.separator
                property color plot: VehicleState.batteryLow ? SystemState.danger : SystemState.tint

                onPointsChanged: requestPaint()
                onTimesChanged: requestPaint()
                onVminChanged: requestPaint()
                onVmaxChanged: requestPaint()
                onRangeDaysChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPlotChanged: requestPaint()
                onAxisChanged: requestPaint()
                onGridChanged: requestPaint()

                function fmtLabel(ms, days) {
                    var d = new Date(ms)
                    var mo = d.getMonth() + 1
                    var day = d.getDate()
                    var hh = d.getHours()
                    var mm = d.getMinutes()
                    function z(n) { return n < 10 ? "0" + n : "" + n }
                    if (days <= 1)
                        return z(hh) + ":" + z(mm)
                    return mo + "-" + day
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var W = width
                    var H = height
                    if (W < 40 || H < 40)
                        return

                    var padL = 36
                    var padR = 10
                    var padT = 6
                    var padB = 22
                    var plotW = W - padL - padR
                    var plotH = H - padT - padB

                    var lo = Math.floor(Math.min(vmin, 11.5) * 2) / 2
                    var hi = Math.ceil(Math.max(vmax, 14.5) * 2) / 2
                    if (hi - lo < 1.0) {
                        lo -= 0.5
                        hi += 0.5
                    }

                    var n = points.length
                    var t0 = 0
                    var t1 = 1
                    if (n >= 1 && times.length === n) {
                        t0 = times[0]
                        t1 = times[n - 1]
                        if (t1 <= t0)
                            t1 = t0 + 1
                    }
                    var gapMs = 2 * 3600 * 1000

                    function yAt(v) {
                        return padT + plotH - ((v - lo) / (hi - lo)) * plotH
                    }
                    function xAtTime(ms) {
                        return padL + ((ms - t0) / (t1 - t0)) * plotW
                    }

                    ctx.strokeStyle = grid
                    ctx.fillStyle = axis
                    ctx.lineWidth = 1
                    ctx.font = "11px sans-serif"
                    ctx.textAlign = "right"
                    ctx.textBaseline = "middle"

                    for (var v = lo; v <= hi + 0.001; v += 0.5) {
                        var y = yAt(v)
                        ctx.beginPath()
                        ctx.moveTo(padL, y)
                        ctx.lineTo(padL + plotW, y)
                        ctx.stroke()
                        ctx.fillText(v.toFixed(1), padL - 6, y)
                    }

                    ctx.textAlign = "left"
                    ctx.textBaseline = "top"
                    ctx.fillText("V", 4, padT)

                    ctx.strokeStyle = axis
                    ctx.beginPath()
                    ctx.moveTo(padL, padT + plotH)
                    ctx.lineTo(padL + plotW, padT + plotH)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(padL, padT)
                    ctx.lineTo(padL, padT + plotH)
                    ctx.stroke()

                    ctx.fillStyle = axis
                    ctx.textBaseline = "top"
                    if (n >= 1 && times.length === n) {
                        var labelTs = n === 1 ? [t0] : [t0, (t0 + t1) / 2, t1]
                        for (var li = 0; li < labelTs.length; ++li) {
                            var lx = xAtTime(labelTs[li])
                            ctx.textAlign = li === 0 ? "left" : (li === labelTs.length - 1 ? "right" : "center")
                            ctx.fillText(fmtLabel(labelTs[li], rangeDays), lx, padT + plotH + 4)
                        }
                    }

                    if (n < 1 || times.length !== n)
                        return

                    ctx.strokeStyle = plot
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    var drawing = false
                    for (var i = 0; i < n; ++i) {
                        var x = xAtTime(times[i])
                        var py = yAt(points[i])
                        if (i > 0 && (times[i] - times[i - 1]) > gapMs) {
                            ctx.stroke()
                            ctx.beginPath()
                            drawing = false
                        }
                        if (!drawing) {
                            ctx.moveTo(x, py)
                            drawing = true
                        } else {
                            ctx.lineTo(x, py)
                        }
                    }
                    if (drawing)
                        ctx.stroke()

                    ctx.fillStyle = plot
                    ctx.beginPath()
                    ctx.arc(xAtTime(times[n - 1]), yAt(points[n - 1]), 3.5, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
        }
    }
}
