import QtQuick

Item {
    id: root
    anchors.fill: parent
    z: 100
    visible: opacity > 0.001
    opacity: 1

    signal finished()

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Column {
        id: mark
        anchors.centerIn: parent
        spacing: 20
        opacity: 0
        scale: 0.45

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 120
            height: 120
            source: "qrc:/qt/qml/IviShell/skoda-boot-icon.png"
            fillMode: Image.PreserveAspectFit
            mipmap: true
            asynchronous: false
            cache: true
            smooth: true
        }

        Text {
            id: brandLabel
            anchors.horizontalCenter: parent.horizontalCenter
            text: "SKODA"
            color: "#69F0AE"
            font.pixelSize: 28
            font.bold: true
            font.letterSpacing: 6
            opacity: 0.75
        }
    }

    SequentialAnimation {
        running: true
        ParallelAnimation {
            NumberAnimation {
                target: mark
                property: "opacity"
                from: 0
                to: 1
                duration: 1200
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: mark
                property: "scale"
                from: 0.45
                to: 1
                duration: 900
                easing.type: Easing.OutBack
                easing.overshoot: 1.6
            }
        }
        SequentialAnimation {
            loops: 2
            NumberAnimation {
                target: brandLabel
                property: "opacity"
                to: 0.45
                duration: 700
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: brandLabel
                property: "opacity"
                to: 1
                duration: 700
                easing.type: Easing.InOutSine
            }
        }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 0
                duration: 800
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: mark
                property: "scale"
                to: 1.35
                duration: 700
                easing.type: Easing.InCubic
            }
        }
        ScriptAction {
            script: {
                root.visible = false
                root.finished()
            }
        }
    }
}
