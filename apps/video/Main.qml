import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root
    property string tab: "local"
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
        if (DlnaRenderer.hasMedia)
            DlnaRenderer.stopMedia()
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

    function switchTab(t) {
        if (tab === t)
            return
        if (t === "wlan" && page === "player")
            goBack()
        tab = t
    }

    onVisibleChanged: {
        if (!visible && page === "player")
            player.playing = false
    }

    Component.onDestruction: {
        AudioFocus.release("video")
        if (DlnaRenderer.running)
            DlnaRenderer.stop()
    }

    function consumePending() {
        const name = FileBrowser.pendingVideo
        if (!name.length)
            return
        root.clips = clipProbe.listClips()
        root.tab = "local"
        for (let i = 0; i < root.clips.length; ++i) {
            if (root.clips[i].title === name) {
                FileBrowser.clearPendingVideo()
                openPlayer(i)
                return
            }
        }
        FileBrowser.clearPendingVideo()
    }

    VideoScreen {
        id: clipProbe
        visible: false
        width: 1
        height: 1
        Component.onCompleted: {
            root.clips = listClips()
            root.consumePending()
        }
    }

    Connections {
        target: FileBrowser
        function onPendingVideoChanged() { root.consumePending() }
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
        color: (root.page === "player" || (root.tab === "wlan" && DlnaRenderer.hasFrame))
               ? "#000000" : "transparent"
    }

    Item {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        visible: root.page === "library"

        Column {
            anchors.fill: parent
            spacing: 12

            Row {
                width: parent.width
                spacing: 16
                Text {
                    text: "视频"
                    color: SystemState.ink
                    font.pixelSize: 28
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item { width: 8; height: 1 }
                IosSegmented {
                    width: 200
                    anchors.verticalCenter: parent.verticalCenter
                    labels: ["本地", "无线投屏"]
                    currentIndex: root.tab === "wlan" ? 1 : 0
                    onActivated: function(i) { root.switchTab(i === 1 ? "wlan" : "local") }
                }
            }

            Item {
                width: parent.width
                height: parent.height - 54
                visible: root.tab === "local"

                Text {
                    visible: root.clips.length === 0
                    text: "media/video 下没有 AVI"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }

                GridView {
                    id: grid
                    anchors.fill: parent
                    clip: true
                    cellWidth: width / 4
                    cellHeight: cellWidth * 0.62 + 40
                    model: root.clips
                    visible: root.clips.length > 0

                    delegate: IosPressable {
                        required property var modelData
                        required property int index
                        width: grid.cellWidth
                        height: grid.cellHeight
                        onClicked: root.openPlayer(index)

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
                                    border.color: index === root.index ? SystemState.tint : "transparent"
                                    border.width: 2
                                }

                                IosIcon {
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    name: "play"
                                    ink: "#FFFFFF"
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
                    }
                }
            }

            Item {
                width: parent.width
                height: parent.height - 54
                visible: root.tab === "wlan"

                Column {
                    anchors.centerIn: parent
                    spacing: 14
                    visible: !DlnaRenderer.hasFrame
                    width: Math.min(parent.width, 520)

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "无线投屏"
                        color: SystemState.ink
                        font.pixelSize: 26
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: DlnaRenderer.status
                        color: SystemState.ink
                        font.pixelSize: 18
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: DlnaRenderer.detail
                        color: SystemState.secondary
                        font.pixelSize: 14
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: DlnaRenderer.hasMedia
                        text: DlnaRenderer.mediaTitle + " · " + DlnaRenderer.transportState
                        color: SystemState.secondary
                        font.pixelSize: 13
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 12
                        IosPressable {
                            width: 120
                            height: 40
                            enabled: !DlnaRenderer.running
                            onClicked: {
                                if (root.page === "player")
                                    root.goBack()
                                DlnaRenderer.start()
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 20
                                color: DlnaRenderer.running ? SystemState.fill : SystemState.tint
                                Text {
                                    anchors.centerIn: parent
                                    text: DlnaRenderer.running ? "运行中" : "开始接收"
                                    color: DlnaRenderer.running ? SystemState.secondary : "#FFFFFF"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                            }
                        }
                        IosPressable {
                            width: 100
                            height: 40
                            visible: DlnaRenderer.running
                            onClicked: DlnaRenderer.stop()
                            Rectangle {
                                anchors.fill: parent
                                radius: 20
                                color: SystemState.danger
                                Text {
                                    anchors.centerIn: parent
                                    text: "停止"
                                    color: "#FFFFFF"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                            }
                        }
                        IosPressable {
                            width: 100
                            height: 40
                            visible: DlnaRenderer.hasMedia
                            onClicked: DlnaRenderer.stopMedia()
                            Rectangle {
                                anchors.fill: parent
                                radius: 20
                                color: SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: "清除"
                                    color: SystemState.ink
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.tab === "wlan" && root.page === "library"
        z: 10

        DlnaVideoItem {
            anchors.fill: parent
            visible: DlnaRenderer.hasFrame
            session: DlnaRenderer
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            spacing: 12
            visible: DlnaRenderer.hasFrame
            z: 20

            IosPressable {
                width: 88
                height: 36
                onClicked: {
                    if (DlnaRenderer.transportState === "PAUSED_PLAYBACK")
                        DlnaRenderer.resumeMedia()
                    else
                        DlnaRenderer.pauseMedia()
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    color: SystemState.elevated
                    Text {
                        anchors.centerIn: parent
                        text: DlnaRenderer.transportState === "PAUSED_PLAYBACK" ? "继续" : "暂停"
                        color: SystemState.ink
                        font.pixelSize: 14
                    }
                }
            }
            IosPressable {
                width: 88
                height: 36
                onClicked: DlnaRenderer.stopMedia()
                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    color: SystemState.danger
                    Text {
                        anchors.centerIn: parent
                        text: "停止"
                        color: "#FFFFFF"
                        font.pixelSize: 14
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

                    IosPressable {
                        width: 44
                        height: 44
                        onClicked: root.goBack()
                        Rectangle {
                            anchors.fill: parent
                            radius: 22
                            color: "#66FFFFFF"
                            IosIcon {
                                anchors.centerIn: parent
                                width: 22
                                height: 22
                                name: "back"
                                ink: "#FFFFFF"
                            }
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
                                    color: SystemState.tint
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

                        IosPressable {
                            width: 56
                            height: 56
                            onClicked: {
                                root.playPrev()
                                hideTimer.restart()
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 28
                                color: "#55FFFFFF"
                                IosIcon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    name: "prev"
                                    ink: "#FFFFFF"
                                }
                            }
                        }
                        IosPressable {
                            width: 68
                            height: 68
                            onClicked: {
                                player.playing = !player.playing
                                if (player.playing)
                                    AudioFocus.request("video", AudioFocus.mediaPriority)
                                else
                                    AudioFocus.release("video")
                                root.controlsVisible = true
                                hideTimer.restart()
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 34
                                color: SystemState.tint
                                IosIcon {
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    name: player.playing ? "pause" : "play"
                                    ink: "#FFFFFF"
                                }
                            }
                        }
                        IosPressable {
                            width: 56
                            height: 56
                            onClicked: {
                                root.playNext()
                                hideTimer.restart()
                            }
                            Rectangle {
                                anchors.fill: parent
                                radius: 28
                                color: "#55FFFFFF"
                                IosIcon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    name: "next"
                                    ink: "#FFFFFF"
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
