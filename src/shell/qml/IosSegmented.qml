import QtQuick
import Ivi.Services 1.0

Rectangle {
    id: root
    property var labels: []
    property int currentIndex: 0
    signal activated(int index)

    height: 44
    radius: 12
    color: SystemState.fill
    clip: true

    Rectangle {
        id: thumb
        x: 3 + currentIndex * ((root.width - 6) / Math.max(1, root.labels.length))
        y: 3
        width: (root.width - 6) / Math.max(1, root.labels.length)
        height: root.height - 6
        radius: 10
        color: SystemState.elevated
        border.width: SystemState.dark ? 0 : (1 / Screen.devicePixelRatio)
        border.color: "#1A000000"
        Behavior on x {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            radius: parent.radius
            z: -1
            visible: !SystemState.dark
            color: "#18000000"
        }
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 3
        spacing: 0
        z: 1

        Repeater {
            model: root.labels
            delegate: IosPressable {
                required property int index
                required property var modelData
                width: row.width / Math.max(1, root.labels.length)
                height: row.height
                pressScale: 0.98
                pressOpacity: 0.92

                Text {
                    anchors.centerIn: parent
                    text: typeof modelData === "string" ? modelData : (modelData.label || "")
                    color: SystemState.ink
                    font.pixelSize: 15
                    font.bold: root.currentIndex === index
                    opacity: root.currentIndex === index ? 1 : 0.72
                }

                onClicked: {
                    root.currentIndex = index
                    root.activated(index)
                }
            }
        }
    }
}
