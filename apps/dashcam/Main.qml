import QtQuick
import Ivi.Services 1.0
import IviShell

Item {
    id: root

    Component.onCompleted: CameraService.acquire("dashcam")
    Component.onDestruction: CameraService.release("dashcam")

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    CameraVideoItem {
        anchors.fill: parent
        session: CameraService
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 56
        color: "#CC1C1C1E"

        Row {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 16

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "行车记录仪"
                color: "#FFFFFF"
                font.pixelSize: 22
                font.bold: true
            }
            Rectangle {
                width: 8
                height: 8
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: CameraService.recording ? SystemState.danger
                       : (CameraService.demoMode ? SystemState.warning : SystemState.success)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: CameraService.recording ? "录像中"
                      : (CameraService.demoMode ? "演示" : (CameraService.status || "就绪"))
                color: "#E5E5EA"
                font.pixelSize: 14
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: CameraService.recording

            Rectangle {
                id: recDot
                width: 10
                height: 10
                radius: 5
                anchors.verticalCenter: parent.verticalCenter
                color: SystemState.danger
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: CameraService.recording && recDot.visible
                    NumberAnimation { from: 1; to: 0.25; duration: 700; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 0.25; to: 1; duration: 700; easing.type: Easing.InOutQuad }
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "REC  " + CameraService.recordSeconds + "s"
                color: "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 120
        color: "#CC1C1C1E"
    }

    Column {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        spacing: 8
        width: Math.min(280, parent.width * 0.4)
        z: 2

        Text {
            text: "摄像头"
            color: "#E5E5EA"
            font.pixelSize: 12
        }
        Repeater {
            model: CameraService.devices
            delegate: IosPressable {
                width: parent.width
                height: 36
                onClicked: CameraService.setDevice(modelData)
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: modelData === CameraService.deviceName ? SystemState.selected : "#33FFFFFF"
                    border.width: modelData === CameraService.deviceName ? 0.5 : 0
                    border.color: SystemState.tint
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: modelData === CameraService.deviceName ? SystemState.tint : "#FFFFFF"
                        font.pixelSize: 13
                        font.bold: modelData === CameraService.deviceName
                    }
                }
            }
        }
        Rectangle {
            width: parent.width
            height: 36
            radius: 8
            color: "#33FFFFFF"
            visible: CameraService.devices.length === 0
            Text {
                anchors.centerIn: parent
                text: CameraService.demoMode ? "演示模式" : "未发现设备"
                color: "#E5E5EA"
                font.pixelSize: 13
            }
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        height: 72
        spacing: 16
        z: 2

        IosPressable {
            width: 72
            height: 72
            onClicked: {
                if (CameraService.recording)
                    CameraService.stopRecording()
                else
                    CameraService.startRecording()
            }
            Rectangle {
                anchors.fill: parent
                radius: 36
                color: "#33FFFFFF"
                border.color: "#66FFFFFF"
                border.width: 2
                Rectangle {
                    anchors.centerIn: parent
                    width: CameraService.recording ? 22 : 28
                    height: CameraService.recording ? 22 : 28
                    radius: CameraService.recording ? 4 : 14
                    color: SystemState.danger
                    Behavior on width { NumberAnimation { duration: 140 } }
                    Behavior on height { NumberAnimation { duration: 140 } }
                    Behavior on radius { NumberAnimation { duration: 140 } }
                }
            }
        }
        IosPressable {
            width: 120
            height: 48
            anchors.verticalCenter: parent.verticalCenter
            onClicked: CameraService.refreshDevices()
            Rectangle {
                anchors.fill: parent
                radius: 24
                color: "#33FFFFFF"
                Text {
                    anchors.centerIn: parent
                    text: "刷新设备"
                    color: "#FFFFFF"
                    font.pixelSize: 16
                }
            }
        }
    }
}
