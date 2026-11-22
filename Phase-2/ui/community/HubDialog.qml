pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Modal form in the ConfirmDialog style: a title, a scrolling body (the dialog's children) and
// buttons. `error` shows above the buttons (from the server or a local check); `busy` disables
// the primary button while a request runs.
Popup {
    id: dialog

    property string title
    property string subtitle
    property string error
    property bool busy: false
    property string primaryText: qsTr("Send")
    property bool primaryEnabled: true
    property string primaryVariant: "primary"
    default property alias content: body.data
    signal accepted()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, parent.width - 48)
    modal: true
    dim: true
    padding: 24
    closePolicy: Popup.CloseOnEscape
    onOpened: error = ""

    Overlay.modal: Rectangle { color: "#990B0D11" }
    background: Rectangle {
        radius: Theme.radius
        color: Theme.surfaceRaised
        border.color: Theme.borderStrong
    }

    contentItem: ColumnLayout {
        spacing: 12
        TfText {
            text: dialog.title
            variant: "section"
            font.pixelSize: 20
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        TfText {
            visible: dialog.subtitle !== ""
            text: dialog.subtitle
            variant: "muted"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(body.implicitHeight, (dialog.parent ? dialog.parent.height : 700) - 260)
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: flick.contentHeight > flick.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
            ColumnLayout {
                id: body
                width: flick.width - (flick.contentHeight > flick.height ? 14 : 0)
                spacing: 12
            }
        }
        Rectangle {
            visible: dialog.error !== ""
            Layout.fillWidth: true
            implicitHeight: errorText.implicitHeight + 16
            radius: Theme.radiusSmall
            color: Theme.accentSubtle
            border.color: "#5A2A2E"
            TfText {
                id: errorText
                anchors.fill: parent
                anchors.margins: 8
                text: dialog.error
                color: "#FFD2D4"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                font.pixelSize: Theme.fontSm
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8
            Item { Layout.fillWidth: true }
            TfButton {
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: dialog.close()
            }
            TfButton {
                text: dialog.busy ? qsTr("Working…") : dialog.primaryText
                variant: dialog.primaryVariant
                enabled: dialog.primaryEnabled && !dialog.busy
                onClicked: dialog.accepted()
            }
        }
    }
}
