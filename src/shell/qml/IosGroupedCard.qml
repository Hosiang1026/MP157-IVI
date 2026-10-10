import QtQuick
import Ivi.Services 1.0

Rectangle {
    id: root
    property real contentRadius: 14
    default property alias contentData: body.data

    radius: contentRadius
    color: SystemState.card
    border.width: 1 / Screen.devicePixelRatio
    border.color: SystemState.separator
    clip: true

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Math.min(parent.height * 0.35, 22)
        radius: root.contentRadius
        opacity: SystemState.dark ? 0.18 : 0.35
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#FFFFFF" }
            GradientStop { position: 1.0; color: "#00FFFFFF" }
        }
    }

    Item {
        id: body
        anchors.fill: parent
    }
}
