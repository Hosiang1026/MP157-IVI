import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property Item glassSource
    property var running: []
    property var background: []
    property string currentId: ""
    property bool loadError: false
    readonly property bool opened: currentId !== ""
    readonly property bool useGlass: opened && currentId !== "weather" && glassSource

    function refreshGlass() {
        if (frost.visible)
            frost.refresh()
    }

    ListModel {
        id: cache
    }

    function snapshot() {
        const out = []
        for (let i = 0; i < cache.count; ++i) {
            const row = cache.get(i)
            out.push({
                appId: row.appId,
                name: row.name,
                entry: row.entry,
                color: row.color
            })
        }
        return out
    }

    function publish() {
        running = snapshot()
        const out = []
        for (let i = 0; i < running.length; ++i) {
            if (running[i].appId !== currentId)
                out.push(running[i])
        }
        background = out
    }

    function cacheIndex(id) {
        for (let i = 0; i < cache.count; ++i) {
            if (cache.get(i).appId === id)
                return i
        }
        return -1
    }

    function open(entry) {
        const info = AppCatalog.appInfo(entry)
        currentId = info.appId || ""
        loadError = false
        if (currentId !== "" && cacheIndex(currentId) < 0) {
            cache.append({
                appId: info.appId,
                name: info.name || "",
                entry: info.entry || "",
                color: info.color || "#3A3A3C"
            })
        }
        publish()
    }

    function dismiss(id) {
        const i = cacheIndex(id)
        if (i >= 0)
            cache.remove(i)
        if (id === "music")
            MediaSession.pause()
        if (id === "map")
            NavSession.stop()
        if (currentId === id)
            close()
        else
            publish()
    }

    function close() {
        currentId = ""
        loadError = false
        publish()
    }

    visible: opened

    GlassPanel {
        id: frost
        anchors.fill: parent
        visible: root.useGlass
        sourceItem: root.glassSource
        radius: 0
        sheen: false
        fill: SystemState.dark ? "#A61C1C1E" : "#B3F2F2F7"
        stroke: "transparent"
        strokeWidth: 0
        blurAmount: 1.0
        blurMax: 64
    }

    Rectangle {
        anchors.fill: parent
        visible: root.opened && !root.useGlass && root.currentId !== "weather"
        color: SystemState.page
    }

    Connections {
        target: SystemState
        function onDarkChanged() {
            if (frost.visible)
                frost.refresh()
        }
    }

    Connections {
        target: root
        function onCurrentIdChanged() {
            if (frost.visible)
                frost.refresh()
        }
    }

    onVisibleChanged: {
        if (visible && frost.visible)
            frost.refresh()
    }

    Repeater {
        model: cache
        Loader {
            required property string appId
            required property string entry
            anchors.fill: parent
            anchors.topMargin: (appId === "weather" || (appId === "carplay" && CarPlaySession.hasVideo)) ? 0 : 8
            anchors.bottomMargin: (appId === "weather" || (appId === "carplay" && CarPlaySession.hasVideo)) ? 0 : 16
            visible: appId === root.currentId
            source: entry
            onVisibleChanged: {
                if (item && item.playing !== undefined && !visible)
                    item.playing = false
            }
            onStatusChanged: {
                if (appId === root.currentId)
                    root.loadError = status === Loader.Error
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.loadError
        text: "无法打开应用"
        color: SystemState.ink
        font.pixelSize: 18
    }
}
