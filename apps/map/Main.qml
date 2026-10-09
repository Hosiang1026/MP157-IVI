import QtQuick
import Ivi.Services 1.0

Item {
    id: root

    property string query: ""
    property real dragDx: 0
    property real dragDy: 0
    property var tileModel: []
    property int winX0: 0
    property int winX1: -1
    property int winY0: 0
    property int winY1: -1

    readonly property int tileSize: MapTiles.tileSize
    readonly property int zoom: MapTiles.zoom
    readonly property int tilesX: MapTiles.tileCount(zoom)
    readonly property var centerWorld: MapTiles.latLonToWorld(MapTiles.centerLat, MapTiles.centerLon, zoom)
    readonly property real viewCx: centerWorld.x - dragDx
    readonly property real viewCy: centerWorld.y - dragDy

    function wrapTileX(x) {
        const n = tilesX
        let v = x % n
        if (v < 0)
            v += n
        return v
    }

    function updateTileWindow() {
        if (width <= 0 || height <= 0 || tileSize <= 0)
            return
        const x0 = Math.floor((viewCx - width * 0.5) / tileSize) - 1
        const x1 = Math.ceil((viewCx + width * 0.5) / tileSize) + 1
        const y0 = Math.floor((viewCy - height * 0.5) / tileSize) - 1
        const y1 = Math.ceil((viewCy + height * 0.5) / tileSize) + 1
        if (x0 === winX0 && x1 === winX1 && y0 === winY0 && y1 === winY1)
            return
        winX0 = x0
        winX1 = x1
        winY0 = y0
        winY1 = y1
        const list = []
        for (let x = x0; x <= x1; ++x) {
            for (let y = y0; y <= y1; ++y) {
                if (y < 0 || y >= tilesX)
                    continue
                list.push({ tx: x, ty: y })
            }
        }
        tileModel = list
    }

    function markerX(lat, lon) {
        const p = MapTiles.latLonToWorld(lat, lon, zoom)
        return width * 0.5 + (p.x - viewCx)
    }

    function markerY(lat, lon) {
        const p = MapTiles.latLonToWorld(lat, lon, zoom)
        return height * 0.5 + (p.y - viewCy)
    }

    function commitDrag() {
        if (dragDx === 0 && dragDy === 0)
            return
        const dx = dragDx
        const dy = dragDy
        dragDx = 0
        dragDy = 0
        MapTiles.panByPixels(dx, dy)
        updateTileWindow()
    }

    onViewCxChanged: updateTileWindow()
    onViewCyChanged: updateTileWindow()
    onZoomChanged: {
        winX1 = -1
        updateTileWindow()
    }
    onWidthChanged: {
        winX1 = -1
        updateTileWindow()
    }
    onHeightChanged: {
        winX1 = -1
        updateTileWindow()
    }
    Component.onCompleted: updateTileWindow()

    Rectangle {
        anchors.fill: parent
        color: "#E8ECE6"
    }

    Item {
        id: mapLayer
        anchors.fill: parent
        clip: true
        z: 0

        Repeater {
            model: root.tileModel
            delegate: Image {
                required property var modelData
                readonly property int tileX: root.wrapTileX(modelData.tx)
                readonly property int tileY: modelData.ty
                width: root.tileSize
                height: root.tileSize
                x: Math.floor(root.width * 0.5 + modelData.tx * root.tileSize - root.viewCx)
                y: Math.floor(root.height * 0.5 + modelData.ty * root.tileSize - root.viewCy)
                asynchronous: true
                cache: true
                fillMode: Image.Pad
                source: MapTiles.tileUrl(root.zoom, tileX, tileY)
                visible: status === Image.Ready
            }
        }

        Repeater {
            model: MapTiles.markers
            delegate: Item {
                required property var modelData
                width: 1
                height: 1
                x: root.markerX(modelData.lat, modelData.lon)
                y: root.markerY(modelData.lat, modelData.lon)
                visible: x > -40 && x < root.width + 40 && y > -40 && y < root.height + 40

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: modelData.id === "vehicle" ? 2 : 0
                    width: modelData.id === "vehicle" ? 18 : 14
                    height: width
                    radius: width * 0.5
                    color: modelData.color
                    border.color: "#FFFFFF"
                    border.width: 2
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.top
                    anchors.bottomMargin: 6
                    text: modelData.name
                    color: SystemState.ink
                    font.pixelSize: 12
                    font.bold: true
                    style: Text.Outline
                    styleColor: "#FFFFFF"
                }
            }
        }

        DragHandler {
            id: dragHandler
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (!active)
                    root.commitDrag()
            }
            onTranslationChanged: {
                if (!active)
                    return
                root.dragDx = translation.x
                root.dragDy = translation.y
            }
        }

        PinchHandler {
            target: null
            onActiveChanged: {
                if (!active)
                    root.commitDrag()
            }
            onScaleChanged: function() {
                if (!active)
                    return
                if (activeScale > 1.12) {
                    root.commitDrag()
                    MapTiles.zoomIn()
                } else if (activeScale < 0.88) {
                    root.commitDrag()
                    MapTiles.zoomOut()
                }
            }
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(event) {
                root.commitDrag()
                if (event.angleDelta.y > 0)
                    MapTiles.zoomIn()
                else if (event.angleDelta.y < 0)
                    MapTiles.zoomOut()
            }
        }
    }

    Timer {
        interval: 1000
        running: NavSession.active && MapTiles.hasDestination
        repeat: true
        onTriggered: MapTiles.stepTowardDestination(0.1)
    }

    Item {
        id: uiLayer
        anchors.fill: parent
        z: 10

        Column {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 8

            Rectangle {
                width: Math.min(420, root.width - 32)
                height: 44
                radius: 12
                color: SystemState.card
                border.color: SystemState.fill
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    Rectangle {
                        width: parent.width - 78
                        height: parent.height
                        radius: 8
                        color: SystemState.fill
                        TextInput {
                            id: searchInput
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            verticalAlignment: Text.AlignVCenter
                            color: SystemState.ink
                            font.pixelSize: 14
                            clip: true
                            text: root.query
                            onTextChanged: root.query = text
                            Keys.onReturnPressed: MapTiles.searchPlaces(root.query)
                            Keys.onEnterPressed: MapTiles.searchPlaces(root.query)
                        }
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            text: "搜索地点"
                            color: SystemState.secondary
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            visible: searchInput.text.length === 0 && !searchInput.activeFocus
                        }
                    }

                    Rectangle {
                        id: searchBtn
                        width: 72
                        height: parent.height
                        radius: 8
                        color: "#007AFF"
                        Text {
                            anchors.centerIn: parent
                            text: MapTiles.searching ? "…" : "搜索"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                        }
                        TapHandler {
                            onTapped: MapTiles.searchPlaces(root.query)
                        }
                    }
                }
            }

            Rectangle {
                width: Math.min(420, root.width - 32)
                height: Math.min(180, resultCol.implicitHeight + 12)
                radius: 12
                color: SystemState.card
                border.color: SystemState.fill
                border.width: 1
                visible: MapTiles.searchResults.length > 0
                clip: true

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 6
                    contentHeight: resultCol.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height

                    Column {
                        id: resultCol
                        width: parent.width
                        spacing: 4
                        Repeater {
                            model: MapTiles.searchResults
                            delegate: Rectangle {
                                required property var modelData
                                width: resultCol.width
                                height: 36
                                radius: 8
                                color: SystemState.fill
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                    text: modelData.label
                                    color: SystemState.ink
                                    font.pixelSize: 14
                                }
                                TapHandler {
                                    onTapped: {
                                        MapTiles.goToPlace(modelData.name, modelData.lat, modelData.lon)
                                        root.query = ""
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: Math.min(360, root.width - 32)
                height: 56
                radius: 12
                color: Qt.rgba(1, 1, 1, SystemState.dark ? 0.12 : 0.92)
                border.color: SystemState.fill
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text {
                        text: NavSession.active
                              ? NavSession.text
                              : (MapTiles.hasDestination ? ("前往 " + MapTiles.destinationName) : "离线瓦片地图")
                        color: SystemState.ink
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: NavSession.active
                              ? ("限速 " + NavSession.speedLimit + " · ETA " + NavSession.etaMin + " 分")
                              : (MapTiles.hasTiles
                                 ? (MapTiles.tileSource + "  z" + MapTiles.zoom + "  " + MapTiles.centerLat.toFixed(4) + ", " + MapTiles.centerLon.toFixed(4))
                                 : "未找到底图（先运行 scripts/start-tileserver.ps1）")
                        color: SystemState.secondary
                        font.pixelSize: 12
                    }
                }
            }

            Row {
                spacing: 8

                Rectangle {
                    width: 108
                    height: 40
                    radius: 10
                    color: NavSession.active ? "#FF3B30" : "#007AFF"
                    Text {
                        anchors.centerIn: parent
                        text: NavSession.active ? "结束导航" : "开始导航"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                    }
                    TapHandler {
                        onTapped: {
                            if (NavSession.active) {
                                NavSession.stop()
                            } else {
                                NavSession.startTo(MapTiles.hasDestination ? MapTiles.destinationName : "目的地")
                            }
                        }
                    }
                }

                Rectangle {
                    width: 88
                    height: 40
                    radius: 10
                    color: SystemState.card
                    border.color: SystemState.fill
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "定位"
                        color: SystemState.ink
                        font.pixelSize: 14
                    }
                    TapHandler {
                        onTapped: {
                            root.dragDx = 0
                            root.dragDy = 0
                            MapTiles.centerOnVehicle()
                            root.winX1 = -1
                            root.updateTileWindow()
                        }
                    }
                }
            }
        }

        Column {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 16
            spacing: 8

            Rectangle {
                width: 44
                height: 44
                radius: 12
                color: SystemState.card
                border.color: SystemState.fill
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: SystemState.ink
                    font.pixelSize: 24
                }
                TapHandler {
                    onTapped: {
                        root.commitDrag()
                        MapTiles.zoomIn()
                    }
                }
            }

            Rectangle {
                width: 44
                height: 44
                radius: 12
                color: SystemState.card
                border.color: SystemState.fill
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "−"
                    color: SystemState.ink
                    font.pixelSize: 24
                }
                TapHandler {
                    onTapped: {
                        root.commitDrag()
                        MapTiles.zoomOut()
                    }
                }
            }
        }
    }
}
