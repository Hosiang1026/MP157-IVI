import QtQuick
import QtQuick.Window
import Ivi.Services 1.0

Window {
    id: window
    width: 1024
    height: 600
    visible: true
    title: "IVI"
    color: "#000000"

    Image {
        id: wall
        anchors.fill: parent
        source: WallpaperStore.current
        fillMode: Image.PreserveAspectCrop
        cache: true
        asynchronous: true
        mipmap: true
    }

    WeatherFx {
        anchors.fill: parent
        wallpaper: wall
        pagePos: home.pagePos
        active: !stage.opened || Weather.preview.length > 0
    }

    Component.onCompleted: {
        if (Qt.platform.os === "linux")
            visibility = Window.FullScreen
    }

    StatusBar {
        id: statusBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBar.barH
        z: 5
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo)
        darkContent: stage.opened
        running: stage.background
        runningAll: stage.running
        onOpenApp: function (entry) { stage.open(entry) }
        onDismissApp: function (id) { stage.dismiss(id) }
    }

    Item {
        id: appTray
        anchors.top: statusBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: statusBar.drawerOpen && stage.running.length > 0 ? statusBar.trayH : 0
        visible: height > 0
        clip: true
        z: 5

        Item {
            anchors.fill: parent
            clip: true
            visible: !stage.opened
            Image {
                width: window.width
                height: window.height
                x: 0
                y: -statusBar.height
                source: WallpaperStore.current
                fillMode: Image.PreserveAspectCrop
                cache: true
                asynchronous: true
                mipmap: true
            }
        }

        Rectangle {
            anchors.fill: parent
            color: stage.opened
                    ? (SystemState.dark ? "#CC1C1C1E" : "#CCF2F2F7")
                    : (SystemState.dark ? "#70101010" : "#78F2F2F7")
            border.color: SystemState.dark ? "#28FFFFFF" : "#2E000000"
            border.width: 1
        }

        DragHandler {
            target: null
            yAxis.minimum: -1000
            yAxis.maximum: 0
            onActiveChanged: {
                if (!active && centroid.scenePressPosition.y - centroid.scenePosition.y > 12)
                    statusBar.drawerOpen = false
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 14
            Repeater {
                model: stage.running
                delegate: Rectangle {
                    width: 48
                    height: 48
                    radius: 11
                    color: modelData.color
                    AppGlyph {
                        anchors.fill: parent
                        anchors.margins: 8
                        appId: modelData.appId
                    }
                    MouseArea {
                        property bool held: false
                        anchors.fill: parent
                        onPressAndHold: {
                            held = true
                            stage.dismiss(modelData.appId)
                        }
                        onClicked: {
                            if (!held)
                                stage.open(modelData.entry)
                        }
                    }
                }
            }
        }
    }

    Item {
        anchors.top: (stage.currentId === "carplay" && CarPlaySession.hasVideo) ? parent.top : appTray.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        HomeScreen {
            id: home
            anchors.fill: parent
            visible: !stage.opened
            onOpenApp: function(entry) { stage.open(entry) }
        }

        AppStage {
            id: stage
            anchors.fill: parent
        }

        Connections {
            target: CarPlaySession
            function onHostUiRequested() {
                stage.close()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: (1 - SystemState.brightness) * 0.55
        enabled: false
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo)
        z: 1
    }

    Item {
        z: 6
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        width: 160
        height: 22

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: 128
            height: 5
            radius: 2.5
            color: !stage.opened ? "#F5FFFFFF" : (SystemState.dark ? "#59FFFFFF" : "#59000000")
        }

        MouseArea {
            anchors.fill: parent
            enabled: stage.opened
            onClicked: stage.close()
        }
    }
}
