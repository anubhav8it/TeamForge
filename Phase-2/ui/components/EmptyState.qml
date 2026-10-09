import QtQuick
import QtQuick.Layouts
import TeamForge

// Centered placeholder for empty lists and missing selections.
ColumnLayout {
    id: empty

    property string title
    property string message
    property string actionText
    signal actionTriggered()

    spacing: 6

    TfText {
        text: empty.title
        variant: "heading"
        font.pixelSize: Theme.fontLg
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    TfText {
        visible: empty.message !== ""
        text: empty.message
        variant: "muted"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }
    TfButton {
        visible: empty.actionText !== ""
        text: empty.actionText
        variant: "primary"
        compact: true
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        onClicked: empty.actionTriggered()
    }
}
