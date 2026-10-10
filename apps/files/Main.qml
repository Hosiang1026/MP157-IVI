import QtQuick
import Ivi.Services 1.0
import IviShell

Item {
    id: root
    property string copyName: ""
    property var copyList: []
    property string sheetName: ""
    property bool sheetPlayable: false

    function showCopy(name) {
        sheetName = ""
        copyName = name
        copyList = FileBrowser.copyTargets(name)
    }

    function showSheet(name, playable) {
        sheetName = name
        sheetPlayable = playable
    }

    function hideSheets() {
        sheetName = ""
        copyName = ""
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Row {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        spacing: 14

        Rectangle {
            width: 200
            height: parent.height
            radius: 12
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator

            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: leftCol.height + 16
                clip: true

                Column {
                    id: leftCol
                    x: 12
                    y: 12
                    width: parent.width - 24
                    spacing: 8

                    Text {
                        text: "位置"
                        color: SystemState.secondary
                        font.pixelSize: 13
                    }

                    Repeater {
                        model: FileBrowser.roots
                        delegate: IosPressable {
                            required property var modelData
                            width: leftCol.width
                            height: 52

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: FileBrowser.rootId === modelData.id ? SystemState.selected : "transparent"
                            }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                IosIcon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    name: "folder"
                                    ink: FileBrowser.rootId === modelData.id ? SystemState.tint : SystemState.secondary
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 26
                                    elide: Text.ElideRight
                                    text: modelData.name
                                    color: FileBrowser.rootId === modelData.id ? SystemState.tint : SystemState.ink
                                    font.pixelSize: 15
                                    font.bold: FileBrowser.rootId === modelData.id
                                }
                            }

                            onClicked: FileBrowser.openRoot(modelData.id)
                        }
                    }

                    Item { width: 1; height: 6 }

                    Text {
                        text: "无线传文件"
                        color: SystemState.secondary
                        font.pixelSize: 13
                    }

                    Rectangle {
                        width: parent.width
                        height: FileBrowser.shareRunning ? 132 : 52
                        radius: 10
                        color: SystemState.fill
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: FileBrowser.shareRunning ? FileBrowser.shareUrl : FileBrowser.shareStatus
                                color: FileBrowser.shareRunning ? SystemState.tint : SystemState.ink
                                font.pixelSize: FileBrowser.shareRunning ? 15 : 14
                                font.bold: FileBrowser.shareRunning
                            }
                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                visible: FileBrowser.shareRunning
                                text: FileBrowser.shareStatus
                                color: SystemState.secondary
                                font.pixelSize: 12
                            }
                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                visible: FileBrowser.lastEvent.length > 0
                                text: FileBrowser.lastEvent
                                color: SystemState.success
                                font.pixelSize: 12
                            }
                        }
                    }

                }
            }
        }

        Rectangle {
            width: parent.width - 214
            height: parent.height
            radius: 12
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator
            clip: true

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Row {
                    width: parent.width
                    spacing: 10

                    IosPressable {
                        width: 44
                        height: 36
                        visible: FileBrowser.relativePath.length > 0
                        IosIcon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            name: "back"
                            ink: SystemState.tint
                        }
                        onClicked: FileBrowser.goUp()
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - (FileBrowser.relativePath.length > 0 ? 200 : 152)
                        elide: Text.ElideMiddle
                        text: FileBrowser.rootName
                              + (FileBrowser.relativePath.length ? (" / " + FileBrowser.relativePath) : "")
                        color: SystemState.ink
                        font.pixelSize: 17
                        font.bold: true
                    }

                    IosPressable {
                        width: 80
                        height: 36
                        Text {
                            anchors.centerIn: parent
                            text: FileBrowser.shareRunning ? "关闭传输" : "开启传输"
                            color: FileBrowser.shareRunning ? SystemState.danger : SystemState.tint
                            font.pixelSize: 14
                        }
                        onClicked: {
                            if (FileBrowser.shareRunning)
                                FileBrowser.stopShare()
                            else
                                FileBrowser.startShare()
                        }
                    }

                    IosPressable {
                        width: 56
                        height: 36
                        Text {
                            anchors.centerIn: parent
                            text: "刷新"
                            color: SystemState.tint
                            font.pixelSize: 14
                        }
                        onClicked: FileBrowser.refresh()
                    }
                }

                ListView {
                    width: parent.width
                    height: parent.height - 46
                    clip: true
                    model: FileBrowser.entries
                    spacing: 0
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 56

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                        }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            IosIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 22
                                height: 22
                                name: modelData.dir ? "folder" : "doc"
                                ink: SystemState.tint
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 58
                                spacing: 2
                                Text {
                                    width: parent.width
                                    elide: Text.ElideMiddle
                                    text: modelData.name
                                    color: SystemState.ink
                                    font.pixelSize: 16
                                }
                                Text {
                                    text: modelData.sizeText + " · " + modelData.mtime
                                    color: SystemState.secondary
                                    font.pixelSize: 12
                                }
                            }

                            IosIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                name: "chevron"
                                ink: SystemState.secondary
                                visible: modelData.dir
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 46
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: SystemState.separator
                            visible: index < FileBrowser.entries.length - 1
                        }

                        IosPressable {
                            anchors.fill: parent
                            onClicked: {
                                if (modelData.dir)
                                    FileBrowser.enter(modelData.name)
                                else if (modelData.playable)
                                    FileBrowser.openEntry(modelData.name)
                                else
                                    root.showSheet(modelData.name, false)
                            }
                            onPressAndHold: root.showSheet(modelData.name, modelData.playable)
                        }
                    }

                    IosEmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: FileBrowser.entries.length === 0
                        icon: "folder"
                        title: "空文件夹"
                        subtitle: "此位置没有文件"
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.sheetName.length > 0 || root.copyName.length > 0
        color: "#99000000"
        z: 20
        MouseArea {
            anchors.fill: parent
            onClicked: root.hideSheets()
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.copyName.length > 0
                    ? Math.min(parent.height * 0.7, 100 + root.copyList.length * 48 + 56)
                    : (root.sheetPlayable ? 220 : 176)
            radius: 16
            color: SystemState.card
            border.width: 1 / Screen.devicePixelRatio
            border.color: SystemState.separator
            visible: root.sheetName.length > 0 || root.copyName.length > 0
            MouseArea { anchors.fill: parent }

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                visible: root.sheetName.length > 0 && root.copyName.length === 0

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    text: root.sheetName
                    color: SystemState.secondary
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                }

                IosPressable {
                    width: parent.width
                    height: 48
                    visible: root.sheetPlayable
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: SystemState.fill
                    }
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        IosIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            name: "play"
                            ink: SystemState.tint
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "播放"
                            color: SystemState.tint
                            font.pixelSize: 16
                        }
                    }
                    onClicked: {
                        FileBrowser.openEntry(root.sheetName)
                        root.sheetName = ""
                    }
                }

                IosPressable {
                    width: parent.width
                    height: 48
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: SystemState.fill
                    }
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        IosIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            name: "copy"
                            ink: SystemState.tint
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "拷贝"
                            color: SystemState.tint
                            font.pixelSize: 16
                        }
                    }
                    onClicked: root.showCopy(root.sheetName)
                }

                IosPressable {
                    width: parent.width
                    height: 48
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: SystemState.fill
                    }
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        IosIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            name: "trash"
                            ink: SystemState.danger
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "删除"
                            color: SystemState.danger
                            font.pixelSize: 16
                        }
                    }
                    onClicked: {
                        FileBrowser.removeEntry(root.sheetName)
                        root.sheetName = ""
                    }
                }
            }

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                visible: root.copyName.length > 0

                Text {
                    text: "拷贝到"
                    color: SystemState.ink
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.copyName
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                Flickable {
                    width: parent.width
                    height: Math.max(48, parent.height - 120)
                    contentHeight: copyCol.height
                    clip: true
                    Column {
                        id: copyCol
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: root.copyList
                            delegate: Item {
                                required property var modelData
                                required property int index
                                width: copyCol.width
                                height: 48
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.name
                                    color: SystemState.ink
                                    font.pixelSize: 15
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: SystemState.separator
                                    visible: index < root.copyList.length - 1
                                }
                                IosPressable {
                                    anchors.fill: parent
                                    onClicked: {
                                        FileBrowser.copyEntry(root.copyName, modelData.id)
                                        root.hideSheets()
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: root.copyList.length === 0
                    text: "没有可用目标（可插 U 盘后点刷新）"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }

                IosPressable {
                    width: parent.width
                    height: 52
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: SystemState.fill
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: SystemState.ink
                        font.pixelSize: 16
                    }
                    onClicked: root.hideSheets()
                }
            }
        }
    }

    Component.onDestruction: {
        if (FileBrowser.shareRunning)
            FileBrowser.stopShare()
    }
}
