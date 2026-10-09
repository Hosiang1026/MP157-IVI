import QtQuick
import QtQuick.Controls
import Ivi.Services 1.0

Item {
    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + 28
        clip: true

        Column {
            id: column
            x: 16
            y: 12
            width: parent.width - 32
            spacing: 8

            Text { text: "设置"; color: SystemState.ink; font.pixelSize: 28; font.bold: true }

            Text { text: "天气调试"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: weatherDebug.implicitHeight + 20
                radius: 12
                color: SystemState.card
                Flow {
                    id: weatherDebug
                    x: 12
                    y: 10
                    width: parent.width - 24
                    spacing: 8
                    Repeater {
                        model: [
                            { mode: "", label: "实况" },
                            { mode: "clear", label: "晴" },
                            { mode: "night", label: "晴夜" },
                            { mode: "cloudy", label: "多云" },
                            { mode: "overcast", label: "阴" },
                            { mode: "fog", label: "雾" },
                            { mode: "haze", label: "霾" },
                            { mode: "dust", label: "沙尘" },
                            { mode: "sandLift", label: "扬沙" },
                            { mode: "wind", label: "大风" },
                            { mode: "blizzard", label: "暴雪" },
                            { mode: "frost", label: "霜" },
                            { mode: "ponding", label: "积水" },
                            { mode: "wetRoad", label: "湿滑" },
                            { mode: "typhoon", label: "台风" },
                            { mode: "rain", label: "小雨" },
                            { mode: "rainMid", label: "中雨" },
                            { mode: "rainHard", label: "暴雨" },
                            { mode: "thunder", label: "雷雨" },
                            { mode: "sleet", label: "雨夹雪" },
                            { mode: "freezeRain", label: "冻雨" },
                            { mode: "snow", label: "小雪" },
                            { mode: "snowMid", label: "中雪" },
                            { mode: "snowHard", label: "大雪" },
                            { mode: "hail", label: "冰雹" }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            width: 72
                            height: 32
                            radius: 8
                            color: Weather.preview === modelData.mode ? "#007AFF" : SystemState.fill
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: Weather.preview === modelData.mode ? "#FFFFFF" : SystemState.ink
                                font.pixelSize: 14
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: Weather.setPreview(modelData.mode)
                            }
                        }
                    }
                }
            }

            Text { text: "连接"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: 96
                radius: 12
                color: SystemState.card
                Column {
                    anchors.fill: parent
                    Rectangle {
                        width: parent.width
                        height: 48
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "蓝牙"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 51
                            height: 31
                            radius: 16
                            color: SystemState.bluetooth ? "#34C759" : "#E5E5EA"
                            Rectangle {
                                width: 27
                                height: 27
                                radius: 14
                                color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                x: SystemState.bluetooth ? 22 : 2
                            }
                            MouseArea { anchors.fill: parent; onClicked: SystemState.bluetooth = !SystemState.bluetooth }
                        }
                    }
                    Rectangle { x: 16; width: parent.width - 16; height: 1; color: SystemState.fill }
                    Rectangle {
                        width: parent.width
                        height: 47
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "无线局域网"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: wifiSwitch.left
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: !SystemState.wifi ? "关闭" : (SystemState.wifiName.length > 0 ? SystemState.wifiName : "未连接")
                            color: SystemState.secondary
                            font.pixelSize: 15
                        }
                        Rectangle {
                            id: wifiSwitch
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 51
                            height: 31
                            radius: 16
                            color: SystemState.wifi ? "#34C759" : "#E5E5EA"
                            Rectangle {
                                width: 27
                                height: 27
                                radius: 14
                                color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                x: SystemState.wifi ? 22 : 2
                            }
                            MouseArea { anchors.fill: parent; onClicked: SystemState.wifi = !SystemState.wifi }
                        }
                    }
                }
            }

            Text { text: "外观"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: SystemState.autoTheme ? 48 : 97
                radius: 12
                color: SystemState.card
                clip: true
                Column {
                    width: parent.width
                    Rectangle {
                        width: parent.width
                        height: 48
                        color: "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "自动"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Text {
                            anchors.right: autoSwitch.left
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: SystemState.dark ? "深色" : "浅色"
                            color: SystemState.secondary
                            font.pixelSize: 15
                        }
                        Rectangle {
                            id: autoSwitch
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 51
                            height: 31
                            radius: 16
                            color: SystemState.autoTheme ? "#34C759" : SystemState.fill
                            Rectangle {
                                width: 27
                                height: 27
                                radius: 14
                                color: "#FFFFFF"
                                anchors.verticalCenter: parent.verticalCenter
                                x: SystemState.autoTheme ? 22 : 2
                            }
                            MouseArea { anchors.fill: parent; onClicked: SystemState.autoTheme = !SystemState.autoTheme }
                        }
                    }
                    Rectangle {
                        x: 16
                        width: parent.width - 16
                        height: 1
                        color: SystemState.fill
                        visible: !SystemState.autoTheme
                    }
                    Rectangle {
                        width: parent.width
                        height: 48
                        color: "transparent"
                        visible: !SystemState.autoTheme
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "手动"
                            color: SystemState.ink
                            font.pixelSize: 16
                        }
                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Rectangle {
                                width: 64
                                height: 30
                                radius: 8
                                color: SystemState.manualDark ? SystemState.fill : "#007AFF"
                                Text {
                                    anchors.centerIn: parent
                                    text: "浅色"
                                    color: SystemState.manualDark ? SystemState.ink : "#FFFFFF"
                                    font.pixelSize: 13
                                }
                                MouseArea { anchors.fill: parent; onClicked: SystemState.manualDark = false }
                            }
                            Rectangle {
                                width: 64
                                height: 30
                                radius: 8
                                color: SystemState.manualDark ? "#007AFF" : SystemState.fill
                                Text {
                                    anchors.centerIn: parent
                                    text: "深色"
                                    color: SystemState.manualDark ? "#FFFFFF" : SystemState.ink
                                    font.pixelSize: 13
                                }
                                MouseArea { anchors.fill: parent; onClicked: SystemState.manualDark = true }
                            }
                        }
                    }
                }
            }

            Text { text: "亮度"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: 52
                radius: 12
                color: SystemState.card
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "亮度"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Text {
                    anchors.right: brightnessSlider.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(SystemState.brightness * 100) + "%"
                    color: SystemState.secondary
                    font.pixelSize: 14
                }
                Slider {
                    id: brightnessSlider
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 280
                    from: 0.15
                    to: 1
                    Component.onCompleted: value = SystemState.brightness
                    onMoved: SystemState.brightness = value
                }
            }

            Text { text: "声音"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: 52
                radius: 12
                color: SystemState.card
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "音量"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Slider {
                    id: volumeSlider
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 280
                    from: 0
                    to: 1
                    Component.onCompleted: value = SystemState.volume
                    onMoved: SystemState.volume = value
                }
            }

            Text { text: "通用"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: 48
                radius: 12
                color: SystemState.card
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "关于本机"
                    color: SystemState.ink
                    font.pixelSize: 16
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "MP157 IVI  0.1"
                    color: SystemState.secondary
                    font.pixelSize: 15
                }
            }

            Text { text: "壁纸"; color: SystemState.secondary; font.pixelSize: 13; leftPadding: 12 }
            Rectangle {
                width: parent.width
                height: wallGrid.implicitHeight + 58
                radius: 12
                color: SystemState.card
                Grid {
                    id: wallGrid
                    x: 12
                    y: 12
                    width: parent.width - 24
                    columns: 5
                    columnSpacing: 8
                    rowSpacing: 8
                    Repeater {
                        model: WallpaperStore.items
                        delegate: Item {
                            required property var modelData
                            width: (wallGrid.width - 32) / 5
                            height: width * 0.62
                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                source: modelData.path
                                fillMode: Image.PreserveAspectCrop
                            }
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                radius: 8
                                border.width: 2
                                border.color: WallpaperStore.current === modelData.path ? "#007AFF" : "transparent"
                            }
                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.name
                                color: "#FFFFFF"
                                font.pixelSize: 11
                                style: Text.Outline
                                styleColor: "#99000000"
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: WallpaperStore.select(modelData.path)
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    width: 96
                    height: 30
                    radius: 15
                    color: "#007AFF"
                    Text { anchors.centerIn: parent; text: "上传壁纸"; color: "#FFFFFF"; font.pixelSize: 13 }
                    MouseArea { anchors.fill: parent; onClicked: WallpaperStore.upload() }
                }
            }
        }
    }

    Connections {
        target: SystemState
        function onBrightnessChanged() { brightnessSlider.value = SystemState.brightness }
        function onVolumeChanged() { volumeSlider.value = SystemState.volume }
    }
}
