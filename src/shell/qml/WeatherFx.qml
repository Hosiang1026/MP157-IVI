import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property var wallpaper
    property real pagePos: 0
    property bool active: true
    opacity: root.active ? Math.max(0, 1 - root.pagePos) : 0
    enabled: false

    readonly property real wx: {
        if (Weather.kind === "clear")
            return Weather.day ? 1 : 2
        if (Weather.kind === "cloudy")
            return 3
        if (Weather.kind === "overcast")
            return 4
        if (Weather.kind === "fog")
            return 5
        if (Weather.kind === "rain")
            return 6
        if (Weather.kind === "rainMid")
            return 7
        if (Weather.kind === "rainHard")
            return 8
        if (Weather.kind === "thunder")
            return 9
        if (Weather.kind === "snow")
            return 10
        if (Weather.kind === "snowMid")
            return 11
        if (Weather.kind === "snowHard")
            return 12
        if (Weather.kind === "hail")
            return 13
        if (Weather.kind === "haze")
            return 14
        if (Weather.kind === "dust")
            return 15
        if (Weather.kind === "sleet")
            return 16
        if (Weather.kind === "freezeRain")
            return 17
        if (Weather.kind === "wind")
            return 18
        if (Weather.kind === "blizzard")
            return 19
        if (Weather.kind === "frost")
            return 20
        if (Weather.kind === "sandLift")
            return 21
        if (Weather.kind === "ponding")
            return 22
        if (Weather.kind === "wetRoad")
            return 23
        if (Weather.kind === "typhoon")
            return 24
        return 0
    }

    property real fromWx: 0
    property real toWx: 0
    property real fade: 1
    property bool shaderReady: false

    Timer {
        interval: 2200
        running: root.active
        repeat: false
        onTriggered: root.shaderReady = true
    }

    onWxChanged: {
        fromWx = toWx
        toWx = wx
        fade = 0
        fadeAnim.restart()
    }
    NumberAnimation {
        id: fadeAnim
        target: root
        property: "fade"
        to: 1
        duration: 1800
        easing.type: Easing.InOutQuad
    }

    ShaderEffect {
        anchors.fill: parent
        visible: root.active && root.shaderReady && root.wx > 0
        property variant wallpaper: root.wallpaper
        property real time: 0
        property real wx: root.toWx
        property real day: Weather.day ? 1 : 0
        property real wx2: root.fromWx
        property real fade: root.fade
        fragmentShader: "qrc:/qt/qml/IviShell/weather.frag.qsb"
        NumberAnimation on time {
            from: 0
            to: 400
            duration: 400000
            loops: Animation.Infinite
            running: root.opacity > 0.01 && root.wx > 0
        }
    }
}
