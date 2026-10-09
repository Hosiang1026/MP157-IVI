import QtQuick
import Ivi.Services 1.0

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
        color: "#99000000"

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
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: CameraService.status
                color: "#A1A1A6"
                font.pixelSize: 14
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            visible: CameraService.recording
            text: "● REC  " + CameraService.recordSeconds + "s"
            color: "#FF453A"
            font.pixelSize: 16
            font.bold: true
        }
    }

    Column {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        spacing: 8
        width: Math.min(280, parent.width * 0.4)

        Text {
            text: "摄像头"
            color: "#A1A1A6"
            font.pixelSize: 12
        }
        Repeater {
            model: CameraService.devices
            delegate: Rectangle {
                width: parent.width
                height: 36
                radius: 8
                color: modelData === CameraService.deviceName ? "#0A84FF" : "#663A3A3C"
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 16
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    color: "#FFFFFF"
                    font.pixelSize: 13
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: CameraService.setDevice(modelData)
                }
            }
        }
        Rectangle {
            width: parent.width
            height: 36
            radius: 8
            color: "#663A3A3C"
            visible: CameraService.devices.length === 0
            Text {
                anchors.centerIn: parent
                text: CameraService.demoMode ? "演示模式" : "未发现设备"
                color: "#A1A1A6"
                font.pixelSize: 13
            }
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        spacing: 16

        Rectangle {
            width: 148
            height: 48
            radius: 24
            color: CameraService.recording ? "#3A3A3C" : "#FF453A"
            Text {
                anchors.centerIn: parent
                text: CameraService.recording ? "停止录像" : "开始录像"
                color: "#FFFFFF"
                font.pixelSize: 17
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (CameraService.recording)
                        CameraService.stopRecording()
                    else
                        CameraService.startRecording()
                }
            }
        }
        Rectangle {
            width: 120
            height: 48
            radius: 24
            color: "#3A3A3C"
            Text {
                anchors.centerIn: parent
                text: "刷新设备"
                color: "#FFFFFF"
                font.pixelSize: 16
            }
            MouseArea {
                anchors.fill: parent
                onClicked: CameraService.refreshDevices()
            }
        }
    }
}
