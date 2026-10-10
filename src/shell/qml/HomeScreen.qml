import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property Item glassSource
    signal openApp(string entry)

    function refreshGlass() {
        if (dockBar.visible)
            dockBar.refresh()
    }

    onXChanged: refreshGlass()
    onYChanged: refreshGlass()
    onWidthChanged: refreshGlass()
    onHeightChanged: refreshGlass()
    onVisibleChanged: if (visible) refreshGlass()

    readonly property int columns: 5
    readonly property int rows: 3
    readonly property int perPage: columns * rows
    readonly property int gapX: 24
    readonly property int gapY: 18
    readonly property int dockBottomMargin: 18
    readonly property int dockMax: 5
    readonly property int slotW: flick.width > 0
            ? Math.floor((flick.width - (columns - 1) * gapX) / columns) : 100
    readonly property int carPlayIconPx: 118
    readonly property int dockGap: 18
    readonly property int dockCellW: homeIconSize
    readonly property int dockBarPadV: Math.max(12, Math.round(carPlayIconPx * 0.1))
    readonly property int dockBarPadH: Math.max(22, Math.round(carPlayIconPx * 0.22))
    readonly property int gridReserveHome: carPlayIconPx + 10 + dockBarPadV * 2 + dockBottomMargin + 26
    readonly property int gridReserveApps: dockBottomMargin + 22
    readonly property int gridReserve: root.currentPage === 0 ? gridReserveHome : gridReserveApps
    readonly property int slotH: flick.height > 0
            ? Math.floor((flick.height - gridReserve - (rows - 1) * gapY) / rows) : 100
    readonly property int slotHApps: flick.height > 0
            ? Math.floor((flick.height - gridReserveApps - (rows - 1) * gapY) / rows) : 100
    readonly property int gridW: columns * slotW + (columns - 1) * gapX
    readonly property int gridH: rows * slotH + (rows - 1) * gapY
    readonly property int gridTop: Math.max(6, Math.floor((flick.height - gridReserve - gridH) / 2))
    readonly property int homeIconSize: Math.min(carPlayIconPx, Math.min(Math.round(slotW * 0.78), Math.round(slotHApps * 0.78)))
    readonly property int homeLabelSize: Math.max(18, Math.round(homeIconSize * 0.155))
    readonly property int dockBarH: homeIconSize + 8 + dockBarPadV * 2
    readonly property int appPages: Math.max(1, Math.ceil(AppCatalog.installed.count / perPage))
    readonly property int pageCount: 1 + appPages
    property int currentPage: 0
    readonly property real pagePos: flick.width > 0 ? flick.contentX / flick.width : 0
    property bool dragging: false
    property bool dragFromDock: false
    property bool dockHot: false
    property string dragId: ""
    property string dragLabel: ""
    property string dragColor: "#3A3A3C"
    property string dragIcon: ""
    property real dragX: 0
    property real dragY: 0
    property real lastEdgeFlip: 0

    function goTo(page) {
        page = Math.max(0, Math.min(root.pageCount - 1, page))
        root.currentPage = page
        flick.cancelFlick()
        snap.to = page * flick.width
        snap.start()
    }

    function dockContains(id) {
        for (let i = 0; i < dockIcons.count; ++i) {
            const item = dockIcons.itemAt(i)
            if (item && item.appId === id)
                return true
        }
        return false
    }

    function updateDrag(sx, sy) {
        const p = mapFromItem(null, sx, sy)
        dragX = p.x
        dragY = p.y
        const d = dockBar.mapFromItem(null, sx, sy)
        const hit = d.x >= -16 && d.y >= -24 && d.x <= dockBar.width + 16 && d.y <= dockBar.height + 24
        const canDock = root.dragFromDock || root.dockContains(root.dragId) || dockIcons.count < root.dockMax
        dockHot = hit && canDock
        const now = Date.now()
        if (now - root.lastEdgeFlip >= 450) {
            if (p.x < 48 && root.currentPage > 0) {
                root.lastEdgeFlip = now
                goTo(root.currentPage - 1)
            } else if (p.x > root.width - 48 && root.currentPage < root.pageCount - 1) {
                root.lastEdgeFlip = now
                goTo(root.currentPage + 1)
            }
        }
    }

    function beginDrag(id, label, color, icon, fromDock, sx, sy) {
        dragging = true
        dragFromDock = fromDock
        dragId = id
        dragLabel = label
        dragColor = color
        dragIcon = icon
        lastEdgeFlip = 0
        updateDrag(sx, sy)
    }

    function indexAt(repeater, sx, sy) {
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (!item)
                continue
            const p = item.mapFromItem(null, sx, sy)
            if (p.x >= 0 && p.y >= 0 && p.x <= item.width && p.y <= item.height)
                return i
        }
        return -1
    }

    function finishDrag(sx, sy) {
        updateDrag(sx, sy)
        const dockIndex = indexAt(dockIcons, sx, sy)
        const homeIndex = indexAt(homeIcons, sx, sy)
        if (dragFromDock) {
            if (!dockHot)
                AppCatalog.removeFromDock(dragId)
            else if (dockIndex >= 0)
                AppCatalog.moveDock(dragId, dockIndex)
        } else if (dockHot) {
            if (dockIndex >= 0)
                AppCatalog.moveDock(dragId, dockIndex)
            else
                AppCatalog.addToDock(dragId)
        } else if (homeIndex >= 0) {
            AppCatalog.moveHome(dragId, homeIndex)
        }
        dragging = false
        dragId = ""
        dockHot = false
    }

    Flickable {
        id: flick
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.topMargin: 8
        anchors.bottomMargin: 12
        contentWidth: root.pageCount * width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: !root.dragging

        Repeater {
            id: homeIcons
            model: AppCatalog.installed
            delegate: AppIcon {
                width: root.slotW
                height: root.slotH
                x: {
                    const page = 1 + Math.floor(index / root.perPage)
                    const i = index % root.perPage
                    const col = i % root.columns
                    const ox = (flick.width - root.gridW) / 2
                    return page * flick.width + ox + col * (root.slotW + root.gapX)
                }
                y: {
                    const i = index % root.perPage
                    const row = Math.floor(i / root.columns)
                    return root.gridTop + row * (root.slotH + root.gapY)
                }
                label: model.name
                appId: model.appId
                tileColor: model.color
                iconSource: model.icon
                iconSize: root.homeIconSize
                labelSize: root.homeLabelSize
                dimmed: root.dragging && !root.dragFromDock && root.dragId === model.appId
                onClicked: root.openApp(model.entry)
                onHoldDrag: function (sx, sy) { root.beginDrag(model.appId, model.name, model.color, model.icon, false, sx, sy) }
                onHoldMove: function (sx, sy) { if (root.dragging) root.updateDrag(sx, sy) }
                onHoldDrop: function (sx, sy) { root.finishDrag(sx, sy) }
            }
        }

        onDragEnded: {
            const origin = root.currentPage * width
            const delta = contentX - origin
            let page = root.currentPage
            if (delta > 48 || horizontalVelocity > 250)
                page += 1
            else if (delta < -48 || horizontalVelocity < -250)
                page -= 1
            root.goTo(page)
        }

        NumberAnimation {
            id: snap
            target: flick
            property: "contentX"
            duration: 320
            easing.type: Easing.OutQuint
        }
    }

    GlassPanel {
        id: dockBar
        z: 2
        visible: root.currentPage === 0 || root.dragging
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.dockBottomMargin
        width: Math.max(120, dockRow.implicitWidth + root.dockBarPadH * 2)
        height: root.dockBarH
        radius: Math.max(16, Math.round(root.dockBarH * 0.18))
        sourceItem: root.glassSource
        fill: SystemState.dark ? "#A61C1C1E" : "#8CF2F2F7"
        stroke: root.dockHot ? "#E6FFFFFF" : (SystemState.dark ? "#59FFFFFF" : "#99FFFFFF")
        strokeWidth: root.dockHot ? 1.25 : (1 / Screen.devicePixelRatio)
        blurAmount: 1.0
        blurMax: 56
        opacity: root.currentPage === 0 ? 1 : 0.94
    }

    Row {
        id: dockRow
        z: 2
        visible: dockBar.visible
        anchors.centerIn: dockBar
        spacing: root.dockGap

        Repeater {
            id: dockIcons
            model: AppCatalog.dock
            delegate: AppIcon {
                width: root.dockCellW
                height: root.homeIconSize + 10
                showLabel: false
                iconSize: root.homeIconSize
                labelSize: root.homeLabelSize
                label: model.name
                appId: model.appId
                tileColor: model.color
                iconSource: model.icon
                dimmed: root.dragging && root.dragFromDock && root.dragId === model.appId
                onClicked: root.openApp(model.entry)
                onHoldDrag: function (sx, sy) { root.beginDrag(model.appId, model.name, model.color, model.icon, true, sx, sy) }
                onHoldMove: function (sx, sy) { if (root.dragging) root.updateDrag(sx, sy) }
                onHoldDrop: function (sx, sy) { root.finishDrag(sx, sy) }
            }
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: (root.currentPage === 0 || root.dragging)
                ? root.dockBottomMargin + root.dockBarH + 10
                : root.dockBottomMargin + 10
        spacing: 8
        z: 3

        Repeater {
            model: root.pageCount
            delegate: Item {
                width: root.currentPage === index ? 18 : 10
                height: 10
                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                Rectangle {
                    anchors.centerIn: parent
                    width: root.currentPage === index ? 18 : 7
                    height: 7
                    radius: 3.5
                    color: "#FFFFFF"
                    opacity: root.currentPage === index ? 1.0 : 0.28
                    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 220 } }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.goTo(index)
                }
            }
        }
    }

    AppIcon {
        z: 30
        visible: root.dragging
        enabled: false
        x: root.dragX - width / 2
        y: root.dragY - height / 2
        width: root.dragFromDock ? root.dockCellW : root.slotW
        height: root.dragFromDock ? (root.homeIconSize + 10) : root.slotH
        showLabel: false
        iconSize: root.homeIconSize
        labelSize: root.homeLabelSize
        label: root.dragLabel
        appId: root.dragId
        tileColor: root.dragColor
        iconSource: root.dragIcon
    }

    Connections {
        target: AppCatalog.installed
        function onModelReset() {
            if (root.currentPage >= root.pageCount)
                root.goTo(0)
        }
    }
}
