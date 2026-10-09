import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root

    property string page: "playlists"
    property int playlistIndex: 0
    property string query: ""
    property bool showLyrics: false

    readonly property int miniH: 72
    readonly property bool hasTrack: MediaSession.title.length > 0
    readonly property bool showMini: hasTrack && page !== "player"

    readonly property var playlists: {
        const all = MediaSession.tracks
        const n = all.length
        if (n === 0)
            return [{ name: "空歌单", color: "#8E8E93", cover: "空", tracks: [] }]
        const colors = ["#FF2D55", "#5856D6", "#007AFF", "#34C759"]
        const names = ["全部曲目", "精选 A", "精选 B", "精选 C"]
        const out = []
        const allIdx = []
        for (let i = 0; i < n; ++i)
            allIdx.push(i)
        out.push({ name: names[0], color: colors[0], cover: "全", tracks: allIdx.slice() })
        for (let p = 1; p < 4; ++p) {
            const tracks = []
            for (let i = p - 1; i < n; i += 3)
                tracks.push(i)
            if (tracks.length === 0)
                tracks.push(0)
            out.push({ name: names[p], color: colors[p], cover: names[p].charAt(0), tracks: tracks })
        }
        return out
    }

    readonly property var currentPlaylist: playlists[playlistIndex]
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
    readonly property string modeIcon: {
        if (MediaSession.playMode === 1)
            return "🔂"
        if (MediaSession.playMode === 2)
            return "🔀"
        return "🔁"
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
        page = "songs"
        applyPlaylistQueue()
    }

    function playSong(trackIndex) {
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
        color: SystemState.page
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: root.page === "player" ? playerBar.top : miniBar.top
        anchors.margins: 16
        anchors.bottomMargin: 8

        Item {
            anchors.fill: parent
            visible: root.page === "playlists"

            Column {
                anchors.fill: parent
                spacing: 14

                Text {
                    text: "音乐"
                    color: SystemState.ink
                    font.pixelSize: 28
                    font.bold: true
                }

                GridView {
                    id: playlistGrid
                    width: parent.width
                    height: parent.height - 48
                    cellWidth: width / 4
                    cellHeight: height / 2
                    clip: true
                    interactive: false
                    model: 8
                    delegate: Item {
                        id: cell
                        required property int index
                        width: playlistGrid.cellWidth
                        height: playlistGrid.cellHeight
                        readonly property var playlist: index < root.playlists.length ? root.playlists[index] : null
                        readonly property int coverSize: Math.min(width - 20, height - 52)

                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            width: cell.coverSize
                            visible: cell.playlist !== null

                            Rectangle {
                                width: cell.coverSize
                                height: cell.coverSize
                                radius: 12
                                color: cell.playlist.color
                                Text {
                                    anchors.centerIn: parent
                                    text: cell.playlist.cover
                                    color: "#FFFFFF"
                                    font.pixelSize: Math.round(cell.coverSize * 0.28)
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.openPlaylist(cell.index)
                                }
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
                    Rectangle {
                        width: 44
                        height: 44
                        radius: 22
                        color: SystemState.fill
                        Text {
                            anchors.centerIn: parent
                            text: "‹"
                            color: SystemState.ink
                            font.pixelSize: 28
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.goBack()
                        }
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

                Rectangle {
                    width: parent.width
                    height: 48
                    radius: 12
                    color: SystemState.card
                    TextField {
                        id: searchField
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        placeholderText: "搜索歌曲"
                        color: SystemState.ink
                        font.pixelSize: 16
                        background: Item {}
                        onTextChanged: root.query = text
                    }
                }

                ListView {
                    width: parent.width
                    height: parent.height - 120
                    clip: true
                    spacing: 8
                    model: root.filteredSongs
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 72
                        radius: 12
                        color: modelData.trackIndex === MediaSession.trackIndex ? "#E5F1FF" : SystemState.card

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 14
                            spacing: 12

                            Rectangle {
                                width: 48
                                height: 48
                                radius: 10
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color || root.currentPlaylist.color
                                Text {
                                    anchors.centerIn: parent
                                    text: root.coverGlyph(modelData.title)
                                    color: "#FFFFFF"
                                    font.pixelSize: 20
                                    font.bold: true
                                }
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4
                                width: parent.width - 130
                                Text {
                                    text: modelData.title
                                    color: SystemState.ink
                                    font.pixelSize: 17
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

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.playSong(modelData.trackIndex)
                        }
                    }
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: root.page === "player"

            Column {
                anchors.fill: parent
                spacing: 12

                Row {
                    width: parent.width
                    spacing: 12

                    Rectangle {
                        width: 44
                        height: 44
                        radius: 22
                        color: SystemState.fill
                        Text {
                            anchors.centerIn: parent
                            text: "‹"
                            color: SystemState.ink
                            font.pixelSize: 28
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.goBack()
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.showLyrics ? "歌词" : "正在播放"
                        color: SystemState.ink
                        font.pixelSize: 22
                        font.bold: true
                    }
                    Item { width: parent.width - 220; height: 1 }
                    Rectangle {
                        width: 72
                        height: 44
                        radius: 22
                        color: SystemState.fill
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.centerIn: parent
                            text: root.showLyrics ? "封面" : "歌词"
                            color: "#007AFF"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.showLyrics = !root.showLyrics
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 56

                    Column {
                        anchors.centerIn: parent
                        spacing: 20
                        width: Math.min(parent.width, 440)
                        visible: !root.showLyrics

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 180
                            height: 180
                            radius: 20
                            color: MediaSession.coverColor
                            Text {
                                anchors.centerIn: parent
                                text: root.coverGlyph(MediaSession.title)
                                color: "#FFFFFF"
                                font.pixelSize: 64
                                font.bold: true
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.showLyrics = true
                            }
                        }
                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 6
                            width: parent.width
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: MediaSession.title
                                color: SystemState.ink
                                font.pixelSize: 26
                                font.bold: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: MediaSession.artist
                                color: SystemState.secondary
                                font.pixelSize: 15
                            }
                        }
                    }

                    ListView {
                        id: lyricView
                        anchors.fill: parent
                        anchors.leftMargin: 24
                        anchors.rightMargin: 24
                        visible: root.showLyrics
                        clip: true
                        spacing: 18
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
                            font.pixelSize: index === root.lyricIndex ? 24 : 17
                            font.bold: index === root.lyricIndex
                            opacity: index === root.lyricIndex ? 1.0 : 0.45
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.showLyrics = false
                        }
                    }
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
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: SystemState.fill
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
                Text {
                    anchors.centerIn: parent
                    text: root.coverGlyph(MediaSession.title)
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
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
            Rectangle {
                width: 48
                height: 48
                radius: 24
                anchors.verticalCenter: parent.verticalCenter
                color: SystemState.fill
                Text {
                    anchors.centerIn: parent
                    text: MediaSession.playing ? "⏸" : "▶"
                    color: "#007AFF"
                    font.pixelSize: 18
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: MediaSession.toggle()
                }
            }
            Rectangle {
                width: 48
                height: 48
                radius: 24
                anchors.verticalCenter: parent.verticalCenter
                color: SystemState.fill
                Text {
                    anchors.centerIn: parent
                    text: "⏭"
                    color: "#007AFF"
                    font.pixelSize: 16
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: MediaSession.next()
                }
            }
        }

        MouseArea {
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

    Rectangle {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 128
        visible: root.page === "player"
        color: SystemState.card

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: SystemState.fill
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Column {
                width: parent.width
                spacing: 2
                Slider {
                    id: seek
                    width: parent.width
                    height: 28
                    from: 0
                    to: Math.max(1, MediaSession.duration)
                    onMoved: MediaSession.seek(Math.round(value))
                    Component.onCompleted: value = MediaSession.position
                    background: Rectangle {
                        x: seek.leftPadding
                        y: seek.topPadding + seek.availableHeight / 2 - height / 2
                        implicitHeight: 6
                        width: seek.availableWidth
                        height: 6
                        radius: 3
                        color: SystemState.fill
                        Rectangle {
                            width: seek.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: "#007AFF"
                        }
                    }
                    handle: Rectangle {
                        x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                        y: seek.topPadding + seek.availableHeight / 2 - height / 2
                        width: 22
                        height: 22
                        radius: 11
                        color: "#FFFFFF"
                        border.color: "#D1D1D6"
                    }
                }
                Row {
                    width: parent.width
                    Text { text: root.mmss(MediaSession.position); color: SystemState.secondary; font.pixelSize: 12 }
                    Item { width: parent.width - 80; height: 1 }
                    Text { text: root.mmss(MediaSession.duration); color: SystemState.secondary; font.pixelSize: 12 }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 22

                Rectangle {
                    width: 52
                    height: 52
                    radius: 26
                    color: SystemState.fill
                    Text {
                        anchors.centerIn: parent
                        text: root.modeIcon
                        font.pixelSize: 20
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: MediaSession.cyclePlayMode()
                    }
                }
                Rectangle {
                    width: 56
                    height: 56
                    radius: 28
                    color: SystemState.fill
                    Text {
                        anchors.centerIn: parent
                        text: "⏮"
                        color: "#007AFF"
                        font.pixelSize: 20
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: MediaSession.previous()
                    }
                }
                Rectangle {
                    width: 64
                    height: 64
                    radius: 32
                    color: "#007AFF"
                    Text {
                        anchors.centerIn: parent
                        text: MediaSession.playing ? "⏸" : "▶"
                        color: "#FFFFFF"
                        font.pixelSize: 24
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: MediaSession.toggle()
                    }
                }
                Rectangle {
                    width: 56
                    height: 56
                    radius: 28
                    color: SystemState.fill
                    Text {
                        anchors.centerIn: parent
                        text: "⏭"
                        color: "#007AFF"
                        font.pixelSize: 20
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: MediaSession.next()
                    }
                }
                Item { width: 52; height: 52 }
            }
        }
    }

    Connections {
        target: MediaSession
        function onPositionChanged() {
            if (root.page === "player" && !seek.pressed)
                seek.value = MediaSession.position
        }
        function onTrackChanged() {
            if (root.page === "player") {
                seek.to = Math.max(1, MediaSession.duration)
                seek.value = MediaSession.position
            }
        }
    }
}
