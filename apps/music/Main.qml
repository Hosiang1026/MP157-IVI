import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root

    property int mainTab: 0
    property string page: "playlists"
    property int playlistIndex: 0
    property string query: ""
    property bool showLyrics: false
    property string streamQuery: ""

    readonly property int miniH: 80
    readonly property bool hasTrack: MediaSession.title.length > 0
    readonly property bool showMini: hasTrack && page !== "player" && mainTab === 0

    Component.onCompleted: {
        if (StreamSession.playing)
            mainTab = 1
    }

    Connections {
        target: StreamSession
        function onLoggedInChanged() {
            if (!StreamSession.loggedIn && root.mainTab === 1)
                root.streamQuery = ""
        }
    }

    readonly property var playlists: {
        const all = MediaSession.tracks
        const n = all.length
        if (n === 0)
            return []
        const colors = ["#FF2D55", "#5856D6", "#007AFF", "#34C759"]
        const names = ["全部曲目", "精选 A", "精选 B", "精选 C"]
        const out = []
        const allIdx = []
        for (let i = 0; i < n; ++i)
            allIdx.push(i)
        out.push({ name: names[0], color: colors[0], tracks: allIdx.slice() })
        for (let p = 1; p < 4; ++p) {
            const tracks = []
            for (let i = p - 1; i < n; i += 3)
                tracks.push(i)
            if (tracks.length === 0)
                tracks.push(0)
            out.push({ name: names[p], color: colors[p], tracks: tracks })
        }
        return out
    }

    readonly property var currentPlaylist: playlists.length > 0
                                           ? playlists[Math.min(playlistIndex, playlists.length - 1)]
                                           : { name: "", color: "#8E8E93", tracks: [] }
    readonly property var playlistSongs: {
        const all = MediaSession.tracks
        const idxs = currentPlaylist.tracks
        const out = []
        for (let i = 0; i < idxs.length; ++i) {
            const ti = idxs[i]
            if (ti >= 0 && ti < all.length)
                out.push({
                    title: all[ti].title,
                    artist: all[ti].artist,
                    duration: all[ti].duration,
                    color: all[ti].color,
                    trackIndex: ti
                })
        }
        return out
    }
    readonly property var filteredSongs: {
        const q = query.trim().toLowerCase()
        if (q.length === 0)
            return playlistSongs
        const out = []
        for (let i = 0; i < playlistSongs.length; ++i) {
            const s = playlistSongs[i]
            if (s.title.toLowerCase().indexOf(q) >= 0 || s.artist.toLowerCase().indexOf(q) >= 0)
                out.push(s)
        }
        return out
    }
    readonly property var lyricLines: MediaSession.lyrics.length > 0 ? MediaSession.lyrics : [MediaSession.title || "未在播放"]
    readonly property int lyricIndex: {
        const n = lyricLines.length
        if (n <= 0)
            return 0
        return Math.floor(Math.max(0, MediaSession.position) / 3) % n
    }
    readonly property string modeIconName: {
        if (MediaSession.playMode === 1)
            return "repeat1"
        if (MediaSession.playMode === 2)
            return "shuffle"
        return "repeat"
    }

    function mmss(value) {
        const s = Math.max(0, Math.floor(value))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function applyPlaylistQueue() {
        const idxs = []
        for (let i = 0; i < playlistSongs.length; ++i)
            idxs.push(playlistSongs[i].trackIndex)
        MediaSession.setQueue(idxs)
    }

    function openPlaylist(i) {
        playlistIndex = i
        query = ""
        if (songSearch)
            songSearch.text = ""
        page = "songs"
        applyPlaylistQueue()
    }

    function playSong(trackIndex) {
        if (MediaSession.bluetoothMode)
            BluetoothMediaHub.clearActive()
        applyPlaylistQueue()
        MediaSession.playInQueue(trackIndex)
        page = "player"
        showLyrics = false
    }

    function goBack() {
        if (page === "player")
            page = "songs"
        else if (page === "songs")
            page = "playlists"
    }

    function coverGlyph(title) {
        return title && title.length > 0 ? title.charAt(0) : "♪"
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    IosSegmented {
        id: mainTabBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        height: 40
        labels: ["本地", "音流"]
        currentIndex: root.mainTab
        onActivated: function (index) { root.mainTab = index }
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainTabBar.bottom
        anchors.bottom: root.page === "player" ? parent.bottom : miniBar.top
        anchors.margins: 16
        anchors.topMargin: 12
        anchors.bottomMargin: root.page === "player" ? 16 : 8
        visible: root.mainTab === 0

        Item {
            anchors.fill: parent
            visible: root.page === "playlists"

            Column {
                anchors.fill: parent
                spacing: 14

                Rectangle {
                    width: parent.width
                    height: btCol.height + 20
                    radius: 16
                    color: SystemState.card
                    border.width: 1 / Screen.devicePixelRatio
                    border.color: SystemState.separator
                    Column {
                        id: btCol
                        x: 14
                        y: 10
                        width: parent.width - 28
                        spacing: 8
                        Row {
                            width: parent.width
                            Text {
                                width: parent.width - 72
                                text: "蓝牙音源"
                                color: SystemState.ink
                                font.pixelSize: 15
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            IosPressable {
                                width: 48
                                height: 28
                                anchors.verticalCenter: parent.verticalCenter
                                Text {
                                    anchors.centerIn: parent
                                    text: "刷新"
                                    color: SystemState.tint
                                    font.pixelSize: 13
                                }
                                onClicked: BluetoothMediaHub.refresh()
                            }
                        }
                        Row {
                            width: parent.width
                            spacing: 8
                            visible: BluetoothMediaHub.status.length > 0
                            Canvas {
                                id: btStatusIcon
                                width: 14
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.clearRect(0, 0, width, height)
                                    const on = MediaSession.bluetoothMode
                                    ctx.strokeStyle = on ? SystemState.tint : SystemState.secondary
                                    ctx.lineWidth = 1.6
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    const cx = 7
                                    ctx.beginPath()
                                    ctx.moveTo(cx, 1.2)
                                    ctx.lineTo(cx, 14.8)
                                    ctx.moveTo(cx, 4.5)
                                    ctx.lineTo(11.5, 1.8)
                                    ctx.moveTo(cx, 4.5)
                                    ctx.lineTo(11.5, 7.2)
                                    ctx.moveTo(cx, 11.5)
                                    ctx.lineTo(11.5, 14.2)
                                    ctx.moveTo(cx, 11.5)
                                    ctx.lineTo(11.5, 8.8)
                                    ctx.moveTo(cx, 4.5)
                                    ctx.lineTo(2.5, 1.8)
                                    ctx.moveTo(cx, 4.5)
                                    ctx.lineTo(2.5, 7.2)
                                    ctx.moveTo(cx, 11.5)
                                    ctx.lineTo(2.5, 14.2)
                                    ctx.moveTo(cx, 11.5)
                                    ctx.lineTo(2.5, 8.8)
                                    ctx.stroke()
                                }
                                Connections {
                                    target: MediaSession
                                    function onSourceChanged() { btStatusIcon.requestPaint() }
                                }
                                Connections {
                                    target: SystemState
                                    function onDarkChanged() { btStatusIcon.requestPaint() }
                                }
                            }
                            Rectangle {
                                width: 7
                                height: 7
                                radius: 3.5
                                anchors.verticalCenter: parent.verticalCenter
                                color: MediaSession.bluetoothMode ? SystemState.success : SystemState.secondary
                            }
                            Text {
                                width: parent.width - 40
                                text: BluetoothMediaHub.status
                                color: SystemState.secondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Flow {
                            width: parent.width
                            spacing: 8
                            IosPressable {
                                width: localLabel.implicitWidth + 24
                                height: 30
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 8
                                    color: !MediaSession.bluetoothMode ? SystemState.selected : SystemState.fill
                                }
                                Text {
                                    id: localLabel
                                    anchors.centerIn: parent
                                    text: "本机媒体"
                                    color: !MediaSession.bluetoothMode ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 13
                                    font.bold: !MediaSession.bluetoothMode
                                }
                                onClicked: BluetoothMediaHub.clearActive()
                            }
                            Repeater {
                                model: BluetoothMediaHub.devices
                                delegate: IosPressable {
                                    required property var modelData
                                    width: Math.min(200, btChipRow.width + 20)
                                    height: 30
                                    opacity: modelData.paired ? 1 : 0.55
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 8
                                        color: modelData.active ? SystemState.selected : SystemState.fill
                                    }
                                    Row {
                                        id: btChipRow
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Rectangle {
                                            width: 7
                                            height: 7
                                            radius: 3.5
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: modelData.connected ? SystemState.success
                                                   : (modelData.paired ? SystemState.secondary : SystemState.fill)
                                            border.color: modelData.connected ? SystemState.success : SystemState.separator
                                            border.width: modelData.connected ? 0 : 1
                                        }
                                        Text {
                                            id: btName
                                            text: "媒体·" + (modelData.name || modelData.address)
                                            color: modelData.active ? SystemState.tint : SystemState.ink
                                            font.pixelSize: 13
                                            font.bold: modelData.active
                                            elide: Text.ElideRight
                                            width: Math.min(160, implicitWidth)
                                        }
                                    }
                                    onClicked: BluetoothMediaHub.selectDevice(modelData.address)
                                    onPressAndHold: BluetoothMediaHub.pairDevice(modelData.address)
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 48 - btCol.height - 34

                    GridView {
                        id: playlistGrid
                        anchors.fill: parent
                        cellWidth: width / 4
                        cellHeight: height / 2
                        clip: true
                        interactive: false
                        visible: root.playlists.length > 0
                        model: root.playlists
                        delegate: Item {
                            id: cell
                            required property int index
                            required property var modelData
                            width: playlistGrid.cellWidth
                            height: playlistGrid.cellHeight
                            readonly property var playlist: modelData
                            readonly property int coverSize: Math.min(width - 20, height - 52)

                            Column {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                width: cell.coverSize

                                IosPressable {
                                    width: cell.coverSize
                                    height: cell.coverSize
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 12
                                        color: cell.playlist.color
                                    }
                                    IosIcon {
                                        anchors.centerIn: parent
                                        width: Math.round(cell.coverSize * 0.36)
                                        height: Math.round(cell.coverSize * 0.36)
                                        name: "play"
                                        ink: "#FFFFFF"
                                    }
                                    onClicked: root.openPlaylist(cell.index)
                                }
                                Text {
                                    width: parent.width
                                    text: cell.playlist.name
                                    color: SystemState.ink
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                Text {
                                    width: parent.width
                                    text: cell.playlist.tracks.length + " 首"
                                    color: SystemState.secondary
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }

                    IosEmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: root.playlists.length === 0
                        icon: "play"
                        title: "暂无曲目"
                        subtitle: "将音乐放入媒体目录后刷新"
                    }
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: root.page === "songs"

            Column {
                anchors.fill: parent
                spacing: 12

                Row {
                    spacing: 12
                    IosPressable {
                        width: 44
                        height: 44
                        IosIcon {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            name: "back"
                            ink: SystemState.tint
                        }
                        onClicked: root.goBack()
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            text: root.currentPlaylist.name
                            color: SystemState.ink
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Text {
                            text: root.playlistSongs.length + " 首"
                            color: SystemState.secondary
                            font.pixelSize: 13
                        }
                    }
                }

                IosSearchField {
                    id: songSearch
                    width: parent.width
                    placeholder: "搜索歌曲"
                    onTextChanged: root.query = text
                    onCleared: root.query = ""
                }

                Rectangle {
                    width: parent.width
                    height: parent.height - 120
                    radius: 16
                    color: SystemState.card
                    border.width: 1 / Screen.devicePixelRatio
                    border.color: SystemState.separator
                    clip: true

                    ListView {
                        anchors.fill: parent
                        clip: true
                        spacing: 0
                        visible: root.filteredSongs.length > 0
                        model: root.filteredSongs
                        delegate: IosPressable {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 64

                            Rectangle {
                                anchors.fill: parent
                                color: modelData.trackIndex === MediaSession.trackIndex ? SystemState.selected : "transparent"
                            }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 14
                                spacing: 12

                                Rectangle {
                                    width: 44
                                    height: 44
                                    radius: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: modelData.color || root.currentPlaylist.color
                                    IosIcon {
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        name: "play"
                                        ink: "#FFFFFF"
                                    }
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    width: parent.width - 120
                                    Text {
                                        text: modelData.title
                                        color: modelData.trackIndex === MediaSession.trackIndex ? SystemState.tint : SystemState.ink
                                        font.pixelSize: 16
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }
                                    Text {
                                        text: modelData.artist
                                        color: SystemState.secondary
                                        font.pixelSize: 13
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.mmss(modelData.duration)
                                    color: SystemState.secondary
                                    font.pixelSize: 13
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 68
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 0.5
                                color: SystemState.separator
                                visible: index < root.filteredSongs.length - 1
                            }

                            onClicked: root.playSong(modelData.trackIndex)
                        }
                    }

                    IosEmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: root.filteredSongs.length === 0
                        icon: "search"
                        title: "暂无曲目"
                        subtitle: root.query.length > 0 ? "没有匹配的歌曲" : "歌单为空"
                    }
                }
            }
        }

        Item {
            id: playerPage
            anchors.fill: parent
            visible: root.page === "player"

            Row {
                id: playerHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 40
                spacing: 8

                IosPressable {
                    width: 40
                    height: 40
                    IosIcon {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        name: "back"
                        ink: SystemState.tint
                    }
                    onClicked: root.goBack()
                }
                Item { width: parent.width - 140; height: 1 }
                IosPressable {
                    width: 64
                    height: 40
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: root.showLyrics ? "封面" : "歌词"
                        color: SystemState.tint
                        font.pixelSize: 15
                    }
                    onClicked: root.showLyrics = !root.showLyrics
                }
            }

            Item {
                id: playerBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: playerHeader.bottom
                anchors.bottom: parent.bottom
                anchors.topMargin: 4

                Item {
                    anchors.fill: parent
                    visible: !root.showLyrics
                    readonly property int coverSize: Math.max(
                        96,
                        Math.min(180, Math.floor(Math.min(width * 0.42, height - 200)))
                    )

                    Rectangle {
                        id: coverArt
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        width: parent.coverSize
                        height: parent.coverSize
                        radius: 12
                        color: MediaSession.coverColor
                        IosIcon {
                            anchors.centerIn: parent
                            width: Math.round(parent.width * 0.28)
                            height: Math.round(parent.width * 0.28)
                            name: MediaSession.playing ? "pause" : "play"
                            ink: "#FFFFFF"
                        }
                        IosPressable {
                            anchors.fill: parent
                            onClicked: root.showLyrics = true
                        }
                    }

                    Column {
                        id: metaCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: coverArt.bottom
                        anchors.topMargin: 10
                        spacing: 2
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: MediaSession.title
                            color: SystemState.ink
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: MediaSession.artist
                            color: SystemState.secondary
                            font.pixelSize: 13
                        }
                    }

                    Column {
                        id: seekCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: transportRow.top
                        anchors.bottomMargin: 6
                        width: Math.min(parent.width, 420)
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 2
                        Slider {
                            id: seek
                            width: parent.width
                            height: 22
                            from: 0
                            to: Math.max(1, MediaSession.duration)
                            onMoved: MediaSession.seek(Math.round(value))
                            Component.onCompleted: value = MediaSession.position
                            background: Rectangle {
                                x: seek.leftPadding
                                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                implicitHeight: 4
                                width: seek.availableWidth
                                height: 4
                                radius: 2
                                color: SystemState.fill
                                Rectangle {
                                    width: seek.visualPosition * parent.width
                                    height: parent.height
                                    radius: 2
                                    color: SystemState.tint
                                }
                            }
                            handle: Rectangle {
                                x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                width: 18
                                height: 18
                                radius: 9
                                color: "#FFFFFF"
                                border.color: SystemState.separator
                                border.width: 0.5
                            }
                        }
                        Row {
                            width: parent.width
                            Text { text: root.mmss(MediaSession.position); color: SystemState.secondary; font.pixelSize: 11 }
                            Item { width: parent.width - 80; height: 1 }
                            Text { text: root.mmss(MediaSession.duration); color: SystemState.secondary; font.pixelSize: 11 }
                        }
                    }

                    Row {
                        id: transportRow
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        spacing: 22

                        IosPressable {
                            width: 40
                            height: 40
                            anchors.verticalCenter: parent.verticalCenter
                            IosIcon {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                name: root.modeIconName
                                ink: SystemState.ink
                            }
                            onClicked: MediaSession.cyclePlayMode()
                        }
                        IosPressable {
                            width: 44
                            height: 44
                            anchors.verticalCenter: parent.verticalCenter
                            IosIcon {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                name: "prev"
                                ink: SystemState.ink
                            }
                            onClicked: MediaSession.previous()
                        }
                        IosPressable {
                            width: 56
                            height: 56
                            Rectangle {
                                anchors.fill: parent
                                radius: 28
                                color: SystemState.tint
                            }
                            IosIcon {
                                anchors.centerIn: parent
                                width: 26
                                height: 26
                                name: MediaSession.playing ? "pause" : "play"
                                ink: "#FFFFFF"
                            }
                            onClicked: MediaSession.toggle()
                        }
                        IosPressable {
                            width: 44
                            height: 44
                            anchors.verticalCenter: parent.verticalCenter
                            IosIcon {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                name: "next"
                                ink: SystemState.ink
                            }
                            onClicked: MediaSession.next()
                        }
                        Item { width: 40; height: 40 }
                    }
                }

                ListView {
                    id: lyricView
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    visible: root.showLyrics
                    clip: true
                    spacing: 14
                    model: root.lyricLines
                    preferredHighlightBegin: height / 2 - 20
                    preferredHighlightEnd: height / 2 + 20
                    highlightRangeMode: ListView.StrictlyEnforceRange
                    currentIndex: root.lyricIndex
                    highlightMoveDuration: 220
                    delegate: Text {
                        required property string modelData
                        required property int index
                        width: lyricView.width
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: index === root.lyricIndex ? SystemState.ink : SystemState.secondary
                        font.pixelSize: index === root.lyricIndex ? 22 : 16
                        font.bold: index === root.lyricIndex
                        opacity: index === root.lyricIndex ? 1.0 : 0.45
                    }
                    IosPressable {
                        anchors.fill: parent
                        onClicked: root.showLyrics = false
                    }
                }
            }
        }
    }

    Item {
        id: streamPane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainTabBar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 8
        visible: root.mainTab === 1

        Column {
            anchors.fill: parent
            spacing: 12
            visible: !StreamSession.loggedIn

            Text {
                text: "连接 Navidrome / Subsonic / Jellyfin"
                color: SystemState.secondary
                font.pixelSize: 14
            }

            Rectangle {
                width: parent.width
                height: loginCol.implicitHeight + 28
                radius: 16
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator

                Column {
                    id: loginCol
                    x: 16
                    y: 14
                    width: parent.width - 32
                    spacing: 12

                    IosSegmented {
                        width: parent.width
                        height: 40
                        labels: ["Subsonic", "Jellyfin"]
                        currentIndex: StreamSession.backend === "jellyfin" ? 1 : 0
                        onActivated: function (index) {
                            StreamSession.backend = index === 1 ? "jellyfin" : "subsonic"
                        }
                    }

                    TextField {
                        id: serverField
                        width: parent.width
                        height: 40
                        placeholderText: "服务器 http://192.168.1.10:4533"
                        color: SystemState.ink
                        placeholderTextColor: SystemState.secondary
                        text: StreamSession.serverUrl
                        background: Rectangle { radius: 10; color: SystemState.fill }
                    }
                    TextField {
                        id: userField
                        width: parent.width
                        height: 40
                        placeholderText: "用户名"
                        color: SystemState.ink
                        placeholderTextColor: SystemState.secondary
                        text: StreamSession.username
                        background: Rectangle { radius: 10; color: SystemState.fill }
                    }
                    TextField {
                        id: passField
                        width: parent.width
                        height: 40
                        placeholderText: "密码"
                        echoMode: TextInput.Password
                        color: SystemState.ink
                        placeholderTextColor: SystemState.secondary
                        text: StreamSession.password
                        background: Rectangle { radius: 10; color: SystemState.fill }
                    }

                    Row {
                        spacing: 12
                        IosPressable {
                            width: 120
                            height: 40
                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: SystemState.tint
                            }
                            Text {
                                anchors.centerIn: parent
                                text: StreamSession.loading ? "连接中…" : "登录"
                                color: "#FFFFFF"
                                font.pixelSize: 16
                                font.bold: true
                            }
                            onClicked: {
                                StreamSession.serverUrl = serverField.text
                                StreamSession.username = userField.text
                                StreamSession.password = passField.text
                                StreamSession.login()
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: StreamSession.status + (StreamSession.detail.length ? (" · " + StreamSession.detail) : "")
                            color: SystemState.secondary
                            font.pixelSize: 13
                        }
                    }
                }
            }
        }

        Row {
            anchors.fill: parent
            spacing: 16
            visible: StreamSession.loggedIn

            Rectangle {
                width: 300
                height: parent.height
                radius: 16
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 80
                            text: "专辑"
                            color: SystemState.ink
                            font.pixelSize: 18
                            font.bold: true
                        }
                        IosPressable {
                            width: 72
                            height: 32
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: SystemState.fill
                            }
                            Text {
                                anchors.centerIn: parent
                                text: "退出"
                                color: SystemState.danger
                                font.pixelSize: 13
                            }
                            onClicked: StreamSession.logout()
                        }
                    }

                    ListView {
                        width: parent.width
                        height: parent.height - 48
                        clip: true
                        model: StreamSession.albums
                        delegate: IosPressable {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 58
                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: modelData.name
                                    color: SystemState.ink
                                    font.pixelSize: 15
                                }
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: modelData.artist || ""
                                    color: SystemState.secondary
                                    font.pixelSize: 12
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 0.5
                                color: SystemState.separator
                            }
                            onClicked: StreamSession.openAlbum(modelData.id)
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width - 316
                height: parent.height
                radius: 16
                color: SystemState.card
                border.width: 1 / Screen.devicePixelRatio
                border.color: SystemState.separator
                clip: true

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 10
                        IosSearchField {
                            width: parent.width - 170
                            height: 40
                            placeholder: "搜索曲目"
                            text: root.streamQuery
                            onSubmitted: {
                                root.streamQuery = text
                                StreamSession.search(text)
                            }
                        }
                        IosPressable {
                            width: 72
                            height: 40
                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: SystemState.fill
                            }
                            Text {
                                anchors.centerIn: parent
                                text: "专辑"
                                color: SystemState.ink
                                font.pixelSize: 14
                            }
                            onClicked: StreamSession.loadAlbums()
                        }
                        IosPressable {
                            width: 72
                            height: 40
                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: StreamSession.playing ? SystemState.danger : SystemState.tint
                            }
                            Text {
                                anchors.centerIn: parent
                                text: StreamSession.playing ? "停止" : "播放"
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.bold: true
                            }
                            onClicked: StreamSession.toggle()
                        }
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: StreamSession.playing
                              ? (StreamSession.title + (StreamSession.artist.length ? (" · " + StreamSession.artist) : ""))
                              : (StreamSession.status + (StreamSession.detail.length ? (" · " + StreamSession.detail) : ""))
                        color: StreamSession.playing ? SystemState.success : SystemState.secondary
                        font.pixelSize: 13
                    }

                    ListView {
                        width: parent.width
                        height: parent.height - 96
                        clip: true
                        model: StreamSession.tracks
                        delegate: IosPressable {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 58
                            Rectangle {
                                anchors.fill: parent
                                color: StreamSession.currentIndex === index ? SystemState.selected : "transparent"
                            }
                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: modelData.title
                                    color: StreamSession.currentIndex === index ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 16
                                    font.bold: StreamSession.currentIndex === index
                                }
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: (modelData.artist || "") + (modelData.album ? (" · " + modelData.album) : "")
                                    color: SystemState.secondary
                                    font.pixelSize: 12
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 0.5
                                color: SystemState.separator
                            }
                            onClicked: StreamSession.playIndex(index)
                        }
                    }
                }

                IosEmptyState {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    visible: !StreamSession.loading && StreamSession.tracks.length === 0
                    icon: "speaker"
                    title: "选择专辑或搜索"
                    subtitle: "登录后从左侧打开专辑，或搜索曲目"
                }

                IosSpinner {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    visible: StreamSession.loading
                    running: StreamSession.loading
                    ink: SystemState.tint
                }
            }
        }
    }

    Rectangle {
        id: miniBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.showMini ? root.miniH : 0
        visible: root.showMini
        color: SystemState.card
        border.width: 1 / Screen.devicePixelRatio
        border.color: SystemState.separator
        radius: 16
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: SystemState.separator
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            Rectangle {
                width: 48
                height: 48
                radius: 10
                anchors.verticalCenter: parent.verticalCenter
                color: MediaSession.coverColor
                IosIcon {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    name: MediaSession.playing ? "pause" : "play"
                    ink: "#FFFFFF"
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                width: parent.width - 180
                Text {
                    text: MediaSession.title
                    color: SystemState.ink
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                    width: parent.width
                }
                Text {
                    text: MediaSession.artist
                    color: SystemState.secondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    width: parent.width
                }
            }
            IosPressable {
                width: 48
                height: 48
                anchors.verticalCenter: parent.verticalCenter
                IosIcon {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    name: MediaSession.playing ? "pause" : "play"
                    ink: SystemState.tint
                }
                onClicked: MediaSession.toggle()
            }
            IosPressable {
                width: 48
                height: 48
                anchors.verticalCenter: parent.verticalCenter
                IosIcon {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    name: "next"
                    ink: SystemState.tint
                }
                onClicked: MediaSession.next()
            }
        }

        IosPressable {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width - 130
            onClicked: {
                root.page = "player"
                root.showLyrics = false
            }
        }
    }

    Connections {
        target: MediaSession
        function onPositionChanged() {
            if (root.page === "player" && seek && !seek.pressed)
                seek.value = MediaSession.position
        }
        function onTrackChanged() {
            if (root.page === "player" && seek) {
                seek.to = Math.max(1, MediaSession.duration)
                seek.value = MediaSession.position
            }
        }
    }
}
