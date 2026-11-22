import QtQuick
import QtQuick.Controls.Basic
import TeamForge

TextField {
    id: control

    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBase
    font.weight: Font.Medium
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: "#FFFFFF"
    selectByMouse: true
    hoverEnabled: true
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.input
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

        // recessed: a little shade along the top inside edge
        Rectangle { x: parent.radius; y: 1; width: Math.max(0, parent.width - parent.radius * 2); height: 1; color: "#40000000" }
        // soft glow ring while focused
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 3
            border.color: Theme.focusRing
            opacity: control.activeFocus ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }
    }
}
