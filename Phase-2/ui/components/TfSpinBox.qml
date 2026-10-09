import QtQuick
import QtQuick.Controls.Basic
import TeamForge

SpinBox {
    id: control

    implicitWidth: 116
    implicitHeight: 40
    editable: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontMd
    font.weight: Font.Bold
    hoverEnabled: true

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: Theme.text
        selectionColor: Theme.accent
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhDigitsOnly
    }

    down.indicator: Rectangle {
        x: 2
        y: 2
        width: 32
        height: control.height - 4
        radius: Theme.radiusSmall - 2
        color: control.down.pressed ? Theme.surfaceHover : control.down.hovered ? Theme.surfaceRaised : "transparent"
        Text {
            anchors.centerIn: parent
            text: "−"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLg
            font.weight: Font.Bold
            color: control.value > control.from ? Theme.textSecondary : Theme.textMuted
        }
    }

    up.indicator: Rectangle {
        x: control.width - width - 2
        y: 2
        width: 32
        height: control.height - 4
        radius: Theme.radiusSmall - 2
        color: control.up.pressed ? Theme.surfaceHover : control.up.hovered ? Theme.surfaceRaised : "transparent"
        Text {
            anchors.centerIn: parent
            text: "+"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLg
            font.weight: Font.Bold
            color: control.value < control.to ? Theme.textSecondary : Theme.textMuted
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.input
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.borderStrong : Theme.border
    }
}
