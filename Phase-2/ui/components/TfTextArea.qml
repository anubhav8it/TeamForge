import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Multi-line text input in the TfTextField style, with a character counter. `tooLong` turns the
// counter amber; the owner decides what that blocks (the backend enforces the real limit).
ColumnLayout {
    id: area

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property int maxLength: 500
    property int minimumHeight: 72
    readonly property bool tooLong: field.length > maxLength
    signal editingFinished()

    function clear() { field.clear() }
    function focusField() { field.forceActiveFocus() }

    spacing: 4

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(area.minimumHeight, Math.min(field.implicitHeight, 150))
        clip: true

        TextArea {
            id: field
            wrapMode: TextArea.Wrap
            leftPadding: 12
            rightPadding: 12
            topPadding: 10
            bottomPadding: 10
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBase
            font.weight: Font.Medium
            color: Theme.text
            placeholderTextColor: Theme.textMuted
            selectionColor: Theme.accent
            selectedTextColor: "#FFFFFF"
            selectByMouse: true
            hoverEnabled: true
            textFormat: TextEdit.PlainText
            Accessible.name: placeholderText
            onEditingFinished: area.editingFinished()

            background: Rectangle {
                radius: Theme.radiusSmall
                color: Theme.input
                border.width: field.activeFocus ? 2 : 1
                border.color: area.tooLong ? Theme.warning
                            : field.activeFocus ? Theme.accent : field.hovered ? Theme.borderStrong : Theme.border
                Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
            }
        }
    }
    TfText {
        Layout.alignment: Qt.AlignRight
        text: field.length + " / " + area.maxLength
        variant: "muted"
        font.pixelSize: Theme.fontXs
        color: area.tooLong ? Theme.warning : Theme.textMuted
    }
}
