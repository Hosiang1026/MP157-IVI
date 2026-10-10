import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property string label
    property string appId
    property string tileColor: "#3A3A3C"
    property string iconSource
    property bool showLabel: true
    property int iconSize: 120
    property int labelSize: 22
    property bool dimmed: false
    signal clicked()
    signal holdDrag(real sceneX, real sceneY)
    signal holdMove(real sceneX, real sceneY)
    signal holdDrop(real sceneX, real sceneY)

    readonly property bool drawn: ["music", "phone", "vehicle", "settings", "store", "radio", "video", "map", "carplay", "androidauto", "weather", "airplay", "dlna", "dashcam", "files"].indexOf(appId) >= 0
    readonly property real squircle: 0.2237

    opacity: dimmed ? 0.32 : 1
    scale: area.pressed && !area.dragging ? 0.86 : 1
    Behavior on scale {
        NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 2.2 }
    }
    Behavior on opacity { NumberAnimation { duration: 160 } }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.showLabel ? 2 : Math.max(0, (root.height - height) / 2)
        spacing: Math.max(6, Math.round(root.iconSize * 0.07))

        Item {
            id: iconBox
            width: root.iconSize
            height: root.iconSize
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(4, Math.round(root.iconSize * 0.05))
                width: root.iconSize
                height: root.iconSize
                radius: root.iconSize * root.squircle
                color: "#000000"
                opacity: 0.22
                scale: 0.97
                z: -1
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(2, Math.round(root.iconSize * 0.025))
                width: root.iconSize
                height: root.iconSize
                radius: root.iconSize * root.squircle
                color: "#000000"
                opacity: 0.12
                scale: 0.99
                z: -1
            }

            Rectangle {
                id: tile
                width: root.iconSize
                height: root.iconSize
                radius: root.iconSize * root.squircle
                anchors.fill: parent
                antialiasing: true
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(root.tileColor, 1.28) }
                    GradientStop { position: 0.42; color: Qt.lighter(root.tileColor, 1.06) }
                    GradientStop { position: 0.78; color: root.tileColor }
                    GradientStop { position: 1.0; color: Qt.darker(root.tileColor, 1.12) }
                }

                Item {
                    id: glyphHost
                    anchors.fill: parent
                    anchors.margins: parent.width * 0.10

                    Image {
                        anchors.centerIn: parent
                        width: parent.width
                        height: parent.height
                        source: root.iconSource
                        visible: root.iconSource !== "" && !root.drawn
                        fillMode: Image.PreserveAspectFit
                        mipmap: true
                        smooth: true
                    }

                    AppGlyph {
                        anchors.fill: parent
                        appId: root.appId
                        visible: root.drawn
                        opacity: root.iconSource === "" ? 1 : 0
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !root.drawn && root.iconSource === ""
                        text: root.label.length > 0 ? root.label.charAt(0) : ""
                        color: "#FFFFFF"
                        font.pixelSize: root.iconSize * 0.46
                        font.weight: Font.DemiBold
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#55FFFFFF" }
                        GradientStop { position: 0.18; color: "#22FFFFFF" }
                        GradientStop { position: 0.45; color: "#00FFFFFF" }
                        GradientStop { position: 0.82; color: "#08000000" }
                        GradientStop { position: 1.0; color: "#22000000" }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.width: 1
                    border.color: "#40FFFFFF"
                    opacity: 0.9
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    color: "transparent"
                    border.width: 1
                    border.color: "#18000000"
                }
            }
        }

        Text {
            width: Math.max(root.iconSize + 28, root.labelSize * 5.2)
            visible: root.showLabel
            height: root.showLabel ? implicitHeight : 0
            text: root.label
            color: WallpaperStore.darkBackdrop ? "#F5FFFFFF" : "#E6000000"
            font.pixelSize: root.labelSize
            font.weight: Font.Medium
            font.letterSpacing: -0.15
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            anchors.horizontalCenter: parent.horizontalCenter
            style: Text.Raised
            styleColor: WallpaperStore.darkBackdrop ? "#66000000" : "#66FFFFFF"
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        preventStealing: dragging
        property bool dragging: false
        property bool movedIcon: false
        property real sx: 0
        property real sy: 0

        function scenePoint(x, y) {
            return mapToItem(null, x, y)
        }

        Timer {
            id: hold
            interval: 280
            onTriggered: {
                area.dragging = true
                area.movedIcon = true
                const p = area.scenePoint(area.sx, area.sy)
                root.holdDrag(p.x, p.y)
            }
        }

        onPressed: function (mouse) {
            sx = mouse.x
            sy = mouse.y
            dragging = false
            movedIcon = false
            hold.start()
        }
        onPositionChanged: function (mouse) {
            if (!dragging) {
                if (Math.abs(mouse.x - sx) > 10 || Math.abs(mouse.y - sy) > 10)
                    hold.stop()
                return
            }
            const p = scenePoint(mouse.x, mouse.y)
            root.holdMove(p.x, p.y)
        }
        onReleased: function (mouse) {
            hold.stop()
            if (!dragging)
                return
            dragging = false
            const p = scenePoint(mouse.x, mouse.y)
            root.holdDrop(p.x, p.y)
        }
        onCanceled: {
            hold.stop()
            dragging = false
        }
        onClicked: {
            if (!movedIcon)
                root.clicked()
        }
    }
}
