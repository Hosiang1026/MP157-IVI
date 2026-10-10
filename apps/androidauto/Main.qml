import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root

    readonly property bool busy: AndroidAutoSession.running
        && AndroidAutoSession.aoapState !== "SessionOpen"
        && AndroidAutoSession.aoapState !== "AccessoryReady"
        && AndroidAutoSession.aoapState !== "Failed"
        && AndroidAutoSession.aoapState !== "Idle"
    readonly property color brandGreen: "#3DDC84"

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        spacing: 16

        Row {
            spacing: 12
            Rectangle {
                width: 4
                height: 48
                radius: 2
                color: root.brandGreen
                anchors.verticalCenter: parent.verticalCenter
            }
            Column {
                spacing: 6
                Text {
                    text: "Android Auto"
                    color: SystemState.ink
                    font.pixelSize: 30
                    font.bold: true
                }
                Text {
                    text: "有线 USB · AOAP"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            width: parent.width
            height: statusCol.height + 28
            radius: 16
            color: SystemState.card
            border.width: 0
            Column {
                id: statusCol
                x: 18
                y: 14
                width: parent.width - 36
                spacing: 8

                Row {
                    spacing: 10
                    width: parent.width
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: AndroidAutoSession.running ? SystemState.success
                               : AndroidAutoSession.aoapState === "Failed" ? SystemState.danger
                               : SystemState.secondary
                        visible: !root.busy
                    }
                    IosSpinner {
                        width: 18
                        height: 18
                        anchors.verticalCenter: parent.verticalCenter
                        ink: root.brandGreen
                        running: root.busy
                        visible: root.busy
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 28
                        text: AndroidAutoSession.status
                        color: AndroidAutoSession.running ? SystemState.success : SystemState.ink
                        font.pixelSize: 18
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: AndroidAutoSession.detail
                    color: SystemState.secondary
                    font.pixelSize: 13
                    visible: AndroidAutoSession.detail.length > 0
                }
                Text {
                    text: "AOAP " + AndroidAutoSession.aoapState
                          + (AndroidAutoSession.deviceLabel.length ? (" · " + AndroidAutoSession.deviceLabel) : "")
                    color: SystemState.secondary
                    font.pixelSize: 12
                }
                Text {
                    text: "IN " + AndroidAutoSession.bytesIn + " · OUT " + AndroidAutoSession.bytesOut
                    color: SystemState.secondary
                    font.pixelSize: 11
                    opacity: 0.7
                    visible: AndroidAutoSession.running
                }
            }
        }

        Row {
            spacing: 12
            IosPressable {
                width: 120
                height: 44
                enabled: !AndroidAutoSession.running
                onClicked: {
                    if (CarPlaySession.running)
                        CarPlaySession.stop()
                    AndroidAutoSession.start()
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: AndroidAutoSession.running ? SystemState.fill : root.brandGreen
                    opacity: AndroidAutoSession.running ? 0.45 : 1
                    Text {
                        anchors.centerIn: parent
                        text: "开始"
                        color: AndroidAutoSession.running ? SystemState.secondary : "#111111"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
            }
            IosPressable {
                width: 120
                height: 44
                enabled: AndroidAutoSession.running
                onClicked: AndroidAutoSession.stop()
                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: SystemState.fill
                    opacity: AndroidAutoSession.running ? 1 : 0.45
                    Text {
                        anchors.centerIn: parent
                        text: "停止"
                        color: SystemState.ink
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
            }
        }
    }
}
