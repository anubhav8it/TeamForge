import QtQuick
import TeamForge

// Behind every workspace: the midnight page with light from two corners, the workspace's own
// colour at the top and violet at the bottom, and a faint dot grid. Static (no animation), so
// it costs nothing while people work.
Rectangle {
    id: ambient

    property color tone: Theme.accent

    color: Theme.background
    clip: true

    AmbientGlow {
        tone: ambient.tone
        strength: 0.10
        width: Math.max(ambient.width, ambient.height) * 0.9
        height: width
        x: ambient.width * 0.35 - width / 2
        y: -height * 0.55
        Behavior on tone { ColorAnimation { duration: Theme.animSlow } }
    }
    AmbientGlow {
        tone: Theme.violet
        strength: 0.07
        width: Math.max(ambient.width, ambient.height) * 0.8
        height: width
        x: ambient.width - width * 0.45
        y: ambient.height - height * 0.4
    }
    Canvas {
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Qt.rgba(1, 1, 1, 0.035)
            const step = 28
            for (let x = step / 2; x < width; x += step)
                for (let y = step / 2; y < height; y += step)
                    ctx.fillRect(x, y, 1.2, 1.2)
        }
    }
}
