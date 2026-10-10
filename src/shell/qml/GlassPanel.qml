import QtQuick
import QtQuick.Effects

Item {
    id: root
    property Item sourceItem
    property color fill: "#66FFFFFF"
    property color stroke: "#66FFFFFF"
    property real strokeWidth: 1
    property real radius: 24
    property real blurAmount: 0.85
    property int blurMax: 40
    property bool sheen: true

    readonly property real hairline: 1 / Screen.devicePixelRatio
    readonly property point srcOrigin: sourceItem ? mapToItem(sourceItem, 0, 0) : Qt.point(0, 0)

    function refresh() { grab.scheduleUpdate() }

    onWidthChanged: refresh()
    onHeightChanged: refresh()
    onXChanged: refresh()
    onYChanged: refresh()
    onVisibleChanged: if (visible) refresh()
    onSourceItemChanged: refresh()
    onSrcOriginChanged: refresh()

    ShaderEffectSource {
        id: grab
        anchors.fill: parent
        sourceItem: root.sourceItem
        sourceRect: Qt.rect(root.srcOrigin.x, root.srcOrigin.y, Math.max(1, root.width), Math.max(1, root.height))
        hideSource: false
        live: false
        visible: false
        textureMirroring: ShaderEffectSource.NoMirroring
    }

    MultiEffect {
        anchors.fill: parent
        source: grab
        blurEnabled: true
        blur: root.blurAmount
        blurMax: root.blurMax
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: mask
    }

    Item {
        id: mask
        width: root.width
        height: root.height
        layer.enabled: true
        visible: false
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "#FFFFFF"
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.fill
        border.color: root.stroke
        border.width: root.strokeWidth > 0 ? Math.max(root.hairline, root.strokeWidth) : 0
    }

    Rectangle {
        visible: root.sheen
        anchors.fill: parent
        anchors.margins: root.strokeWidth > 0 ? Math.max(root.hairline, root.strokeWidth) : 0
        radius: Math.max(0, root.radius - (root.strokeWidth > 0 ? Math.max(root.hairline, root.strokeWidth) : 0))
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#55FFFFFF" }
            GradientStop { position: 0.18; color: "#22FFFFFF" }
            GradientStop { position: 0.45; color: "#00FFFFFF" }
            GradientStop { position: 1.0; color: "#14000000" }
        }
    }

    Rectangle {
        visible: root.sheen && root.radius > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Math.max(root.hairline, root.strokeWidth)
        height: Math.min(parent.height * 0.42, 28)
        radius: Math.max(0, root.radius - Math.max(root.hairline, root.strokeWidth))
        opacity: 0.55
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#4DFFFFFF" }
            GradientStop { position: 1.0; color: "#00FFFFFF" }
        }
    }
}
