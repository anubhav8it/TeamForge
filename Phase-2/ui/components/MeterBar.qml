import QtQuick
import TeamForge

// Horizontal progress bar for 0..1 values. The fill animates in when shown and on change.
Rectangle {
    id: meter

    property real value: 0
    property color fillColor: Theme.accent
    property real shown: 0

    implicitHeight: 6
    radius: height / 2
    color: Theme.input
    border.color: Theme.border
    border.width: 1

    Component.onCompleted: shown = Math.max(0, Math.min(1, value))
    onValueChanged: shown = Math.max(0, Math.min(1, value))
    Behavior on shown { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }

    Rectangle {
        width: meter.width * meter.shown
        height: parent.height
        radius: parent.radius
        visible: width > 0.5
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.darker(meter.fillColor, 1.25) }
            GradientStop { position: 1.0; color: Qt.lighter(meter.fillColor, 1.15) }
        }
        // glow at the leading edge
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: parent.height + 6
            radius: height / 2
            color: Qt.rgba(meter.fillColor.r, meter.fillColor.g, meter.fillColor.b, 0.35)
        }
    }
}
