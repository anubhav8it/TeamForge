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
    }
}
