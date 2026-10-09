import QtQuick
import Ivi.Services 1.0

Item {
    function tireColor(value) {
        return value < 2.3 ? "#FF9500" : SystemState.ink
    }

    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Row {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 10
        spacing: 12

        Column {
            width: 320
            height: parent.height
            spacing: 12

            Rectangle {
                width: parent.width
                height: parent.height - 180
                radius: 16
                color: SystemState.card

                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: VehicleState.speed
                        color: SystemState.ink
                        font.pixelSize: 92
                        font.bold: true
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "km/h"
                        color: SystemState.secondary
                        font.pixelSize: 16
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    spacing: 8
                    Repeater {
                        model: ["P", "R", "N", "D"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 64
                            height: 44
                            radius: 12
                            color: VehicleState.gear === modelData ? "#007AFF" : SystemState.fill
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: VehicleState.gear === modelData ? "#FFFFFF" : SystemState.ink
                                font.pixelSize: 18
                                font.bold: true
                            }
                            MouseArea { anchors.fill: parent; onClicked: VehicleState.setGear(modelData) }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 168
                radius: 16
                color: SystemState.card
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Rectangle {
                        width: 276
                        height: 64
                        radius: 14
                        color: VehicleState.locked ? "#34C759" : "#FF9500"
                        Text {
                            anchors.centerIn: parent
                            text: VehicleState.locked ? "车门已锁" : "车门未锁"
                            color: "#FFFFFF"
                            font.pixelSize: 18
                            font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleLocked() }
                    }
                    Rectangle {
                        width: 276
                        height: 64
                        radius: 14
                        color: VehicleState.lights ? "#007AFF" : SystemState.fill
                        Text {
                            anchors.centerIn: parent
                            text: VehicleState.lights ? "大灯开" : "大灯关"
                            color: VehicleState.lights ? "#FFFFFF" : SystemState.ink
                            font.pixelSize: 18
                            font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: VehicleState.toggleLights() }
                    }
                }
            }
        }

        Column {
            width: parent.width - 332
            height: parent.height
            spacing: 12

            Rectangle {
                width: parent.width
                height: parent.height - 156
                radius: 16
                color: SystemState.card

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 16
                    text: "胎压 bar"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                Column {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 22
                    spacing: 2
                    Text { text: "左前"; color: SystemState.secondary; font.pixelSize: 13 }
                    Text { text: VehicleState.tireFl.toFixed(1); color: tireColor(VehicleState.tireFl); font.pixelSize: 28; font.bold: true }
                }
                Column {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 22
                    spacing: 2
                    Text { text: "右前"; color: SystemState.secondary; font.pixelSize: 13 }
                    Text { text: VehicleState.tireFr.toFixed(1); color: tireColor(VehicleState.tireFr); font.pixelSize: 28; font.bold: true }
                }
                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 22
                    spacing: 2
                    Text { text: VehicleState.tireRl.toFixed(1); color: tireColor(VehicleState.tireRl); font.pixelSize: 28; font.bold: true }
                    Text { text: "左后"; color: SystemState.secondary; font.pixelSize: 13 }
                }
                Column {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 22
                    spacing: 2
                    Text { text: VehicleState.tireRr.toFixed(1); color: tireColor(VehicleState.tireRr); font.pixelSize: 28; font.bold: true }
                    Text { text: "右后"; color: SystemState.secondary; font.pixelSize: 13 }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 108
                    height: 196
                    radius: 40
                    color: SystemState.fill
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 22
                        width: 72
                        height: 52
                        radius: 14
                        color: SystemState.card
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 28
                        width: 72
                        height: 36
                        radius: 12
                        color: SystemState.card
                    }
                }

            }

            Row {
                width: parent.width
                height: 144
                spacing: 12

                Rectangle {
                    width: (parent.width - 24) / 3
                    height: parent.height
                    radius: 16
                    color: SystemState.card
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: "油量"; color: SystemState.secondary; font.pixelSize: 13 }
                        Text { text: VehicleState.fuel + "%"; color: SystemState.ink; font.pixelSize: 28; font.bold: true }
                        Rectangle {
                            width: parent.width
                            height: 8
                            radius: 4
                            color: SystemState.fill
                            Rectangle {
                                width: parent.width * VehicleState.fuel / 100
                                height: parent.height
                                radius: 4
                                color: VehicleState.fuel < 20 ? "#FF9500" : "#34C759"
                            }
                        }
                        Text { text: "续航 " + VehicleState.rangeKm + " km"; color: SystemState.secondary; font.pixelSize: 13 }
                    }
                }

                Rectangle {
                    width: (parent.width - 24) / 3
                    height: parent.height
                    radius: 16
                    color: SystemState.card
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 14
                        Column {
                            spacing: 2
                            Text { text: "车外"; color: SystemState.secondary; font.pixelSize: 13 }
                            Text { text: VehicleState.outsideTemp + "°"; color: SystemState.ink; font.pixelSize: 26; font.bold: true }
                        }
                        Column {
                            spacing: 2
                            Text { text: "水温"; color: SystemState.secondary; font.pixelSize: 13 }
                            Text { text: VehicleState.coolant + "°"; color: SystemState.ink; font.pixelSize: 26; font.bold: true }
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 24) / 3
                    height: parent.height
                    radius: 16
                    color: SystemState.card
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: "总里程"; color: SystemState.secondary; font.pixelSize: 13 }
                        Text { text: VehicleState.odometer + " km"; color: SystemState.ink; font.pixelSize: 22; font.bold: true }
                        Text { text: "小计 " + VehicleState.trip + " km"; color: SystemState.secondary; font.pixelSize: 14 }
                        Text {
                            text: "清零"
                            color: "#007AFF"
                            font.pixelSize: 15
                            MouseArea { anchors.fill: parent; onClicked: VehicleState.resetTrip() }
                        }
                    }
                }
            }
        }
    }
}
