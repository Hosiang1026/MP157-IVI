import QtQuick
import QtQuick.Effects

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

    Item {
        id: mark
        anchors.centerIn: parent
        width: 280
        height: 200
        opacity: 0
        scale: 0.45

        Item {
            id: logoBlock
            anchors.horizontalCenter: parent.horizontalCenter
            y: 0
            width: 120
            height: 120

            Rectangle {
                id: glow
                anchors.centerIn: parent
                width: parent.width * 1.4
                height: width
                radius: width / 2
                opacity: 0.18
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#664CAF50" }
                    GradientStop { position: 0.5; color: "#184CAF50" }
                    GradientStop { position: 1.0; color: "#004CAF50" }
                }
            }

            Image {
                anchors.fill: parent
                source: "qrc:/qt/qml/IviShell/skoda-boot-icon.png"
                fillMode: Image.PreserveAspectFit
                mipmap: true
                asynchronous: false
                cache: true
                smooth: true
            }
        }

        Item {
            id: wordBlock
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: logoBlock.bottom
            anchors.topMargin: 18
            width: brandLabel.width
            height: brandLabel.height

            Text {
                id: brandGlowSrc
                anchors.centerIn: parent
                text: "SKODA"
                color: "#4CAF50"
                font.pixelSize: 28
                font.bold: true
                font.letterSpacing: 6
                visible: false
            }

            MultiEffect {
                id: brandBloom
                anchors.centerIn: parent
                width: brandLabel.width + 40
                height: brandLabel.height + 28
                source: brandGlowSrc
                blurEnabled: true
                blur: 1
                blurMax: 28
                brightness: 0.35
                colorization: 1
                colorizationColor: "#69F0AE"
                opacity: 0.85
            }

            Text {
                id: brandGlow
                anchors.centerIn: parent
                text: "SKODA"
                color: "#69F0AE"
                font.pixelSize: 28
                font.bold: true
                font.letterSpacing: 6
                opacity: 0.55
            }

            Text {
                id: brandLabel
                anchors.centerIn: parent
                text: "SKODA"
                color: "#E8F5E9"
                font.pixelSize: 28
                font.bold: true
                font.letterSpacing: 6
            }
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
            NumberAnimation {
                target: glow
                property: "opacity"
                from: 0
                to: 0.22
                duration: 1400
                easing.type: Easing.OutCubic
            }
        }
        SequentialAnimation {
            loops: 2
            ParallelAnimation {
                NumberAnimation {
                    target: glow
                    property: "opacity"
                    to: 0.12
                    duration: 700
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    target: brandBloom
                    property: "opacity"
                    to: 0.45
                    duration: 700
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    target: brandGlow
                    property: "opacity"
                    to: 0.3
                    duration: 700
                    easing.type: Easing.InOutSine
                }
            }
            ParallelAnimation {
                NumberAnimation {
                    target: glow
                    property: "opacity"
                    to: 0.22
                    duration: 700
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    target: brandBloom
                    property: "opacity"
                    to: 0.95
                    duration: 700
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    target: brandGlow
                    property: "opacity"
                    to: 0.65
                    duration: 700
                    easing.type: Easing.InOutSine
                }
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
