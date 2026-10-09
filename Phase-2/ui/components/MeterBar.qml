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
    color: Theme.border

    Component.onCompleted: shown = Math.max(0, Math.min(1, value))
    onValueChanged: shown = Math.max(0, Math.min(1, value))
    Behavior on shown { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }

    Rectangle {
        width: meter.width * meter.shown
        height: parent.height
        radius: parent.radius
        color: meter.fillColor
        visible: width > 0.5
        Behavior on color { ColorAnimation { duration: Theme.animNormal } }
    }
}
