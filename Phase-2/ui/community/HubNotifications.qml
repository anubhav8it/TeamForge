pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Every notification, newest first: unread and read, with links to what each is about.
Item {
    id: notes

    signal navigate(string route)
    signal notify(string message)

    property var result: ({ items: [], page: 1, pages: 1, total: 0, unread: 0 })
    property bool loading: true
    property bool unreadOnly: false

    function load(page) {
        loading = true
        Hub.get("/notifications?page=" + (page ?? 1) + (unreadOnly ? "&unread=1" : ""), (code, data) => {
            loading = false
            if (code === 200) result = data
        })
    }
    Component.onCompleted: load(1)
    Connections {
        target: Hub
        function onNotificationsChanged() { if (!notes.loading) notes.load(notes.result.page) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Segmented {
                options: [{ text: qsTr("All"), value: false }, { text: qsTr("Unread  %1").arg(Hub.unreadNotifications), value: true }]
                currentValue: notes.unreadOnly
                onActivated: value => { notes.unreadOnly = value; notes.load(1) }
            }
            Item { Layout.fillWidth: true }
            TfButton {
                text: qsTr("Mark all as read")
                compact: true
                enabled: Hub.unreadNotifications > 0
                onClicked: { Hub.markAllNotificationsRead(); notes.notify(qsTr("All notifications marked as read")) }
            }
        }

        HubSkeleton { visible: notes.loading && notes.result.items.length === 0; rows: 4; rowHeight: 60; Layout.fillWidth: true }

        Rectangle {
            visible: notes.result.items.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: notes.result.items
                    ScrollBar.vertical: ScrollBar {}
                    delegate: HubNotificationRow {
                        required property var modelData
                        width: ListView.view.width
                        notification: modelData
                        onOpened: if (!Hub.openNotification(modelData)) notes.notify(qsTr("Open the Admin page in the browser for this."))
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: notes.result.page; pages: notes.result.pages; total: notes.result.total
                    onPageRequested: p => notes.load(p)
                }
            }
        }
        EmptyState {
            visible: !notes.loading && notes.result.items.length === 0
            title: notes.unreadOnly ? qsTr("All caught up") : qsTr("No notifications yet")
            message: qsTr("You'll hear about applications, decisions, invitations, replies and team messages here.")
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
