import QtQuick
import TeamForge

// Circular progress for a 0..1 value (e.g. skill coverage), with the percentage in the middle.
// The arc animates on change.
Item {
    id: ring

    property real value: 0
    property color fillColor: Theme.accent
    property int size: 88
    property int lineWidth: 8
    property string caption
    property real shown: 0

    implicitWidth: size
    implicitHeight: size

    Component.onCompleted: shown = Math.max(0, Math.min(1, value))
    onValueChanged: shown = Math.max(0, Math.min(1, value))
    Behavior on shown { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
    onShownChanged: canvas.requestPaint()
    onFillColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const r = (Math.min(width, height) - ring.lineWidth) / 2
            const cx = width / 2
            const cy = height / 2
            ctx.lineWidth = ring.lineWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = Theme.border
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.stroke()
            if (ring.shown > 0.001) {
                ctx.strokeStyle = ring.fillColor
                ctx.beginPath()
                ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * ring.shown)
                ctx.stroke()
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: -2
        TfText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.round(ring.shown * 100) + "%"
            font.pixelSize: Math.round(ring.size * 0.24)
            font.weight: Font.ExtraBold
            font.letterSpacing: -0.5
        }
        TfText {
            visible: ring.caption !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            text: ring.caption
            variant: "muted"
            font.pixelSize: 12
        }
    }
}
