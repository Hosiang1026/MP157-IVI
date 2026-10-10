import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0
import IviShell

Item {
    id: root
    property int tab: 0
    property string feedUrlDraft: ""

    readonly property bool connecting: RadioSession.status.indexOf("连接") >= 0
        || RadioSession.status.indexOf("重连") >= 0
    readonly property var accentColors: ["#FF2D55", "#5856D6", "#007AFF", "#34C759", "#FF9500", "#AF52DE"]
    readonly property bool rssMode: tab === 1 || tab === 2
    readonly property string rssTitle: tab === 2 ? "听书" : "播客"

    function applyTab(index) {
        tab = index
        if (index === 1)
            PodcastSession.library = "podcast"
        else if (index === 2)
            PodcastSession.library = "book"
    }

    Component.onCompleted: {
        if (PodcastSession.playing && PodcastSession.library === "book")
            tab = 2
        else if (PodcastSession.playing)
            tab = 1
        else if (RadioSession.playing)
            tab = 0
        applyTab(tab)
    }

    Rectangle { anchors.fill: parent; color: "transparent" }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        spacing: 12

        IosSegmented {
            width: parent.width
            height: 40
            labels: ["电台", "播客", "听书"]
            currentIndex: root.tab
            onActivated: function (index) { root.applyTab(index) }
        }

        Item {
            width: parent.width
            height: parent.height - 52

            Row {
                anchors.fill: parent
                spacing: 16
                visible: root.tab === 0

                Rectangle {
                    width: 360
                    height: parent.height
                    radius: 16
                    color: SystemState.card
                    border.width: 1 / Screen.devicePixelRatio
                    border.color: SystemState.separator

                    Column {
                        anchors.centerIn: parent
                        spacing: 18
                        width: parent.width - 40

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "网络电台"
                            color: SystemState.secondary
                            font.pixelSize: 14
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: RadioSession.stationName.length ? RadioSession.stationName : "选择电台"
                            color: SystemState.ink
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 8
                            IosSpinner {
                                width: 16
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                visible: root.connecting || RadioSession.reconnecting
                                running: root.connecting || RadioSession.reconnecting
                                ink: SystemState.tint
                            }
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: RadioSession.playing ? SystemState.success : SystemState.tint
                                visible: RadioSession.status.length > 0 && !root.connecting && !RadioSession.reconnecting
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: RadioSession.status
                                color: RadioSession.playing ? SystemState.success : SystemState.secondary
                                font.pixelSize: 16
                            }
                        }
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: RadioSession.detail
                            color: SystemState.secondary
                            font.pixelSize: 12
                            visible: RadioSession.detail.length > 0
                        }

                        IosPressable {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 88
                            height: 88
                            Rectangle {
                                anchors.fill: parent
                                radius: 44
                                color: RadioSession.playing || RadioSession.reconnecting ? SystemState.danger : SystemState.tint
                            }
                            IosIcon {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                name: RadioSession.playing || RadioSession.reconnecting ? "pause" : "play"
                                ink: "#FFFFFF"
                            }
                            onClicked: RadioSession.toggle()
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 376
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
                            spacing: 12
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "电台列表"
                                color: SystemState.ink
                                font.pixelSize: 18
                                font.bold: true
                            }
                            Item { width: 1; height: 1 }
                            IosSegmented {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 160
                                height: 32
                                labels: ["全部", "收藏"]
                                currentIndex: RadioSession.filterMode
                                onActivated: function (index) { RadioSession.filterMode = index }
                            }
                        }

                        ListView {
                            width: parent.width
                            height: parent.height - 46
                            clip: true
                            model: RadioSession.stations
                            spacing: 0
                            delegate: IosPressable {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                height: 64

                                Rectangle {
                                    anchors.fill: parent
                                    color: RadioSession.currentIndex === index ? SystemState.selected : "transparent"
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 3
                                    height: 28
                                    radius: 1.5
                                    color: root.accentColors[index % root.accentColors.length]
                                    visible: RadioSession.currentIndex === index
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 36
                                    height: 36
                                    radius: 18
                                    color: root.accentColors[index % root.accentColors.length]
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name.length ? modelData.name.charAt(0) : "?"
                                        color: "#FFFFFF"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                }
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 64
                                    anchors.right: favBtn.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: modelData.name
                                        color: RadioSession.currentIndex === index ? SystemState.tint : SystemState.ink
                                        font.pixelSize: 17
                                        font.bold: RadioSession.currentIndex === index
                                    }
                                    Text {
                                        text: modelData.genre
                                        color: SystemState.secondary
                                        font.pixelSize: 12
                                    }
                                }
                                IosPressable {
                                    id: favBtn
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 36
                                    height: 36
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.favorite ? "★" : "☆"
                                        color: modelData.favorite ? "#FF9F0A" : SystemState.secondary
                                        font.pixelSize: 20
                                    }
                                    onClicked: RadioSession.toggleFavorite(index)
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 64
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 0.5
                                    color: SystemState.separator
                                    visible: index < RadioSession.stations.length - 1
                                }
                                onClicked: RadioSession.playIndex(index)
                            }
                        }
                    }

                    IosEmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: RadioSession.stations.length === 0
                        icon: "speaker"
                        title: RadioSession.filterMode === 1 ? "暂无收藏" : "暂无电台"
                        subtitle: RadioSession.filterMode === 1 ? "点星标收藏电台" : "稍后再试或检查网络"
                    }
                }
            }

            Row {
                anchors.fill: parent
                spacing: 16
                visible: root.rssMode

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

                        Text {
                            text: root.tab === 2 ? "书单" : "订阅源"
                            color: SystemState.ink
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Row {
                            width: parent.width
                            spacing: 8
                            TextField {
                                id: feedField
                                width: parent.width - 76
                                height: 36
                                placeholderText: "RSS URL"
                                color: SystemState.ink
                                placeholderTextColor: SystemState.secondary
                                text: root.feedUrlDraft
                                onTextChanged: root.feedUrlDraft = text
                                background: Rectangle { radius: 10; color: SystemState.fill }
                            }
                            IosPressable {
                                width: 68
                                height: 36
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 10
                                    color: SystemState.tint
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "添加"
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                onClicked: {
                                    PodcastSession.addFeed(root.feedUrlDraft)
                                    root.feedUrlDraft = ""
                                    feedField.text = ""
                                }
                            }
                        }

                        ListView {
                            width: parent.width
                            height: parent.height - 90
                            clip: true
                            model: PodcastSession.feeds
                            spacing: 0
                            delegate: IosPressable {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                height: 56
                                Rectangle {
                                    anchors.fill: parent
                                    color: PodcastSession.feedIndex === index ? SystemState.selected : "transparent"
                                }
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: modelData.name
                                        color: PodcastSession.feedIndex === index ? SystemState.tint : SystemState.ink
                                        font.pixelSize: 15
                                        font.bold: PodcastSession.feedIndex === index
                                    }
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: modelData.url
                                        color: SystemState.secondary
                                        font.pixelSize: 11
                                    }
                                }
                                onClicked: PodcastSession.selectFeed(index)
                                onPressAndHold: PodcastSession.removeFeed(index)
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
                            spacing: 12
                            Column {
                                width: parent.width - 160
                                spacing: 4
                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: PodcastSession.feedTitle.length ? PodcastSession.feedTitle : root.rssTitle
                                    color: SystemState.ink
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                                Text {
                                    text: PodcastSession.playing
                                          ? ("播放中 · " + PodcastSession.episodeTitle)
                                          : PodcastSession.status
                                    color: PodcastSession.playing ? SystemState.success : SystemState.secondary
                                    font.pixelSize: 13
                                    width: parent.width
                                    elide: Text.ElideRight
                                }
                            }
                            IosPressable {
                                width: 72
                                height: 36
                                anchors.verticalCenter: parent.verticalCenter
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 10
                                    color: SystemState.fill
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "刷新"
                                    color: SystemState.ink
                                    font.pixelSize: 14
                                }
                                onClicked: PodcastSession.refresh()
                            }
                            IosPressable {
                                width: 72
                                height: 36
                                anchors.verticalCenter: parent.verticalCenter
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 10
                                    color: PodcastSession.playing ? SystemState.danger : SystemState.tint
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: PodcastSession.playing ? "停止" : "播放"
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                onClicked: PodcastSession.toggle()
                            }
                        }

                        ListView {
                            width: parent.width
                            height: parent.height - 56
                            clip: true
                            model: PodcastSession.episodes
                            spacing: 0
                            delegate: IosPressable {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                height: 64
                                Rectangle {
                                    anchors.fill: parent
                                    color: PodcastSession.currentIndex === index ? SystemState.selected : "transparent"
                                }
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: modelData.title
                                        color: PodcastSession.currentIndex === index ? SystemState.tint : SystemState.ink
                                        font.pixelSize: 16
                                        font.bold: PodcastSession.currentIndex === index
                                    }
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: modelData.date || modelData.url
                                        color: SystemState.secondary
                                        font.pixelSize: 12
                                    }
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 0.5
                                    color: SystemState.separator
                                }
                                onClicked: PodcastSession.playIndex(index)
                            }
                        }
                    }

                    IosEmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: !PodcastSession.loading && PodcastSession.episodes.length === 0
                        icon: "speaker"
                        title: root.tab === 2 ? "暂无章节" : "暂无节目"
                        subtitle: "选择左侧源或添加 RSS"
                    }

                    IosSpinner {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        visible: PodcastSession.loading
                        running: PodcastSession.loading
                        ink: SystemState.tint
                    }
                }
            }
        }
    }
}
