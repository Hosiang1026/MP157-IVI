import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root
    property string page: "library"
    property int index: 0
    property bool controlsVisible: true
    property var clips: []

    function mmss(value) {
        const s = Math.max(0, Math.floor(value))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function openPlayer(i) {
        if (i < 0 || i >= clips.length)
            return
        index = i
        page = "player"
        controlsVisible = true
        hideTimer.restart()
        player.clipName = clips[i].title
        player.seek(0)
        player.playing = true
        AudioFocus.request("video", AudioFocus.mediaPriority)
        if (MediaSession.playing)
            MediaSession.pause()
    }

    function playPrev() {
        if (clips.length === 0)
            return
        openPlayer((index - 1 + clips.length) % clips.length)
    }

    function playNext() {
        if (clips.length === 0)
            return
        openPlayer((index + 1) % clips.length)
    }

    function goBack() {
        player.playing = false
        AudioFocus.release("video")
        page = "library"
    }

    function pokeControls() {
        controlsVisible = !controlsVisible
        if (controlsVisible)
            hideTimer.restart()
    }

    onVisibleChanged: {
        if (!visible && page === "player")
            player.playing = false
    }

    Component.onDestruction: AudioFocus.release("video")

    VideoScreen {
        id: clipProbe
        visible: false
        width: 1
        height: 1
        Component.onCompleted: root.clips = listClips()
    }

    Timer {
        id: hideTimer
        interval: 3500
        onTriggered: {
            if (root.page === "player" && player.playing)
                root.controlsVisible = false
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.page === "player" ? "#000000" : SystemState.page
    }

    Item {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        visible: root.page === "library"

        Column {
            anchors.fill: parent
            spacing: 14

            Text {
                text: "视频"
                color: SystemState.ink
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                visible: root.clips.length === 0
                text: "media/video 下没有 AVI"
                color: SystemState.secondary
                font.pixelSize: 14
            }

            GridView {
                id: grid
                width: parent.width
                height: parent.height - 48
                clip: true
                cellWidth: width / 4
                cellHeight: cellWidth * 0.62 + 40
                model: root.clips

                delegate: Item {
                    required property var modelData
                    required property int index
                    width: grid.cellWidth
                    height: grid.cellHeight

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Rectangle {
                            width: parent.width
                            height: width * 0.56
                            radius: 12
                            color: "#000000"
                            clip: true

                            VideoScreen {
                                anchors.fill: parent
                                preview: true
                                clipName: modelData.title
                                playing: false
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: "transparent"
                                border.color: index === root.index ? "#5E5CE6" : "transparent"
                                border.width: 2
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "▶"
                                color: "#FFFFFF"
                                font.pixelSize: 28
                                opacity: 0.85
                            }
                        }

                        Text {
                            width: parent.width
                            text: modelData.title
                            color: SystemState.ink
                            font.pixelSize: 15
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.openPlayer(index)
                    }
                }
            }
        }
    }

    Item {
        id: playerPage
        anchors.fill: parent
        visible: root.page === "player"

        VideoScreen {
            id: player
            anchors.fill: parent
            preview: false
            onEnded: root.playNext()
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.pokeControls()
        }

        Item {
            anchors.fill: parent
            opacity: root.controlsVisible ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 180 } }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 72
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#99000000" }
                    GradientStop { position: 1.0; color: "#00000000" }
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14

                    Rectangle {
                        width: 44
                        height: 44
                        radius: 22
                        color: "#66FFFFFF"
                        Text {
                            anchors.centerIn: parent
                            text: "←"
                            color: "#FFFFFF"
                            font.pixelSize: 22
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.goBack()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.clips.length > 0 ? root.clips[root.index].title : ""
                        color: "#FFFFFF"
                        font.pixelSize: 22
                        font.bold: true
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 132
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#00000000" }
                    GradientStop { position: 1.0; color: "#B3000000" }
                }

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                    spacing: 8

                    Column {
                        width: parent.width
                        spacing: 2
                        Slider {
                            id: seek
                            width: parent.width
                            height: 28
                            from: 0
                            to: Math.max(1, player.duration)
                            onMoved: {
                                player.seek(Math.round(value))
                                hideTimer.restart()
                            }
                            background: Rectangle {
                                x: seek.leftPadding
                                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                implicitHeight: 6
                                width: seek.availableWidth
                                height: 6
                                radius: 3
                                color: "#66FFFFFF"
                                Rectangle {
                                    width: seek.visualPosition * parent.width
                                    height: parent.height
                                    radius: 3
                                    color: "#5E5CE6"
                                }
                            }
                            handle: Rectangle {
                                x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                width: 20
                                height: 20
                                radius: 10
                                color: "#FFFFFF"
                            }
                        }
                        Row {
                            width: parent.width
                            Text { text: root.mmss(player.position); color: "#FFFFFF"; font.pixelSize: 12 }
                            Item { width: parent.width - 80; height: 1 }
                            Text { text: root.mmss(player.duration); color: "#FFFFFF"; font.pixelSize: 12 }
                        }
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 28

                        Rectangle {
                            width: 56
                            height: 56
                            radius: 28
                            color: "#55FFFFFF"
                            Text {
                                anchors.centerIn: parent
                                text: "⏮"
                                color: "#FFFFFF"
                                font.pixelSize: 20
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.playPrev()
                                    hideTimer.restart()
                                }
                            }
                        }
                        Rectangle {
                            width: 68
                            height: 68
                            radius: 34
                            color: "#5E5CE6"
                            Text {
                                anchors.centerIn: parent
                                text: player.playing ? "❚❚" : "▶"
                                color: "#FFFFFF"
                                font.pixelSize: 24
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    player.playing = !player.playing
                                    if (player.playing)
                                        AudioFocus.request("video", AudioFocus.mediaPriority)
                                    else
                                        AudioFocus.release("video")
                                    root.controlsVisible = true
                                    hideTimer.restart()
                                }
                            }
                        }
                        Rectangle {
                            width: 56
                            height: 56
                            radius: 28
                            color: "#55FFFFFF"
                            Text {
                                anchors.centerIn: parent
                                text: "⏭"
                                color: "#FFFFFF"
                                font.pixelSize: 20
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.playNext()
                                    hideTimer.restart()
                                }
                            }
                        }
                    }
                }
            }
        }

        Connections {
            target: player
            function onPositionChanged() {
                if (!seek.pressed)
                    seek.value = player.position
            }
            function onDurationChanged() {
                seek.to = Math.max(1, player.duration)
                seek.value = player.position
            }
            function onPlayingChanged() {
                if (player.playing)
                    hideTimer.restart()
                else
                    root.controlsVisible = true
            }
        }
    }
}
