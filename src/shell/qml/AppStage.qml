import QtQuick
import Ivi.Services 1.0

Item {
    id: root
    property var running: []
    property var background: []
    property string currentId: ""
    readonly property bool opened: loader.source.toString() !== ""

    function publish() {
        const out = []
        for (let i = 0; i < running.length; ++i) {
            if (running[i].appId !== currentId)
                out.push(running[i])
        }
        background = out
    }

    function open(entry) {
        const info = AppCatalog.appInfo(entry)
        currentId = info.appId || ""
        if (currentId !== "") {
            let found = false
            for (let i = 0; i < running.length; ++i) {
                if (running[i].appId === currentId)
                    found = true
            }
            if (!found)
                running = running.concat([info])
        }
        loader.source = entry
        publish()
    }

    function dismiss(id) {
        const next = []
        for (let i = 0; i < running.length; ++i) {
            if (running[i].appId !== id)
                next.push(running[i])
        }
        running = next
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
        loader.source = ""
        publish()
    }

    visible: opened

    Rectangle {
        anchors.fill: parent
        color: SystemState.page
    }

    Loader {
        id: loader
        anchors.fill: parent
        anchors.topMargin: (currentId === "carplay" && CarPlaySession.hasVideo) ? 0 : 8
        anchors.bottomMargin: (currentId === "carplay" && CarPlaySession.hasVideo) ? 0 : 16
    }

    Text {
        anchors.centerIn: parent
        visible: loader.status === Loader.Error
        text: "无法打开应用"
        color: SystemState.ink
        font.pixelSize: 18
    }
}
