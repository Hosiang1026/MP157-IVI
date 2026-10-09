import QtQuick
import Ivi.Services 1.0

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    AirPlayVideoItem {
        anchors.fill: parent
        visible: AirPlayMirror.hasVideo
        z: 10
        session: AirPlayMirror
    }

    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: !AirPlayMirror.hasVideo
        z: 5
        width: parent.width - 48

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "AirPlay 投屏"
            color: "#FFFFFF"
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: AirPlayMirror.status
            color: "#FFFFFF"
            font.pixelSize: 20
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: AirPlayMirror.detail
            color: "#A1A1A6"
            font.pixelSize: 14
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: AirPlayMirror.running
            text: "与 CarPlay 互斥，请先停其一"
            color: "#FF9F0A"
            font.pixelSize: 13
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            Rectangle {
                width: 120
                height: 40
                radius: 20
                color: AirPlayMirror.running ? "#3A3A3C" : "#007AFF"
                Text {
                    anchors.centerIn: parent
                    text: AirPlayMirror.running ? "运行中" : "开始接收"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: !AirPlayMirror.running
                    onClicked: AirPlayMirror.start()
                }
            }
            Rectangle {
                width: 100
                height: 40
                radius: 20
                color: "#FF453A"
                visible: AirPlayMirror.running
                Text {
                    anchors.centerIn: parent
                    text: "停止"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: AirPlayMirror.stop()
                }
            }
        }
    }

    Component.onDestruction: {
        if (AirPlayMirror.running)
            AirPlayMirror.stop()
    }
}
