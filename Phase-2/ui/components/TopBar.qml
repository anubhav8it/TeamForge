import QtQuick
import QtQuick.Layouts
import TeamForge

// Page title, plus the active-project switcher in the host and developer workspaces.
Item {
    id: bar

    property string title
    property string subtitle
    property bool showProjectSwitcher: false

    implicitHeight: 84

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 16

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            TfText {
                text: bar.title
                variant: "title"
                Layout.fillWidth: true
            }
            TfText {
                text: bar.subtitle
                variant: "muted"
                Layout.fillWidth: true
            }
        }

        TfText {
            visible: bar.showProjectSwitcher
            text: qsTr("Project")
            variant: "label"
        }
        TfComboBox {
            id: projectBox
            visible: bar.showProjectSwitcher
            Layout.preferredWidth: 280
            model: Backend.requirements
            textRole: "name"
            valueRole: "id"
            enabled: count > 0
            onActivated: Backend.currentRequirementId = currentValue

            function sync() {
                currentIndex = indexOfValue(Backend.currentRequirementId)
            }
            Component.onCompleted: sync()
            Connections {
                target: Backend
                function onSelectionChanged() { projectBox.sync() }
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
}
