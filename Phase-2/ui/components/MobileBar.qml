import QtQuick
import QtQuick.Layouts
import TeamForge

// The top bar on phones: the menu (opens the navigation drawer), the page title and the
// notification bell. Replaces the sidebar and the desktop title bar below 760 px.
Rectangle {
    id: bar

    property string title
    property bool showBell: false
    signal menuClicked()
    signal bellClicked()

    implicitHeight: 58
    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0E1424" }
        GradientStop { position: 1.0; color: Theme.sidebar }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 10
        spacing: 6

        // menu: three lines, 44 x 44 touch target
        Item {
            implicitWidth: 44
            implicitHeight: 44
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Open navigation")
            Accessible.onPressAction: bar.menuClicked()
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: menuHover.hovered || menuTap.pressed ? Theme.surfaceRaised : "transparent"
            }
            Column {
                anchors.centerIn: parent
                spacing: 4
                Repeater {
                    model: 3
                    delegate: Rectangle { required property int index; width: index === 1 ? 14 : 20; height: 2; radius: 1; color: Theme.text }
                }
            }
            HoverHandler { id: menuHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { id: menuTap; onTapped: bar.menuClicked() }
        }
        Image {
            source: Theme.mark
            fillMode: Image.PreserveAspectFit
            sourceSize.height: 48
            Layout.preferredHeight: 22
            Layout.preferredWidth: 30
        }
        TfText {
            text: bar.title
            font.pixelSize: Theme.fontMd
            font.weight: Font.ExtraBold
            Layout.fillWidth: true
            Layout.leftMargin: 2
        }
        Item {
            visible: bar.showBell
            implicitWidth: 44
            implicitHeight: 44
            Accessible.role: Accessible.Button
            Accessible.name: Hub.unreadNotifications > 0 ? qsTr("Notifications, %1 unread").arg(Hub.unreadNotifications) : qsTr("Notifications")
            Accessible.onPressAction: bar.bellClicked()
            Rectangle { x: 15; y: 12; width: 14; height: 14; radius: 7; color: "transparent"; border.width: 2; border.color: Theme.textSecondary }
            Rectangle { x: 15; y: 19; width: 14; height: 7; color: "#0C111E" }
            Rectangle { x: 15; y: 19; width: 2; height: 8; color: Theme.textSecondary }
            Rectangle { x: 27; y: 19; width: 2; height: 8; color: Theme.textSecondary }
            Rectangle { x: 12; y: 26; width: 20; height: 2; radius: 1; color: Theme.textSecondary }
            Rectangle { x: 20; y: 29; width: 4; height: 3; radius: 1.5; color: Theme.textSecondary }
            HubBadge {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 2
                count: Hub.unreadNotifications
            }
            TapHandler { onTapped: bar.bellClicked() }
        }
    }
}
