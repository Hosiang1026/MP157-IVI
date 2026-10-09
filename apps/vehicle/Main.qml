import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    function tireColor(value) {
        return value < 2.3 ? "#FF9500" : SystemState.ink
    }

    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Rectangle {
        id: alertBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: VehicleState.alertCount > 0 ? 40 : 0
        visible: height > 0
        color: VehicleState.alertColor
        z: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: VehicleState.alertCount > 1 ? 4 : 10
            text: VehicleState.alertText
            color: "#FFFFFF"
            font.pixelSize: 14
            font.bold: true
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 5
            spacing: 5
            visible: VehicleState.alertCount > 1
            Repeater {
                model: VehicleState.alertCount
                delegate: Rectangle {
                    required property int index
                    width: 5
                    height: 5
                    radius: 2.5
                    color: index === VehicleState.alertIndex ? "#FFFFFF" : "#FFFFFF80"
                }
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: VehicleState.alertCount > 1
            text: (VehicleState.alertIndex + 1) + "/" + VehicleState.alertCount
            color: "#FFFFFF"
            font.pixelSize: 12
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.topMargin: VehicleState.alertCount > 0 ? 40 : 0
        contentWidth: width
        contentHeight: body.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 6
            padding: 0
            contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: SystemState.secondary
                opacity: 0.55
            }
        }

        Column {
            id: body
            x: 14
            y: 12
            width: flick.width - 28
            spacing: 10

        Row {
            width: parent.width
            height: 290
            spacing: 12

            Rectangle {
                width: 240
                height: parent.height
                radius: 16
                color: SystemState.card

                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -28
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

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 16
                    spacing: 8
                    Repeater {
                        model: ["P", "R", "N", "D"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 46
                            height: 38
                            radius: 10
                            color: VehicleState.gear === modelData ? "#007AFF" : SystemState.fill
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: VehicleState.gear === modelData ? "#FFFFFF" : SystemState.ink
                                font.pixelSize: 16
                                font.bold: true
                            }
                            MouseArea { anchors.fill: parent; onClicked: VehicleState.setGear(modelData) }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width - 468
                height: parent.height
                radius: 16
                color: SystemState.card

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
                            color: VehicleState.lights ? "#007AFF" : SystemState.fill
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
                            border.color: VehicleState.locked ? "#34C759" : "#FF9500"
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
                radius: 16
                color: SystemState.card

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
                                    color: VehicleState.fuel < 20 ? "#FF9500" : "#34C759"
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: SystemState.fill }

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

                    Rectangle { width: parent.width; height: 1; color: SystemState.fill }

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
                                    color: "#007AFF"
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
            height: 52
            spacing: 8

            readonly property real btnW: (width - 40) / 6

            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.locked ? "#34C759" : "#FF9500"
                Text { anchors.centerIn: parent; text: VehicleState.locked ? "已锁" : "未锁"; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleLocked() }
            }
            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.lights ? "#007AFF" : SystemState.fill
                Text { anchors.centerIn: parent; text: VehicleState.lights ? "大灯开" : "大灯关"; color: VehicleState.lights ? "#FFFFFF" : SystemState.ink; font.pixelSize: 14; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleLights() }
            }
            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.seatbeltOn ? "#34C759" : "#FF9500"
                Text { anchors.centerIn: parent; text: VehicleState.seatbeltOn ? "安全带" : "未系带"; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleSeatbelt() }
            }
            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.handbrakeOn ? "#FF9500" : SystemState.fill
                Text { anchors.centerIn: parent; text: VehicleState.handbrakeOn ? "手刹起" : "手刹"; color: VehicleState.handbrakeOn ? "#FFFFFF" : SystemState.ink; font.pixelSize: 14; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleHandbrake() }
            }
            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.doorAjar ? "#FF9500" : SystemState.fill
                Text { anchors.centerIn: parent; text: VehicleState.doorAjar ? "门未关" : "门已关"; color: VehicleState.doorAjar ? "#FFFFFF" : SystemState.ink; font.pixelSize: 14; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleDoorAjar() }
            }
            Rectangle {
                width: ctrlRow.btnW
                height: 52
                radius: 12
                color: VehicleState.hoodOpen || VehicleState.trunkOpen ? "#FF9500" : SystemState.fill
                Text {
                    anchors.centerIn: parent
                    text: VehicleState.hoodOpen ? "引擎盖" : (VehicleState.trunkOpen ? "后备箱" : "舱盖关")
                    color: VehicleState.hoodOpen || VehicleState.trunkOpen ? "#FFFFFF" : SystemState.ink
                    font.pixelSize: 14
                    font.bold: true
                }
                MouseArea { anchors.fill: parent; onClicked: VehicleState.cycleHoodTrunk() }
            }
        }

        Rectangle {
            width: parent.width
            height: 200
            radius: 16
            color: SystemState.card

            Row {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 12
                Column {
                    spacing: 1
                    Text { text: "蓄电池"; color: SystemState.secondary; font.pixelSize: 12 }
                    Text {
                        text: VehicleState.batteryVoltage.toFixed(2) + " V"
                        color: VehicleState.batteryLow ? "#FF3B30" : SystemState.ink
                        font.pixelSize: 20
                        font.bold: true
                    }
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    text: "长期监测 · " + VehicleState.batteryHistory.length + "点 · " + VehicleState.batteryHistoryMin.toFixed(1) + "–" + VehicleState.batteryHistoryMax.toFixed(1) + " V"
                    color: SystemState.secondary
                    font.pixelSize: 12
                }
            }

            Canvas {
                id: battChart
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                height: parent.height - 52

                property var points: VehicleState.batteryHistory
                property real vmin: VehicleState.batteryHistoryMin
                property real vmax: VehicleState.batteryHistoryMax
                property color line: VehicleState.batteryLow ? "#FF3B30" : "#007AFF"

                onPointsChanged: requestPaint()
                onVminChanged: requestPaint()
                onVmaxChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onLineChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var w = width
                    var h = height
                    if (w < 2 || h < 2)
                        return

                    var lo = Math.min(vmin, 11.5)
                    var hi = Math.max(vmax, 14.5)
                    if (hi - lo < 0.4) {
                        lo -= 0.2
                        hi += 0.2
                    }

                    function yAt(v) {
                        return h - ((v - lo) / (hi - lo)) * h
                    }

                    var n = points.length
                    if (n < 2)
                        return

                    ctx.strokeStyle = line
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    for (var i = 0; i < n; ++i) {
                        var x = i * (w - 1) / (n - 1)
                        var y = yAt(points[i])
                        if (i === 0)
                            ctx.moveTo(x, y)
                        else
                            ctx.lineTo(x, y)
                    }
                    ctx.stroke()

                    ctx.fillStyle = line
                    ctx.beginPath()
                    ctx.arc(w - 1, yAt(points[n - 1]), 3.5, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
        }
    }
}
