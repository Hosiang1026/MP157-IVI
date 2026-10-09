import QtQuick
import Ivi.Services 1.0

Item {
    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 28
        spacing: 18

        Text {
            text: "地图"
            color: SystemState.ink
            font.pixelSize: 28
            font.bold: true
        }

        Text {
            text: NavSession.active ? NavSession.text : "未开始导航"
            color: SystemState.ink
            font.pixelSize: 22
        }

        Rectangle {
            width: 148
            height: 44
            radius: 12
            color: NavSession.active ? "#FF3B30" : "#007AFF"
            Text {
                anchors.centerIn: parent
                text: NavSession.active ? "结束导航" : "开始导航"
                color: "#FFFFFF"
                font.pixelSize: 16
            }
            MouseArea {
                anchors.fill: parent
                onClicked: NavSession.active ? NavSession.stop() : NavSession.start()
            }
        }
    }
}
