import QtQuick

Item {
    id: root
    property real pressScale: 0.96
    property real pressOpacity: 0.88
    property bool enabled: true
    readonly property bool pressed: tap.pressed
    signal clicked()
    signal pressAndHold()

    default property alias contentData: body.data

    Item {
        id: body
        anchors.fill: parent
        transformOrigin: Item.Center
        scale: root.pressed && root.enabled ? root.pressScale : 1
        opacity: root.pressed && root.enabled ? root.pressOpacity : 1
        Behavior on scale {
            NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
        }
    }

    TapHandler {
        id: tap
        enabled: root.enabled
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
        onLongPressed: root.pressAndHold()
    }
}
