import QtQuick
import QtQuick.Layouts
import TeamForge

// Page title, plus the active-project switcher in the host and developer workspaces.
Item {
    id: bar

    property string title
    property string subtitle
    property bool showProjectSwitcher: false
    // Community notifications (Hub): a bell with the unread count.
    property bool showBell: false
    signal bellClicked()

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
        Rectangle {
            id: bell
            visible: bar.showBell
            implicitWidth: 44
            implicitHeight: 44
            radius: 22
            color: bellHover.hovered ? Theme.surfaceHover : Theme.surface
            border.color: Theme.border
            Accessible.role: Accessible.Button
            Accessible.name: Hub.unreadNotifications > 0 ? qsTr("Notifications, %1 unread").arg(Hub.unreadNotifications) : qsTr("Notifications")
            Accessible.onPressAction: bar.bellClicked()
            // a bell: dome, rim and clapper
            Rectangle { x: 15; y: 12; width: 14; height: 14; radius: 7; color: "transparent"; border.width: 2; border.color: Theme.textSecondary }
            Rectangle { x: 15; y: 19; width: 14; height: 7; color: bell.color }
            Rectangle { x: 15; y: 19; width: 2; height: 8; color: Theme.textSecondary }
            Rectangle { x: 27; y: 19; width: 2; height: 8; color: Theme.textSecondary }
            Rectangle { x: 12; y: 26; width: 20; height: 2; radius: 1; color: Theme.textSecondary }
            Rectangle { x: 20; y: 29; width: 4; height: 3; radius: 1.5; color: Theme.textSecondary }
            Rectangle {
                visible: Hub.unreadNotifications > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -2
                anchors.topMargin: -2
                implicitWidth: Math.max(20, count.implicitWidth + 10)
                implicitHeight: 20
                radius: 10
                color: Theme.accent
                TfText {
                    id: count
                    anchors.centerIn: parent
                    text: Hub.unreadNotifications > 99 ? "99+" : String(Hub.unreadNotifications)
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.weight: Font.ExtraBold
                }
            }
            HoverHandler { id: bellHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: bar.bellClicked() }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
}
