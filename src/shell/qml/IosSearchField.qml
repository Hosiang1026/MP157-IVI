import QtQuick
import Ivi.Services 1.0

Rectangle {
    id: root
    property alias text: input.text
    property alias input: input
    property string placeholder: "搜索"
    signal submitted()
    signal cleared()

    height: 44
    radius: 12
    color: SystemState.fill
    border.width: 1 / Screen.devicePixelRatio
    border.color: SystemState.separator

    IosIcon {
        id: mag
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 18
        height: 18
        name: "search"
        ink: SystemState.secondary
    }

    TextInput {
        id: input
        anchors.left: mag.right
        anchors.leftMargin: 10
        anchors.right: clearBtn.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        color: SystemState.ink
        font.pixelSize: 17
        clip: true
        selectByMouse: true
        onAccepted: root.submitted()
    }

    Text {
        anchors.fill: input
        text: root.placeholder
        color: SystemState.secondary
        font.pixelSize: 17
        verticalAlignment: Text.AlignVCenter
        visible: input.text.length === 0 && !input.activeFocus
    }

    IosPressable {
        id: clearBtn
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        visible: input.text.length > 0
        pressScale: 0.9

        Rectangle {
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: 10
            color: SystemState.secondary
            opacity: 0.55
        }
        IosIcon {
            anchors.centerIn: parent
            width: 11
            height: 11
            name: "xmark"
            ink: SystemState.dark ? "#1C1C1E" : "#FFFFFF"
        }
        onClicked: {
            input.text = ""
            root.cleared()
            input.forceActiveFocus()
        }
    }
}
