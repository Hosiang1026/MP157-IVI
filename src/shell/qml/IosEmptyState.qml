import QtQuick
import Ivi.Services 1.0

Column {
    id: root
    property string icon: "doc"
    property string title: ""
    property string subtitle: ""
    property color iconInk: SystemState.secondary
    spacing: 12

    IosIcon {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 44
        height: 44
        name: root.icon
        ink: root.iconInk
        opacity: 0.9
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.title
        color: SystemState.ink
        font.pixelSize: 20
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(320, root.width > 0 ? root.width : 320)
        text: root.subtitle
        color: SystemState.secondary
        font.pixelSize: 15
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        visible: root.subtitle.length > 0
    }
}
