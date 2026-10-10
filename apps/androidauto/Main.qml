import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root

    readonly property bool busy: AndroidAutoSession.running
        && !AndroidAutoSession.hasVideo
        && AndroidAutoSession.aoapState !== "SessionOpen"
        && AndroidAutoSession.aoapState !== "AccessoryReady"
        && AndroidAutoSession.aoapState !== "Failed"
        && AndroidAutoSession.aoapState !== "Idle"
    readonly property color brandGreen: "#3DDC84"

    Rectangle {
        anchors.fill: parent
        color: AndroidAutoSession.hasVideo ? "#0B0F0C" : "transparent"
    }

    Item {
        anchors.fill: parent
        visible: AndroidAutoSession.hasVideo

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#121A14" }
                GradientStop { position: 1.0; color: "#0B0F0C" }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 18
            width: Math.min(parent.width - 64, 520)

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 72
                height: 72
                radius: 18
                color: root.brandGreen
                Text {
                    anchors.centerIn: parent
                    text: "AA"
                    color: "#111111"
                    font.pixelSize: 28
                    font.bold: true
                }
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: AndroidAutoSession.demoMode
                      ? "演示投影 · 上板后接 TLS/视频通道"
                      : AndroidAutoSession.detail
                color: "#B3FFFFFF"
                font.pixelSize: 15
            }
            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 3
                spacing: 12
                Repeater {
                    model: ["地图", "电话", "音乐", "信息", "语音", "设置"]
                    Rectangle {
                        required property string modelData
                        width: 96
                        height: 72
                        radius: 14
                        color: "#1F2A22"
                        border.color: "#334038"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: "#E6FFFFFF"
                            font.pixelSize: 15
                        }
                    }
                }
            }
            IosPressable {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 140
                height: 44
                onClicked: AndroidAutoSession.stop()
                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: "#2A3330"
                    Text {
                        anchors.centerIn: parent
                        text: "退出投影"
                        color: "#FFFFFF"
                        font.pixelSize: 15
                        font.bold: true
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        spacing: 16
        visible: !AndroidAutoSession.hasVideo

            Text {
            width: parent.width
            text: "前排 USB 互联口 · AOAP"
            color: SystemState.secondary
            font.pixelSize: 14
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
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "传感 " + AndroidAutoSession.sensorSummary
                    color: SystemState.secondary
                    font.pixelSize: 11
                    opacity: 0.85
                    visible: AndroidAutoSession.running
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
                enabled: !AndroidAutoSession.running
                onClicked: {
                    if (CarPlaySession.running)
                        CarPlaySession.stop()
                    AndroidAutoSession.startDemo()
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: SystemState.fill
                    Text {
                        anchors.centerIn: parent
                        text: "演示投影"
                        color: SystemState.ink
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
