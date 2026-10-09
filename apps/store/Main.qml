import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property string selectedId: "radio"
    property string selectedName: "电台"
    property string selectedColor: "#FF9F0A"
    property bool selectedInstalled: false
    property bool selectedBuiltin: false

    function blurb(id) {
        if (id === "radio")
            return "收听 FM / AM，支持预设电台和搜台"
        if (id === "music")
            return "播放列表、进度和蓝牙音频"
        if (id === "phone")
            return "拨号、联系人与最近通话"
        if (id === "vehicle")
            return "车速、档位、油量和胎压"
        if (id === "settings")
            return "亮度、音量、蓝牙和无线局域网"
        if (id === "store")
            return "获取和移除车机应用"
        return "车机应用"
    }

    function select(id, name, color, installed, builtin) {
        selectedId = id
        selectedName = name
        selectedColor = color
        selectedInstalled = installed
        selectedBuiltin = builtin
    }

    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Row {
        anchors.fill: parent
        anchors.margins: 16
        anchors.bottomMargin: 8
        spacing: 16

        Rectangle {
            width: 340
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 10
                Text { text: "App Store"; color: SystemState.ink; font.pixelSize: 28; font.bold: true }
                Rectangle {
                    width: 72
                    height: 72
                    radius: 16
                    color: root.selectedColor
                    Text {
                        anchors.centerIn: parent
                        text: root.selectedName.length > 0 ? root.selectedName.charAt(0) : ""
                        color: "#FFFFFF"
                        font.pixelSize: 28
                        font.bold: true
                    }
                }
                Text { text: root.selectedName; color: SystemState.ink; font.pixelSize: 22; font.bold: true }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.blurb(root.selectedId)
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
                Text { text: "版本 1.0 · 12 MB"; color: SystemState.secondary; font.pixelSize: 12 }
                Rectangle {
                    width: 72
                    height: 28
                    radius: 14
                    visible: !root.selectedBuiltin
                    color: root.selectedInstalled ? "#E5E5EA" : "#007AFF"
                    Text {
                        anchors.centerIn: parent
                        text: root.selectedInstalled ? "移除" : "获取"
                        color: root.selectedInstalled ? "#FF3B30" : "#FFFFFF"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (root.selectedInstalled) {
                                if (AppCatalog.uninstall(root.selectedId))
                                    root.selectedInstalled = false
                            } else if (AppCatalog.install(root.selectedId)) {
                                root.selectedInstalled = true
                            }
                        }
                    }
                }
                Text {
                    visible: root.selectedBuiltin
                    text: "系统应用"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            width: parent.width - 356
            height: parent.height
            radius: 12
            color: SystemState.card
            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                Text { text: "精选"; color: SystemState.ink; font.pixelSize: 20; font.bold: true }
                ListView {
                    id: availableList
                    width: parent.width
                    height: Math.min(140, Math.max(44, contentHeight))
                    clip: true
                    model: AppCatalog.available
                    delegate: Rectangle {
                        required property string appId
                        required property string name
                        required property string color
                        required property int index
                        width: availableList.width
                        height: 64
                        color: "transparent"
                        Rectangle {
                            width: 44
                            height: 44
                            radius: 10
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            color: color
                            Text {
                                anchors.centerIn: parent
                                text: name.charAt(0)
                                color: "#FFFFFF"
                                font.bold: true
                            }
                        }
                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 56
                            anchors.verticalCenter: parent.verticalCenter
                            Text { text: name; color: SystemState.ink; font.pixelSize: 16 }
                            Text { text: root.blurb(appId); color: SystemState.secondary; font.pixelSize: 12 }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 58
                            height: 26
                            radius: 13
                            color: SystemState.highlight
                            Text { anchors.centerIn: parent; text: "获取"; color: "#007AFF"; font.pixelSize: 13; font.bold: true }
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.select(appId, name, color, false, false) }
                        Component.onCompleted: if (index === 0) root.select(appId, name, color, false, false)
                    }
                }
                Text {
                    visible: AppCatalog.available.count === 0
                    text: "没有可获取的应用"
                    color: SystemState.secondary
                    font.pixelSize: 13
                }
                Text { text: "已安装"; color: SystemState.ink; font.pixelSize: 20; font.bold: true }
                ListView {
                    id: installedList
                    width: parent.width
                    height: parent.parent.height - 210
                    clip: true
                    model: AppCatalog.installed
                    delegate: Rectangle {
                        required property string appId
                        required property string name
                        required property string color
                        required property bool builtin
                        width: installedList.width
                        height: 52
                        color: root.selectedId === appId ? "#E5F1FF" : "transparent"
                        radius: 8
                        Rectangle {
                            width: 36
                            height: 36
                            radius: 8
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: color
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 50
                            anchors.verticalCenter: parent.verticalCenter
                            text: name
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: builtin ? "系统" : "已获取"
                            color: SystemState.secondary
                            font.pixelSize: 13
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.select(appId, name, color, true, builtin) }
                    }
                }
            }
        }
    }
}
