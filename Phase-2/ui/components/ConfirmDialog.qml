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

    Overlay.modal: Rectangle { color: "#B3050810" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }


    background: Surface { radius: Theme.radiusLarge; baseColor: Theme.surfaceRaised; border.color: Theme.borderStrong }

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
