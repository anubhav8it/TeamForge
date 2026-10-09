import QtQuick
import QtQuick.Layouts
import TeamForge

// Card surface with an optional header (title, subtitle, actions). The first content item
// should fill the body; its implicitHeight sizes the panel when the layout allows.
Rectangle {
    id: panel

    property string title
    property string subtitle
    property bool padded: true
    default property alias content: body.data
    property alias actions: actionRow.data

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    implicitWidth: 280
    implicitHeight: layout.implicitHeight

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: 0

        RowLayout {
            visible: panel.title !== "" || actionRow.children.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: Theme.pad
            Layout.rightMargin: Theme.pad - 4
            Layout.topMargin: 16
            Layout.bottomMargin: 14
            spacing: 8

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                TfText {
                    text: panel.title
                    variant: "section"
                    Layout.fillWidth: true
                }
                TfText {
                    visible: panel.subtitle !== ""
                    text: panel.subtitle
                    variant: "muted"
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                id: actionRow
                spacing: 6
            }
        }

        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: panel.padded ? Theme.pad : 1
            Layout.rightMargin: panel.padded ? Theme.pad : 1
            Layout.bottomMargin: panel.padded ? Theme.pad : 1
            implicitHeight: children.length > 0 ? children[0].implicitHeight : 0
        }
    }
}
