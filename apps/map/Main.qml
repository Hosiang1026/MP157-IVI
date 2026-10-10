import QtQuick
import Ivi.Services 1.0
import IviShell

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
                    anchors.bottomMargin: modelData.id === "vehicle" ? -4 : -2
                    width: modelData.id === "vehicle" ? 36 : 28
                    height: width
                    radius: width * 0.5
                    color: SystemState.dark ? "#66FFFFFF" : "#66000000"
                    z: -1
                }

                Rectangle {
                    visible: modelData.id === "vehicle"
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -9
                    width: 42
                    height: 42
                    radius: 21
                    color: "#40007AFF"
                    z: -1
                }

                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: modelData.id === "vehicle" ? 2 : 0
                    width: modelData.id === "vehicle" ? 18 : 14
                    height: width
                    rotation: modelData.id === "vehicle" && GpsSource.hasFix ? GpsSource.course : 0

                    Rectangle {
                        anchors.fill: parent
                        radius: width * 0.5
                        color: modelData.color
                        border.color: SystemState.dark ? "#E6FFFFFF" : "#F2FFFFFF"
                        border.width: 2.5
                    }

                    Canvas {
                        visible: modelData.id === "vehicle" && GpsSource.hasFix
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        anchors.bottomMargin: -2
                        width: 10
                        height: 8
                        onVisibleChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            if (!ctx)
                                return
                            ctx.reset()
                            ctx.fillStyle = modelData.color
                            ctx.beginPath()
                            ctx.moveTo(width * 0.5, 0)
                            ctx.lineTo(width, height)
                            ctx.lineTo(0, height)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }
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
                    styleColor: SystemState.dark ? "#CC000000" : "#E6FFFFFF"
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
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 16
            spacing: 8

            Row {
                width: parent.width
                height: 44
                spacing: 10

                Rectangle {
                    width: Math.max(180, parent.width - navBtn.width - parent.spacing)
                    height: 44
                    radius: 12
                    color: SystemState.card
                    opacity: 0.92
                    border.color: SystemState.separator
                    border.width: 1 / Screen.devicePixelRatio

                    IosSearchField {
                        id: searchField
                        anchors.fill: parent
                        anchors.margins: 4
                        placeholder: "搜索地点"
                        onSubmitted: MapTiles.searchPlaces(text)
                        onCleared: root.query = ""
                        onTextChanged: root.query = text
                    }
                }

                IosPressable {
                    id: navBtn
                    width: 112
                    height: 44
                    onClicked: {
                        if (NavSession.active) {
                            NavSession.stop()
                        } else {
                            NavSession.startTo(MapTiles.hasDestination ? MapTiles.destinationName : "目的地")
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: NavSession.active ? SystemState.danger : SystemState.tint
                        Text {
                            anchors.centerIn: parent
                            text: NavSession.active ? "结束导航" : "开始导航"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }
            }

            Rectangle {
                width: Math.min(320, parent.width)
                height: Math.min(180, resultCol.implicitHeight + 8)
                radius: 12
                color: SystemState.card
                opacity: 0.92
                border.color: SystemState.separator
                border.width: 1 / Screen.devicePixelRatio
                visible: MapTiles.searchResults.length > 0
                clip: true

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 4
                    contentHeight: resultCol.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height

                    Column {
                        id: resultCol
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: MapTiles.searchResults
                            delegate: Column {
                                required property var modelData
                                required property int index
                                width: resultCol.width

                                IosPressable {
                                    width: parent.width
                                    height: 40
                                    onClicked: {
                                        MapTiles.goToPlace(modelData.name, modelData.lat, modelData.lon)
                                        searchField.text = ""
                                        root.query = ""
                                    }

                                    Item {
                                        anchors.fill: parent

                                        Text {
                                            anchors.left: parent.left
                                            anchors.right: chevron.left
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 8
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                            text: modelData.label
                                            color: SystemState.ink
                                            font.pixelSize: 14
                                        }

                                        IosIcon {
                                            id: chevron
                                            anchors.right: parent.right
                                            anchors.rightMargin: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 14
                                            height: 14
                                            name: "chevron"
                                            ink: SystemState.secondary
                                        }
                                    }
                                }

                                Rectangle {
                                    width: parent.width - 12
                                    height: 1
                                    x: 12
                                    color: SystemState.separator
                                    visible: index < MapTiles.searchResults.length - 1
                                }
                            }
                        }
                    }
                }
            }
        }

        Column {
            id: zoomStack
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 16
            anchors.bottomMargin: 16
            spacing: 10

            IosPressable {
                width: 44
                height: 44
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    root.dragDx = 0
                    root.dragDy = 0
                    MapTiles.centerOnVehicle()
                    root.winX1 = -1
                    root.updateTileWindow()
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 22
                    color: SystemState.card
                    opacity: 0.92
                    border.color: SystemState.separator
                    border.width: 1 / Screen.devicePixelRatio
                    IosIcon {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        name: "locate"
                        ink: SystemState.tint
                    }
                }
            }

            Rectangle {
                width: 44
                height: 88
                radius: 22
                color: SystemState.card
                opacity: 0.92
                border.color: SystemState.separator
                border.width: 1 / Screen.devicePixelRatio
                clip: true

                Column {
                    anchors.fill: parent

                    IosPressable {
                        width: parent.width
                        height: 43
                        onClicked: {
                            root.commitDrag()
                            MapTiles.zoomIn()
                        }
                        IosIcon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            name: "plus"
                            ink: SystemState.ink
                        }
                    }

                    Rectangle {
                        width: parent.width - 12
                        height: 1
                        x: 6
                        color: SystemState.separator
                    }

                    IosPressable {
                        width: parent.width
                        height: 44
                        onClicked: {
                            root.commitDrag()
                            MapTiles.zoomOut()
                        }
                        IosIcon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            name: "minus"
                            ink: SystemState.ink
                        }
                    }
                }
            }
        }
    }
}
