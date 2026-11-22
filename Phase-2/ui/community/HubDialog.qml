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
        // Right-aligned; on a narrow phone the buttons wrap instead of sticking out of the dialog.
        Flow {
            Layout.fillWidth: true
            Layout.topMargin: 4
            layoutDirection: Qt.RightToLeft
            spacing: 8
            TfButton {
                text: dialog.busy ? qsTr("Working…") : dialog.primaryText
                variant: dialog.primaryVariant
                enabled: dialog.primaryEnabled && !dialog.busy
                onClicked: dialog.accepted()
            }
            TfButton {
                text: qsTr("Cancel")
                variant: "ghost"
                onClicked: dialog.close()
            }
        }
    }
}
