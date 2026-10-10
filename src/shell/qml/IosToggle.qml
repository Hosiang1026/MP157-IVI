import QtQuick
import Ivi.Services 1.0

IosPressable {
    id: root
    property bool checked: false
    property color onColor: SystemState.success
    property color offColor: SystemState.fill
    signal toggled(bool checked)

    width: 56
    height: 34
    pressScale: 0.96
    pressOpacity: 0.95

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.onColor : root.offColor
        border.width: root.checked ? 0 : (1 / Screen.devicePixelRatio)
        border.color: SystemState.separator
        Behavior on color { ColorAnimation { duration: 180 } }

        Rectangle {
            id: knob
            width: 28
            height: 28
            radius: 14
            color: "#FFFFFF"
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? parent.width - width - 3 : 3
            border.width: 1 / Screen.devicePixelRatio
            border.color: "#14000000"
            Behavior on x {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutBack
                    easing.overshoot: 1.4
                }
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 13
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#33FFFFFF" }
                    GradientStop { position: 1.0; color: "#00000000" }
                }
            }
        }
    }

    onClicked: root.toggled(!root.checked)
}
