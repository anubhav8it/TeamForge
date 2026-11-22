import QtQuick
import QtQuick.Layouts
import TeamForge

// Field label with an optional hint, above a form control.
ColumnLayout {
    id: label
    property string text
    property string hint
    spacing: 2
    Layout.fillWidth: true
    TfText { text: label.text; variant: "label"; font.pixelSize: Theme.fontSm; Layout.fillWidth: true }
    TfText {
        visible: label.hint !== ""
        text: label.hint
        variant: "muted"
        font.pixelSize: Theme.fontXs
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        Layout.fillWidth: true
    }
}
