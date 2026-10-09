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
        border.width: root.strokeWidth
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.strokeWidth
        radius: Math.max(0, root.radius - root.strokeWidth)
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#3DFFFFFF" }
            GradientStop { position: 0.28; color: "#14FFFFFF" }
            GradientStop { position: 0.55; color: "#00FFFFFF" }
            GradientStop { position: 1.0; color: "#18000000" }
        }
    }
}
