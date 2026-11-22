pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Community home: what needs a decision, how one's own applications stand, the teams with their
// latest message, and the newest notifications.
Item {
    id: home

    signal navigate(string route)
    signal notify(string message)

    readonly property var s: Hub.summary
    readonly property var mine: s.myApplications ?? ({})
    readonly property var pending: s.pendingDecisions ?? ({})
    readonly property var teams: s.teams ?? []

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Item { implicitHeight: 4 }

            // ------------------------------------------------------------ greeting + actions
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 14
                HubAvatar { person: Hub.profile; size: 52 }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    TfText {
                        text: Hub.needsProfile ? qsTr("Welcome to the Community") : qsTr("Hi, %1").arg((Hub.profile.name ?? "").split(" ")[0])
                        variant: "section"
                        Layout.fillWidth: true
                    }
                    TfText {
                        text: qsTr("Find a team, apply to events, talk with your teammates.")
                        variant: "muted"
                        Layout.fillWidth: true
                    }
                }
                TfButton { text: qsTr("Find a team"); compact: true; onClicked: home.navigate("teams") }
                TfButton { text: qsTr("Browse events"); compact: true; onClicked: home.navigate("events") }
            }

            // ------------------------------------------------------------ tiles
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                columns: home.width > 1000 ? 4 : 2
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Repeater {
                    model: [
                        { label: qsTr("Unread messages"), value: Hub.unreadMessages, tone: Theme.info, route: "messages",
                          hint: qsTr("in your team chats") },
                        { label: qsTr("Waiting for your decision"), value: Hub.pendingDecisions, tone: Theme.accent,
                          route: "applications", hint: qsTr("%1 team · %2 event").arg(home.pending.team ?? 0).arg(home.pending.event ?? 0) },
                        { label: qsTr("My open applications"), value: (home.mine.pending ?? 0) + (home.mine.waitlisted ?? 0),
                          tone: Theme.warning, route: "applications?tab=mine",
                          hint: qsTr("%1 approved so far").arg(home.mine.approved ?? 0) },
                        { label: qsTr("Anonymous requests"), value: Hub.anonymousInbox, tone: Theme.violet, route: "anonymous",
                          hint: qsTr("new in your inbox") }
                    ]
                    delegate: Rectangle {
                        id: tile
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: 112
                        radius: Theme.radius
                        color: tileHover.hovered ? Theme.surfaceRaised : Theme.surface
                        border.color: Theme.border
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.label + ": " + modelData.value
                        Accessible.onPressAction: home.navigate(tile.modelData.route)
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 14
                            width: 3
                            radius: 1.5
                            color: tile.modelData.tone
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            anchors.leftMargin: 28
                            spacing: 2
                            TfText { text: tile.modelData.label; variant: "label"; Layout.fillWidth: true }
                            TfText { text: String(tile.modelData.value); variant: "score"; color: tile.modelData.value > 0 ? Theme.text : Theme.textMuted }
                            TfText { text: tile.modelData.hint; variant: "muted"; font.pixelSize: Theme.fontXs; Layout.fillWidth: true }
                        }
                        HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: home.navigate(tile.modelData.route) }
                    }
                }
            }

            // ------------------------------------------------------------ teams + notifications
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                columns: home.width > 1000 ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Panel {
                    title: qsTr("Your teams")
                    subtitle: home.teams.length > 0 ? qsTr("Latest message in each team chat") : ""
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    actions: TfButton { text: qsTr("New team"); compact: true; variant: "ghost"; onClicked: home.navigate("teams?create=1") }

                    ColumnLayout {
                        width: parent.width
                        spacing: 6
                        EmptyState {
                            visible: home.teams.length === 0
                            title: qsTr("No team yet")
                            message: qsTr("Apply to a team that is recruiting, or start your own and invite people to apply.")
                            actionText: qsTr("Find a team")
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            Layout.bottomMargin: 8
                            onActionTriggered: home.navigate("teams")
                        }
                        Repeater {
                            model: home.teams
                            delegate: Rectangle {
                                id: teamRow
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 62
                                radius: Theme.radiusSmall
                                color: rowHover.hovered ? Theme.surfaceRaised : "transparent"
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Open the chat of %1").arg(modelData.name)
                                Accessible.onPressAction: home.navigate("messages/" + teamRow.modelData.teamId)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 10
                                    HubAvatar { person: ({ name: teamRow.modelData.name, color: Theme.avatarColor(teamRow.modelData.name) }); size: 38 }
                                    ColumnLayout {
                                        spacing: 0
                                        Layout.fillWidth: true
                                        RowLayout {
                                            Layout.fillWidth: true
                                            TfText { text: teamRow.modelData.name; font.weight: Font.Bold; Layout.fillWidth: true }
                                            TfText {
                                                text: teamRow.modelData.lastMessage ? Hub.relativeTime(teamRow.modelData.lastMessage.createdAt) : ""
                                                variant: "muted"
                                                font.pixelSize: Theme.fontXs
                                            }
                                        }
                                        TfText {
                                            text: teamRow.modelData.lastMessage
                                                  ? (teamRow.modelData.lastMessage.mine ? qsTr("You") : teamRow.modelData.lastMessage.author)
                                                    + ": " + teamRow.modelData.lastMessage.body.replace(/\n/g, " ")
                                                  : qsTr("No messages yet. Say hello!")
                                            variant: "muted"
                                            font.weight: teamRow.modelData.unread > 0 ? Font.Bold : Font.Medium
                                            color: teamRow.modelData.unread > 0 ? Theme.textSecondary : Theme.textMuted
                                            Layout.fillWidth: true
                                        }
                                    }
                                    HubBadge { count: teamRow.modelData.unread }
                                }
                                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: home.navigate("messages/" + teamRow.modelData.teamId) }
                            }
                        }
                    }
                }

                Panel {
                    title: qsTr("Notifications")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    actions: TfButton { text: qsTr("See all"); compact: true; variant: "ghost"; onClicked: home.navigate("notifications") }

                    ColumnLayout {
                        width: parent.width
                        spacing: 4
                        EmptyState {
                            visible: Hub.recentNotifications.length === 0
                            title: qsTr("Nothing new")
                            message: qsTr("Applications, decisions, invitations and replies show up here.")
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            Layout.bottomMargin: 8
                        }
                        Repeater {
                            model: Hub.recentNotifications.slice(0, 5)
                            delegate: HubNotificationRow {
                                required property var modelData
                                notification: modelData
                                Layout.fillWidth: true
                                onOpened: if (!Hub.openNotification(modelData)) home.notify(qsTr("Open the Admin page in the browser for this."))
                            }
                        }
                    }
                }
            }
        }
    }
}
