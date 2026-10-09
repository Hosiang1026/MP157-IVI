import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    signal openApp(string entry)

    readonly property int columns: 5
    readonly property int rows: 3
    readonly property int perPage: columns * rows
    readonly property int gapX: 28
    readonly property int gapY: 20
    readonly property int dockBottomMargin: 14
    readonly property int slotW: flick.width > 0
            ? Math.floor((flick.width - (columns - 1) * gapX) / columns) : 100
    readonly property int carPlayIconPx: 120
    readonly property int dockGap: 8
    readonly property int dockCellW: homeIconSize
    readonly property int dockBarPadV: Math.max(14, Math.round(carPlayIconPx * 0.12))
    readonly property int dockBarPadH: Math.max(18, Math.round(carPlayIconPx * 0.2))
    readonly property int gridReserveHome: carPlayIconPx + 10 + dockBarPadV * 2 + dockBottomMargin + 22
    readonly property int gridReserveApps: dockBottomMargin + 22
    readonly property int gridReserve: root.currentPage === 0 ? gridReserveHome : gridReserveApps
    readonly property int slotH: flick.height > 0
            ? Math.floor((flick.height - gridReserve - (rows - 1) * gapY) / rows) : 100
    readonly property int slotHApps: flick.height > 0
            ? Math.floor((flick.height - gridReserveApps - (rows - 1) * gapY) / rows) : 100
    readonly property int gridW: columns * slotW + (columns - 1) * gapX
    readonly property int gridH: rows * slotH + (rows - 1) * gapY
    readonly property int gridTop: Math.max(6, Math.floor((flick.height - gridReserve - gridH) / 2))
    readonly property int homeIconSize: Math.min(carPlayIconPx, Math.min(Math.round(slotW * 0.76), Math.round(slotHApps * 0.76)))
    readonly property int homeLabelSize: Math.max(20, Math.round(homeIconSize * 0.18))
    readonly property int dockBarH: homeIconSize + 10 + dockBarPadV * 2
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

    function goTo(page) {
        page = Math.max(0, Math.min(root.pageCount - 1, page))
        root.currentPage = page
        flick.cancelFlick()
        snap.to = page * flick.width
        snap.start()
    }

    function updateDrag(sx, sy) {
        const p = mapFromItem(null, sx, sy)
        dragX = p.x
        dragY = p.y
        const d = dockBar.mapFromItem(null, sx, sy)
        dockHot = root.currentPage === 0
                && d.x >= -16 && d.y >= -24 && d.x <= dockBar.width + 16 && d.y <= dockBar.height + 24
        if (p.x < 48 && root.currentPage > 0)
            goTo(root.currentPage - 1)
    }

    function beginDrag(id, label, color, icon, fromDock, sx, sy) {
        dragging = true
        dragFromDock = fromDock
        dragId = id
        dragLabel = label
        dragColor = color
        dragIcon = icon
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
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 10
        anchors.bottomMargin: 14
        contentWidth: root.pageCount * width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

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
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: dockBar
        z: 2
        visible: root.currentPage === 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.dockBottomMargin
        width: Math.max(120, dockRow.implicitWidth + root.dockBarPadH * 2)
        height: root.dockBarH
        radius: Math.max(22, Math.round(root.dockBarH * 0.32))
        color: SystemState.dark ? "#B31C1C1E" : "#73F5F5F7"
        border.color: root.dockHot ? "#FFFFFF" : (SystemState.dark ? "#33FFFFFF" : "#66FFFFFF")
        border.width: root.dockHot ? 2 : 1
    }

    Row {
        id: dockRow
        z: 2
        visible: root.currentPage === 0
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
        anchors.bottomMargin: root.currentPage === 0
                ? root.dockBottomMargin + root.dockBarH + 8
                : root.dockBottomMargin + 8
        spacing: 7

        Repeater {
            model: root.pageCount
            delegate: Rectangle {
                width: root.currentPage === index ? 8 : 6
                height: 6
                radius: 3
                color: "#FFFFFF"
                opacity: root.currentPage === index ? 1 : 0.45
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
