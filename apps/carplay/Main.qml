import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root
    property string pendingWifiSsid: ""
    property string pendingWifiPass: ""

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    CarPlayVideoItem {
        id: videoSurface
        anchors.fill: parent
        visible: CarPlaySession.hasVideo
        z: 10
        session: CarPlaySession
    }

    MultiPointTouchArea {
        anchors.fill: parent
        visible: CarPlaySession.hasVideo
        z: 20
        maximumTouchPoints: 1
        property real lastX: 0.5
        property real lastY: 0.5

        function send(pt, down) {
            const w = Math.max(1, width)
            const h = Math.max(1, height)
            const x = Math.min(1, Math.max(0, pt.x / w))
            const y = Math.min(1, Math.max(0, pt.y / h))
            lastX = x
            lastY = y
            CarPlaySession.sendTouch(x, y, down)
        }

        onPressed: function(points) {
            if (points.length > 0)
                send(points[0], true)
        }
        onUpdated: function(points) {
            if (points.length > 0)
                send(points[0], true)
        }
        onReleased: function(points) {
            if (points.length > 0)
                send(points[0], false)
            else
                CarPlaySession.sendTouch(lastX, lastY, false)
        }
        onCanceled: function() {
            CarPlaySession.sendTouch(lastX, lastY, false)
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            propagateComposedEvents: false
            property real lastX: 0.5
            property real lastY: 0.5
            function send(mx, my, down) {
                const w = Math.max(1, width)
                const h = Math.max(1, height)
                const x = Math.min(1, Math.max(0, mx / w))
                const y = Math.min(1, Math.max(0, my / h))
                lastX = x
                lastY = y
                CarPlaySession.sendTouch(x, y, down)
            }
            onPressed: function(mouse) { send(mouse.x, mouse.y, true) }
            onPositionChanged: function(mouse) {
                if (pressed)
                    send(mouse.x, mouse.y, true)
            }
            onReleased: function(mouse) { send(mouse.x, mouse.y, false) }
            onCanceled: { CarPlaySession.sendTouch(lastX, lastY, false) }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        visible: !CarPlaySession.hasVideo
        contentHeight: col.height
        clip: true

        Column {
            id: col
            width: parent.width
            spacing: 12

            Text {
                text: "CarPlay 无线"
                color: "#FFFFFF"
                font.pixelSize: 30
                font.bold: true
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: CarPlaySession.status
                color: CarPlaySession.identityReady ? "#34C759" : "#FF453A"
                font.pixelSize: 18
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: CarPlaySession.detail
                color: "#AEAEB2"
                font.pixelSize: 14
            }

            Row {
                spacing: 10
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "手机（蓝牙）"
                    color: "#FFFFFF"
                    font.pixelSize: 16
                    font.bold: true
                }
                Rectangle {
                    width: 72
                    height: 30
                    radius: 8
                    color: "#3A3A3C"
                    Text { anchors.centerIn: parent; text: "刷新"; color: "#FFF"; font.pixelSize: 13 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: CarPlaySession.refreshBluetooth()
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: CarPlaySession.bluetoothAddress.length > 0
                text: "已选：" + (CarPlaySession.bluetoothName.length ? CarPlaySession.bluetoothName + " · " : "")
                      + CarPlaySession.bluetoothAddress
                color: "#0A84FF"
                font.pixelSize: 14
            }

            Repeater {
                model: CarPlaySession.bluetoothDevices
                delegate: Rectangle {
                    id: btRow
                    required property var modelData
                    width: col.width
                    height: 52
                    radius: 10
                    color: String(modelData.address).toUpperCase() === CarPlaySession.bluetoothAddress
                           ? "#0A84FF" : "#1C1C1E"
                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 100
                            Text {
                                text: modelData.name && String(modelData.name).length
                                      ? modelData.name : "未知设备"
                                color: "#FFFFFF"
                                font.pixelSize: 15
                                elide: Text.ElideRight
                                width: parent.width
                            }
                            Text {
                                text: modelData.address
                                      + (modelData.paired ? " · 已配对" : " · 未配对")
                                      + (modelData.connected ? " · 链路中" : "")
                                color: "#AEAEB2"
                                font.pixelSize: 12
                            }
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 72
                            height: 32
                            radius: 8
                            color: "#3A3A3C"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.paired ? "选择" : "配对"
                                color: "#FFFFFF"
                                font.pixelSize: 14
                            }
                            MouseArea {
                                anchors.fill: parent
                                z: 2
                                onClicked: {
                                    const addr = String(btRow.modelData.address)
                                    const name = String(btRow.modelData.name || "")
                                    if (btRow.modelData.paired)
                                        CarPlaySession.selectBluetooth(addr, name)
                                    else
                                        CarPlaySession.pairBluetooth(addr)
                                }
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        z: 1
                        onClicked: {
                            const addr = String(btRow.modelData.address)
                            const name = String(btRow.modelData.name || "")
                            if (btRow.modelData.paired)
                                CarPlaySession.selectBluetooth(addr, name)
                            else
                                CarPlaySession.pairBluetooth(addr)
                        }
                    }
                }
            }

            Item { width: 1; height: 4 }

            Row {
                spacing: 10
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Wi‑Fi"
                    color: "#FFFFFF"
                    font.pixelSize: 16
                    font.bold: true
                }
                Rectangle {
                    width: 72
                    height: 30
                    radius: 8
                    color: "#3A3A3C"
                    Text { anchors.centerIn: parent; text: "刷新"; color: "#FFF"; font.pixelSize: 13 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: CarPlaySession.refreshWifi()
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: CarPlaySession.wifiConnected
                text: "已连接：" + CarPlaySession.wifiSsid
                color: "#34C759"
                font.pixelSize: 14
            }

            Column {
                width: parent.width
                spacing: 8
                visible: CarPlaySession.wifiConnected
                Text {
                    text: "Wi‑Fi 密码（发给手机，必填）"
                    color: "#8E8E93"
                    font.pixelSize: 13
                }
                TextField {
                    width: parent.width
                    height: 40
                    echoMode: TextInput.Password
                    placeholderText: "与手机同一 Wi‑Fi 的密码"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    text: CarPlaySession.wifiPassword
                    background: Rectangle { color: "#1C1C1E"; radius: 8 }
                    onTextChanged: CarPlaySession.wifiPassword = text
                }
                Text {
                    text: "手机 IP"
                    color: "#8E8E93"
                    font.pixelSize: 13
                }
                TextField {
                    width: parent.width
                    height: 40
                    placeholderText: "例如 192.168.2.101"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    text: CarPlaySession.phoneIp
                    background: Rectangle { color: "#1C1C1E"; radius: 8 }
                    onTextChanged: CarPlaySession.phoneIp = text
                }
            }

            Column {
                width: parent.width
                spacing: 8
                visible: !CarPlaySession.wifiConnected

                Repeater {
                    model: CarPlaySession.wifiNetworks
                    delegate: Rectangle {
                        required property var modelData
                        width: col.width
                        height: 48
                        radius: 10
                        color: modelData.ssid === root.pendingWifiSsid || modelData.ssid === CarPlaySession.wifiSsid
                               ? "#0A84FF" : "#1C1C1E"
                        Row {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 70
                                Text {
                                    text: modelData.ssid
                                    color: "#FFFFFF"
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: (modelData.secured ? "加密" : "开放") + " · 信号 " + modelData.signal
                                    color: "#AEAEB2"
                                    font.pixelSize: 12
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "选择"
                                color: "#FFFFFF"
                                font.pixelSize: 14
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.pendingWifiSsid = modelData.ssid
                                root.pendingWifiPass = ""
                                CarPlaySession.selectWifi(modelData.ssid)
                            }
                        }
                    }
                }

                Text {
                    visible: root.pendingWifiSsid.length > 0
                    text: "密码：" + root.pendingWifiSsid
                    color: "#8E8E93"
                    font.pixelSize: 13
                }

                TextField {
                    width: parent.width
                    height: 40
                    visible: root.pendingWifiSsid.length > 0
                    echoMode: TextInput.Password
                    placeholderText: "开放网络可留空"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    background: Rectangle { color: "#1C1C1E"; radius: 8 }
                    onTextChanged: root.pendingWifiPass = text
                }

                Rectangle {
                    width: 140
                    height: 40
                    radius: 10
                    visible: root.pendingWifiSsid.length > 0
                    color: "#0A84FF"
                    Text { anchors.centerIn: parent; text: "连接此网络"; color: "#FFF"; font.pixelSize: 15 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (CarPlaySession.connectWifi(root.pendingWifiSsid, root.pendingWifiPass))
                                root.pendingWifiSsid = ""
                        }
                    }
                }
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: !CarPlaySession.canStart && !CarPlaySession.running
                text: !CarPlaySession.identityReady ? "MFi 身份未就绪"
                      : !CarPlaySession.wifiConnected ? "请先连接 Wi‑Fi"
                      : CarPlaySession.wifiPassword.length === 0 ? "请填写当前 Wi‑Fi 密码"
                      : CarPlaySession.bluetoothAddress.length === 0 ? "请先选择已配对的手机"
                      : "请确认手机已配对后再开始"
                color: "#FF9F0A"
                font.pixelSize: 13
            }

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: CarPlaySession.running
                text: "正在连接 CarPlay，可点「断开」取消"
                color: "#FF9F0A"
                font.pixelSize: 13
            }

            Row {
                spacing: 12
                Rectangle {
                    width: 140
                    height: 44
                    radius: 10
                    color: CarPlaySession.canStart ? "#0A84FF" : "#3A3A3C"
                    Text {
                        anchors.centerIn: parent
                        text: CarPlaySession.running ? "连接中…" : "开始 CarPlay"
                        color: "#FFF"
                        font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: CarPlaySession.canStart
                        onClicked: CarPlaySession.start()
                    }
                }
                Rectangle {
                    width: 100
                    height: 44
                    radius: 10
                    color: CarPlaySession.running ? "#FF453A" : "#3A3A3C"
                    Text { anchors.centerIn: parent; text: "断开"; color: "#FFF"; font.pixelSize: 16 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: CarPlaySession.stop()
                    }
                }
            }

            Component.onCompleted: {
                CarPlaySession.refreshWifi()
                CarPlaySession.refreshBluetooth()
            }
        }
    }
}
