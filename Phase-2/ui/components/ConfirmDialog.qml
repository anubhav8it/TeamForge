import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Modal confirmation for actions that are hard to undo. Call ask(title, message, confirmText).
Popup {
    id: dialog

    property string title
    property string message
    property string confirmText: qsTr("Confirm")
    signal confirmed()

    function ask(newTitle, newMessage, newConfirmText) {
        title = newTitle
        message = newMessage
        confirmText = newConfirmText
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent.width - 48)
    modal: true
    dim: true
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: "#990B0D11" }

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong
    }

    contentItem: ColumnLayout {
        spacing: 14
        TfText {
            text: dialog.title
            variant: "section"
            font.pixelSize: 20
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        TfText {
            text: dialog.message
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Item { Layout.fillWidth: true }
            TfButton {
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: dialog.close()
            }
            TfButton {
                text: dialog.confirmText
                variant: "primary"
                onClicked: {
                    dialog.close()
                    dialog.confirmed()
                }
            }
        }
    }
}
