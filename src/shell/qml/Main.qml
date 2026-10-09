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

    Item {
        id: desktopBg
        anchors.fill: parent

        Image {
            id: wall
            anchors.fill: parent
            source: WallpaperStore.current
            fillMode: Image.PreserveAspectCrop
            cache: true
            asynchronous: true
            mipmap: true
            onStatusChanged: if (status === Image.Ready && home) home.refreshGlass()
        }

        WeatherFx {
            anchors.fill: parent
            wallpaper: wall
            pagePos: home.pagePos
            active: !stage.opened || Weather.preview.length > 0
        }
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
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
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
        visible: height > 0.5
        clip: true
        z: 5

        Behavior on height {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }

        GlassPanel {
            anchors.fill: parent
            radius: 0
            sourceItem: wall
            visible: !stage.opened
            fill: SystemState.dark ? "#661B2030" : "#80F2F2F7"
            stroke: SystemState.dark ? "#33A8B4C8" : "#22FFFFFF"
            strokeWidth: 0.8
            blurAmount: 0.88
            blurMax: 40
        }

        Rectangle {
            anchors.fill: parent
            visible: stage.opened
            color: SystemState.dark ? "#CC1B2030" : "#CCF2F2F7"
            border.color: SystemState.dark ? "#33A8B4C8" : "#2E000000"
            border.width: 1
        }

        DragHandler {
            id: trayPull
            target: null
            xAxis.enabled: false
            yAxis.enabled: true
            dragThreshold: 8
            property bool moved: false
            property real lastDy: 0
            property real lastVy: 0

            onActiveChanged: {
                if (active) {
                    moved = false
                    lastDy = 0
                    lastVy = 0
                } else if (moved) {
                    if (lastDy < -16 || lastVy < -380)
                        statusBar.drawerOpen = false
                }
            }
            onTranslationChanged: {
                if (!active)
                    return
                lastDy = translation.y
                lastVy = centroid.velocity.y
                if (lastDy < -6 && Math.abs(lastDy) > Math.abs(translation.x))
                    moved = true
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 14
            z: 1
            Repeater {
                model: stage.running
                delegate: Rectangle {
                    width: 48
                    height: 48
                    radius: 11
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.lighter(modelData.color, 1.22) }
                        GradientStop { position: 1.0; color: modelData.color }
                    }
                    border.width: 1
                    border.color: "#33FFFFFF"
                    AppGlyph {
                        anchors.fill: parent
                        anchors.margins: 9
                        appId: modelData.appId
                    }
                    MouseArea {
                        property bool held: false
                        anchors.fill: parent
                        onPressed: held = false
                        onPressAndHold: {
                            held = true
                            stage.dismiss(modelData.appId)
                        }
                        onClicked: {
                            if (!held)
                                stage.open(modelData.entry)
                        }
                        onReleased: held = false
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
            glassSource: wall
            onOpenApp: function(entry) { stage.open(entry) }
        }

        AppStage {
            id: stage
            anchors.fill: parent
        }

        MouseArea {
            anchors.fill: parent
            enabled: statusBar.drawerOpen
            z: 100
            onPressed: function (mouse) {
                statusBar.drawerOpen = false
                mouse.accepted = false
            }
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
        visible: opacity > 0.001 && !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
        z: 1
    }

    Item {
        z: 80
        anchors.fill: parent
        visible: CameraService.reverseActive

        CameraVideoItem {
            anchors.fill: parent
            session: CameraService
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            width: badge.implicitWidth + 24
            height: 36
            radius: 8
            color: "#CCFF453A"
            Text {
                id: badge
                anchors.centerIn: parent
                text: "倒车影像"
                color: "#FFFFFF"
                font.pixelSize: 16
                font.bold: true
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            text: CameraService.demoMode ? "演示画面 · 车辆页切 R 档可测" : CameraService.status
            color: "#CCFFFFFF"
            font.pixelSize: 14
        }
    }

    Item {
        z: 6
        visible: !(stage.currentId === "carplay" && CarPlaySession.hasVideo) && !CameraService.reverseActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        width: 200
        height: 36

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            width: 134
            height: 5
            radius: 2.5
            color: !stage.opened ? "#E6FFFFFF" : (SystemState.dark ? "#66FFFFFF" : "#4D000000")
            opacity: 0.9
        }

        MouseArea {
            anchors.fill: parent
            enabled: stage.opened
            property real pressY: 0
            onPressed: function (m) { pressY = m.y }
            onReleased: function (m) {
                if (pressY - m.y > 16)
                    stage.close()
            }
            onClicked: stage.close()
        }
    }
}
