import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    id: root
    property string pendingWifiSsid: ""
    property string pendingWifiPass: ""
    property bool showConnectActions: false

    function tryAutoConnect() {
        CarPlaySession.reconnectLast()
        root.showConnectActions = !CarPlaySession.running
    }

    Connections {
        target: CarPlaySession
        function onRunningChanged() {
            if (!CarPlaySession.running)
                root.showConnectActions = true
        }
    }

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
        id: connectPage
        anchors.fill: parent
        visible: !CarPlaySession.hasVideo
        contentWidth: width
        contentHeight: col.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: col
            y: 20
            width: Math.min(720, connectPage.width - 56)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 18

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: "CarPlay"
                    color: "#FFFFFF"
                    font.pixelSize: 32
                    font.bold: true
                    font.letterSpacing: 0.5
                }
                Text {
                    text: "无线连接"
                    color: "#8E8E93"
                    font.pixelSize: 15
                }
            }

            Rectangle {
                width: parent.width
                height: statusCol.height + 28
                radius: 16
                color: "#141416"
                border.color: "#222226"
                border.width: 1

                Column {
                    id: statusCol
                    x: 18
                    y: 14
                    width: parent.width - 36
                    spacing: 8

                    Row {
                        spacing: 10
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: CarPlaySession.running ? "#FF9F0A"
                                   : CarPlaySession.identityReady ? "#34C759" : "#FF453A"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: CarPlaySession.status
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: CarPlaySession.detail
                        color: "#8E8E93"
                        font.pixelSize: 13
                        visible: text.length > 0
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 12
                visible: root.showConnectActions || CarPlaySession.running

                Rectangle {
                    width: parent.width - 120
                    height: 50
                    radius: 14
                    visible: root.showConnectActions || CarPlaySession.running
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: CarPlaySession.canStart ? "#0A84FF" : "#3A3A3C"
                        }
                        GradientStop {
                            position: 1.0
                            color: CarPlaySession.canStart ? "#0066D6" : "#2C2C2E"
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: CarPlaySession.running ? "连接中…" : "开始 CarPlay"
                        color: "#FFFFFF"
                        font.pixelSize: 17
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: CarPlaySession.canStart
                        onClicked: CarPlaySession.start()
                    }
                }
                Rectangle {
                    width: 108
                    height: 50
                    radius: 14
                    color: CarPlaySession.running ? "#FF453A" : "#2C2C2E"
                    Text {
                        anchors.centerIn: parent
                        text: "断开"
                        color: CarPlaySession.running ? "#FFFFFF" : "#8E8E93"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            CarPlaySession.stop()
                            root.showConnectActions = true
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 40
                radius: 10
                color: "#2A1F0A"
                visible: (root.showConnectActions || CarPlaySession.running)
                         && ((!CarPlaySession.canStart && !CarPlaySession.running) || CarPlaySession.running)
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    text: CarPlaySession.running
                          ? "正在连接 CarPlay，可点「断开」取消"
                          : !CarPlaySession.identityReady ? "MFi 身份未就绪"
                          : !CarPlaySession.wifiConnected ? "请先连接 Wi‑Fi"
                          : CarPlaySession.wifiPassword.length === 0 ? "请填写当前 Wi‑Fi 密码"
                          : CarPlaySession.bluetoothAddress.length === 0 ? "请先选择已配对的手机"
                          : "请确认手机已配对后再开始"
                    color: "#FF9F0A"
                    font.pixelSize: 13
                }
            }

            Column {
                width: parent.width
                spacing: 8

                Row {
                    width: parent.width
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "手机"
                        color: "#8E8E93"
                        font.pixelSize: 13
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Item { width: parent.width - 120; height: 1 }
                    Rectangle {
                        width: 64
                        height: 28
                        radius: 14
                        color: "#2C2C2E"
                        Text {
                            anchors.centerIn: parent
                            text: "刷新"
                            color: "#0A84FF"
                            font.pixelSize: 13
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: CarPlaySession.refreshBluetooth()
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 16
                    color: "#141416"
                    border.color: "#222226"
                    border.width: 1
                    height: btInner.height

                    Column {
                        id: btInner
                        width: parent.width

                        Rectangle {
                            width: parent.width
                            height: 44
                            color: "transparent"
                            visible: CarPlaySession.bluetoothAddress.length > 0
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 16
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 32
                                elide: Text.ElideRight
                                text: "已选  " + (CarPlaySession.bluetoothName.length
                                      ? CarPlaySession.bluetoothName + "  ·  " : "")
                                      + CarPlaySession.bluetoothAddress
                                color: "#0A84FF"
                                font.pixelSize: 13
                            }
                        }
                        Rectangle {
                            width: parent.width - 32
                            height: 1
                            x: 16
                            color: "#222226"
                            visible: CarPlaySession.bluetoothAddress.length > 0
                                     && CarPlaySession.bluetoothDevices.length > 0
                        }

                        Repeater {
                            model: CarPlaySession.bluetoothDevices
                            delegate: Rectangle {
                                id: btRow
                                required property var modelData
                                required property int index
                                width: btInner.width
                                height: 56
                                color: String(modelData.address).toUpperCase() === CarPlaySession.bluetoothAddress
                                       ? "#1A3A5C" : "transparent"

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 36
                                    height: 36
                                    radius: 18
                                    color: String(btRow.modelData.address).toUpperCase() === CarPlaySession.bluetoothAddress
                                           ? "#0A84FF" : "#2C2C2E"
                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            const n = String(btRow.modelData.name || "?")
                                            return n.length ? n.charAt(0).toUpperCase() : "?"
                                        }
                                        color: "#FFFFFF"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                }

                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 64
                                    anchors.right: actionBtn.left
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text {
                                        width: parent.width
                                        text: btRow.modelData.name && String(btRow.modelData.name).length
                                              ? btRow.modelData.name : "未知设备"
                                        color: "#FFFFFF"
                                        font.pixelSize: 15
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: btRow.modelData.address
                                              + (btRow.modelData.paired ? "  ·  已配对" : "  ·  未配对")
                                              + (btRow.modelData.connected ? "  ·  链路中" : "")
                                        color: "#8E8E93"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle {
                                    id: actionBtn
                                    anchors.right: parent.right
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 64
                                    height: 30
                                    radius: 15
                                    color: btRow.modelData.paired ? "#0A84FF" : "#3A3A3C"
                                    Text {
                                        anchors.centerIn: parent
                                        text: btRow.modelData.paired ? "选择" : "配对"
                                        color: "#FFFFFF"
                                        font.pixelSize: 13
                                        font.bold: true
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

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 64
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: "#222226"
                                    visible: btRow.index < CarPlaySession.bluetoothDevices.length - 1
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

                        Item {
                            width: btInner.width
                            height: 48
                            visible: CarPlaySession.bluetoothDevices.length === 0
                            Text {
                                anchors.centerIn: parent
                                text: "暂无设备，点刷新扫描"
                                color: "#636366"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8

                Row {
                    width: parent.width
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Wi‑Fi"
                        color: "#8E8E93"
                        font.pixelSize: 13
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Item { width: parent.width - 120; height: 1 }
                    Rectangle {
                        width: 64
                        height: 28
                        radius: 14
                        color: "#2C2C2E"
                        Text {
                            anchors.centerIn: parent
                            text: "刷新"
                            color: "#0A84FF"
                            font.pixelSize: 13
                            font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: CarPlaySession.refreshWifi()
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    radius: 16
                    color: "#141416"
                    border.color: "#222226"
                    border.width: 1
                    height: wifiInner.height

                    Column {
                        id: wifiInner
                        width: parent.width
                        spacing: 0

                        Rectangle {
                            width: parent.width
                            height: 44
                            color: "transparent"
                            visible: CarPlaySession.wifiConnected
                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 16
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                Rectangle {
                                    width: 7
                                    height: 7
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: "#34C759"
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "已连接  " + CarPlaySession.wifiSsid
                                    color: "#34C759"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            visible: CarPlaySession.wifiConnected
                            spacing: 0

                            Rectangle {
                                width: parent.width - 32
                                height: 1
                                x: 16
                                color: "#222226"
                            }

                            Column {
                                x: 16
                                width: parent.width - 32
                                spacing: 10
                                topPadding: 14
                                bottomPadding: 16

                                Text {
                                    text: "Wi‑Fi 密码（发给手机）"
                                    color: "#8E8E93"
                                    font.pixelSize: 12
                                }
                                TextField {
                                    width: parent.width
                                    height: 42
                                    echoMode: TextInput.Password
                                    placeholderText: "与手机同一 Wi‑Fi 的密码"
                                    color: "#FFFFFF"
                                    font.pixelSize: 15
                                    leftPadding: 12
                                    rightPadding: 12
                                    text: CarPlaySession.wifiPassword
                                    background: Rectangle {
                                        color: "#1C1C1E"
                                        radius: 10
                                        border.color: "#2C2C2E"
                                        border.width: 1
                                    }
                                    onTextChanged: CarPlaySession.wifiPassword = text
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            visible: !CarPlaySession.wifiConnected
                            spacing: 0

                            Repeater {
                                model: CarPlaySession.wifiNetworks
                                delegate: Rectangle {
                                    id: wifiRow
                                    required property var modelData
                                    required property int index
                                    width: wifiInner.width
                                    height: 54
                                    color: modelData.ssid === root.pendingWifiSsid
                                           || modelData.ssid === CarPlaySession.wifiSsid
                                           ? "#1A3A5C" : "transparent"

                                    Column {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 16
                                        anchors.right: parent.right
                                        anchors.rightMargin: 56
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 2
                                        Text {
                                            width: parent.width
                                            text: wifiRow.modelData.ssid
                                            color: "#FFFFFF"
                                            font.pixelSize: 15
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: (wifiRow.modelData.secured ? "加密" : "开放")
                                                  + "  ·  信号 " + wifiRow.modelData.signal
                                            color: "#8E8E93"
                                            font.pixelSize: 12
                                        }
                                    }
                                    Text {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 16
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: wifiRow.modelData.ssid === root.pendingWifiSsid ? "✓" : ""
                                        color: "#0A84FF"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 16
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        color: "#222226"
                                        visible: wifiRow.index < CarPlaySession.wifiNetworks.length - 1
                                                 || root.pendingWifiSsid.length > 0
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            root.pendingWifiSsid = wifiRow.modelData.ssid
                                            root.pendingWifiPass = ""
                                            CarPlaySession.selectWifi(wifiRow.modelData.ssid)
                                        }
                                    }
                                }
                            }

                            Item {
                                width: wifiInner.width
                                height: 48
                                visible: CarPlaySession.wifiNetworks.length === 0
                                Text {
                                    anchors.centerIn: parent
                                    text: "暂无网络，点刷新扫描"
                                    color: "#636366"
                                    font.pixelSize: 13
                                }
                            }

                            Column {
                                x: 16
                                width: parent.width - 32
                                spacing: 10
                                topPadding: 14
                                bottomPadding: 16
                                visible: root.pendingWifiSsid.length > 0

                                Text {
                                    text: "连接  " + root.pendingWifiSsid
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                TextField {
                                    width: parent.width
                                    height: 42
                                    echoMode: TextInput.Password
                                    placeholderText: "开放网络可留空"
                                    color: "#FFFFFF"
                                    font.pixelSize: 15
                                    leftPadding: 12
                                    rightPadding: 12
                                    background: Rectangle {
                                        color: "#1C1C1E"
                                        radius: 10
                                        border.color: "#2C2C2E"
                                        border.width: 1
                                    }
                                    onTextChanged: root.pendingWifiPass = text
                                }
                                Rectangle {
                                    width: parent.width
                                    height: 42
                                    radius: 12
                                    color: "#0A84FF"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "连接此网络"
                                        color: "#FFFFFF"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            if (CarPlaySession.connectWifi(root.pendingWifiSsid, root.pendingWifiPass))
                                                root.pendingWifiSsid = ""
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Component.onCompleted: root.tryAutoConnect()
        }
    }
}
