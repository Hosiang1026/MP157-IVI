import QtQuick

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

    readonly property bool drawn: ["music", "phone", "vehicle", "settings", "store", "radio", "video", "map"].indexOf(appId) >= 0

    opacity: dimmed ? 0.35 : 1
    scale: area.pressed && !area.dragging ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.showLabel ? 4 : Math.max(0, (root.height - height) / 2)
        spacing: 5

        Item {
            width: root.iconSize
            height: root.iconSize
            anchors.horizontalCenter: parent.horizontalCenter

            Rectangle {
                id: tile
                width: root.iconSize
                height: root.iconSize
                radius: root.iconSize * 0.223
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter(root.tileColor, 1.18) }
                    GradientStop { position: 1.0; color: root.tileColor }
                }

                Image {
                    anchors.centerIn: parent
                    width: parent.width * 0.56
                    height: parent.height * 0.56
                    source: root.iconSource
                    visible: root.iconSource !== "" && !root.drawn
                    fillMode: Image.PreserveAspectFit
                }

                AppGlyph {
                    anchors.fill: parent
                    anchors.margins: parent.width * 0.2
                    appId: root.appId
                    visible: root.drawn && root.iconSource === ""
                }

                Text {
                    anchors.centerIn: parent
                    visible: !root.drawn && root.iconSource === ""
                    text: root.label.length > 0 ? root.label.charAt(0) : ""
                    color: "#FFFFFF"
                    font.pixelSize: root.iconSize * 0.38
                    font.bold: true
                }
            }
        }

        Text {
            width: Math.max(root.iconSize + 24, root.labelSize * 5)
            visible: root.showLabel
            height: root.showLabel ? implicitHeight : 0
            text: root.label
            color: "#FFFFFF"
            font.pixelSize: root.labelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            anchors.horizontalCenter: parent.horizontalCenter
            style: Text.Raised
            styleColor: "#99000000"
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
