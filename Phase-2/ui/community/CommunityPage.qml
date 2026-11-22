pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Community: teams and their private chat, applications to teams and events, anonymous
// connection requests and notifications, with real accounts on the TeamForge server (Hub,
// src/app/HubClient.h; the server checks every permission). Shown in the Participant and Host
// workspaces. Where the Community is goes by Hub.route ("teams/3", "messages/5" ...), so
// notification links and buttons anywhere can open any part of it.
TfPage {
    id: page

    signal notify(string message)

    readonly property var parts: Hub.route.split("?")[0].split("/")
    readonly property string section: parts[0] !== "" ? parts[0] : "home"
    readonly property string itemId: parts.length > 1 ? decodeURIComponent(parts[1]) : ""
    readonly property string sub: parts.length > 2 ? parts[2] : ""
    readonly property bool applyRequested: Hub.route.indexOf("apply=1") >= 0
    // The id each detail page shows, changed only while its own section is open, so a page that
    // is closing never receives the next section's id.
    property string teamItem: ""
    property string eventItem: ""
    property string threadItem: ""
    function remember() {
        // read the route itself: the derived properties may not be updated yet at this point
        const split = Hub.route.split("?")[0].split("/")
        const id = split.length > 1 ? decodeURIComponent(split[1]) : ""
        if (split[0] === "teams") teamItem = id
        else if (split[0] === "events") eventItem = id
        else if (split[0] === "anonymous") threadItem = id
    }
    Connections {
        target: Hub
        function onRouteChanged() { page.remember() }
    }
    Component.onCompleted: remember()

    readonly property var tabs: [
        { key: "home", text: qsTr("Home"), count: 0 },
        { key: "messages", text: qsTr("Messages"), count: Hub.unreadMessages },
        { key: "teams", text: qsTr("Teams"), count: 0 },
        { key: "events", text: qsTr("Events"), count: 0 },
        { key: "applications", text: qsTr("Applications"), count: Hub.pendingDecisions },
        ...((Hub.summary.pendingInvitations ?? 0) > 0 || page.section === "invitations"
            ? [{ key: "invitations", text: qsTr("Invitations"), count: Hub.summary.pendingInvitations ?? 0 }] : []),
        { key: "anonymous", text: qsTr("Anonymous"), count: Hub.anonymousInbox + (Hub.summary.anonymousSentUnread ?? 0) },
        { key: "notifications", text: qsTr("Notifications"), count: Hub.unreadNotifications },
        { key: "profile", text: qsTr("Profile"), count: Hub.needsProfile ? 1 : 0 }
    ]

    function go(route) {
        Hub.route = route
    }

    onActiveChanged: if (active && Hub.connected) Hub.refresh()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------------------------------------------------------- section tabs
        Rectangle {
            visible: Hub.connected
            Layout.fillWidth: true
            implicitHeight: 52
            color: "transparent"
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            Flickable {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                contentWidth: tabRow.implicitWidth
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                Row {
                    id: tabRow
                    height: parent.height
                    spacing: 2
                    Repeater {
                        model: page.tabs
                        delegate: Item {
                            id: tab
                            required property var modelData
                            readonly property bool selected: page.section === modelData.key
                            width: tabContent.implicitWidth + 24
                            height: tabRow.height
                            Accessible.role: Accessible.PageTab
                            Accessible.name: modelData.text + (modelData.count > 0 ? qsTr(", %1 new").arg(modelData.count) : "")
                            Accessible.selected: selected
                            Accessible.onPressAction: page.go(tab.modelData.key)
                            Row {
                                id: tabContent
                                anchors.centerIn: parent
                                spacing: 6
                                TfText {
                                    text: tab.modelData.text
                                    font.weight: Font.Bold
                                    font.pixelSize: Theme.fontSm
                                    color: tab.selected ? Theme.text : tabHover.hovered ? Theme.textSecondary : Theme.textMuted
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                HubBadge {
                                    count: tab.modelData.key === "profile" ? 0 : tab.modelData.count
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle {
                                    visible: tab.modelData.key === "profile" && tab.modelData.count > 0
                                    width: 8; height: 8; radius: 4
                                    color: Theme.warning
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width - 12
                                height: 3
                                radius: 1.5
                                color: Theme.accent
                                visible: tab.selected
                            }
                            HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: page.go(tab.modelData.key) }
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------- profile reminder
        Rectangle {
            visible: Hub.connected && Hub.needsProfile && page.section !== "profile"
            Layout.fillWidth: true
            Layout.leftMargin: page.width < 720 ? 12 : 24
            Layout.rightMargin: page.width < 720 ? 12 : 24
            Layout.topMargin: 14
            implicitHeight: reminderRow.implicitHeight + 16
            radius: Theme.radiusSmall
            color: Theme.warningSubtle
            border.color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.45)
            RowLayout {
                id: reminderRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                TfText {
                    text: qsTr("Set up your Community profile so team leaders and organisers know who you are.")
                    color: "#FFE5BD"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
                TfButton { text: qsTr("Set up profile"); compact: true; onClicked: page.go("profile") }
            }
        }

        // ---------------------------------------------------------------- content
        Loader {
            id: content
            Layout.fillWidth: true
            Layout.fillHeight: true
            active: Hub.connected
            sourceComponent: {
                switch (page.section) {
                case "messages": return messagesPage
                case "teams": return page.itemId !== "" ? teamPage : teamsPage
                case "events": return page.itemId !== "" ? eventPage : eventsPage
                case "applications": return applicationsPage
                case "anonymous": return anonymousPage
                case "notifications": return notificationsPage
                case "profile": return profilePage
                case "people": return peoplePage
                case "invitations": return invitationsPage
                case "join": return joinPage
                default: return homePage
                }
            }
        }

        HubConnect {
            visible: !Hub.connected
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Component { id: homePage; HubHome { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component {
        id: messagesPage
        HubMessages {
            // only while Messages is open: the next section's id must not reach this page
            teamId: page.section === "messages" && page.itemId !== "" ? Number(page.itemId) : 0
            pageActive: page.active
            onNavigate: route => page.go(route)
            onNotify: m => page.notify(m)
        }
    }
    Component { id: teamsPage; HubTeams { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component {
        id: teamPage
        HubTeamDetail {
            teamId: Number(page.teamItem)
            tab: page.sub
            applyRequested: page.applyRequested
            onNavigate: route => page.go(route)
            onNotify: m => page.notify(m)
        }
    }
    Component { id: eventsPage; HubEvents { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component {
        id: eventPage
        HubEventDetail {
            eventId: page.eventItem
            tab: page.sub
            applyRequested: page.applyRequested
            onNavigate: route => page.go(route)
            onNotify: m => page.notify(m)
        }
    }
    Component { id: applicationsPage; HubApplications { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component {
        id: anonymousPage
        HubAnonymous {
            threadId: page.threadItem
            onNavigate: route => page.go(route)
            onNotify: m => page.notify(m)
        }
    }
    Component { id: notificationsPage; HubNotifications { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component { id: profilePage; HubProfile { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component { id: peoplePage; HubPeople { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component { id: invitationsPage; HubInvitations { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
    Component { id: joinPage; HubJoin { onNavigate: route => page.go(route); onNotify: m => page.notify(m) } }
}
