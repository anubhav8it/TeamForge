pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// One team: what it is about, its members, and (for the leader and team admins) its applications
// and member management. Visitors can apply or message the leaders anonymously; members open the
// chat. Every action is checked again by the server.
Item {
    id: detail

    property int teamId: 0
    property string tab: ""
    property bool applyRequested: false
    signal navigate(string route)
    signal notify(string message)

    property var team: null
    property bool loading: true
    property string error: ""
    readonly property string myRole: team ? (team.myRole ?? "") : ""
    readonly property string currentTab: tab === "applications" && team && team.canManage ? "applications"
                                       : tab === "members" ? "members" : "overview"

    // applications (managers)
    property var apps: ({ items: [], page: 1, pages: 1, total: 0 })
    property string appStatus: "pending"
    property bool appsLoading: false

    function load() {
        loading = true
        Hub.get("/teams/" + teamId, (status, data) => {
            loading = false
            if (status === 200) {
                team = data.team
                error = ""
                if (applyRequested && Hub.teamApplyBlocker(team) === "")
                    applyDialog.openFor("team", String(team.id), team.name)
                if (currentTab === "applications")
                    loadApps(1)
            } else {
                error = data.error ?? qsTr("This team could not be loaded.")
            }
        })
    }
    function loadApps(page) {
        appsLoading = true
        Hub.get("/teams/" + teamId + "/applications?page=" + page + (appStatus !== "" ? "&status=" + appStatus : ""), (status, data) => {
            appsLoading = false
            if (status === 200)
                apps = data
        })
    }
    function act(path, body, message) {
        Hub.post(path, body, (status, data) => {
            if (status === 200) {
                if (data.team) team = data.team
                if (message) detail.notify(message)
            } else {
                detail.notify(data.error ?? qsTr("That did not work."))
            }
        })
    }

    onTeamIdChanged: load()
    onCurrentTabChanged: if (currentTab === "applications") loadApps(1)
    Component.onCompleted: load()

    TeamFormDialog { id: editForm; onSaved: t => { detail.team = t; detail.notify(qsTr("Team updated")) } }
    ApplyDialog {
        id: applyDialog
        onApplied: { detail.notify(qsTr("Application sent. You'll be notified when the team decides.")); detail.load() }
    }
    AnonymousDialog {
        id: anonDialog
        onSent: threadId => { detail.notify(qsTr("Sent anonymously. Replies appear under Anonymous › Sent.")); detail.navigate("anonymous/" + threadId) }
    }
    DecisionDialog { id: decisionDialog; onDecided: status => { detail.notify(qsTr("Application %1").arg(Hub.statusLabel(status).toLowerCase())); detail.load(); detail.loadApps(detail.apps.page) } }
    PersonDialog { id: personDialog; onMessageAnonymously: p => anonDialog.openFor("person", p.id, p.name) }
    ConfirmDialog {
        id: confirm
        property var pendingAction: null
        onConfirmed: if (pendingAction) pendingAction()
    }
    HubDialog {
        id: deleteDialog
        title: qsTr("Delete %1?").arg(detail.team ? detail.team.name : "")
        subtitle: qsTr("The team, its chat and its applications are deleted for everyone. This cannot be undone. Type the team's name to confirm.")
        primaryText: qsTr("Delete team")
        primaryVariant: "danger"
        primaryEnabled: detail.team && confirmName.text === detail.team.name
        onAccepted: {
            busy = true
            Hub.post("/teams/" + detail.teamId + "/delete", { confirm: confirmName.text }, (status, data) => {
                busy = false
                if (status === 200) { close(); detail.notify(qsTr("Team deleted")); detail.navigate("teams") }
                else error = data.error ?? qsTr("The team could not be deleted.")
            })
        }
        TfTextField { id: confirmName; placeholderText: detail.team ? detail.team.name : ""; Layout.fillWidth: true }
    }

    HubSkeleton { anchors.fill: parent; anchors.margins: 24; visible: detail.loading && !detail.team; rows: 3 }
    EmptyState {
        anchors.centerIn: parent
        width: 420
        visible: !detail.loading && !detail.team
        title: qsTr("Team not found")
        message: detail.error
        actionText: qsTr("All teams")
        onActionTriggered: detail.navigate("teams")
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        visible: !!detail.team

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            // ------------------------------------------------------------ header
            Surface {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 18
                implicitHeight: header.implicitHeight + 40
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                ColumnLayout {
                    id: header
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        HubAvatar { person: ({ name: detail.team ? detail.team.name : "", color: Theme.avatarColor(detail.team ? detail.team.name : "") }); size: 60 }
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                TfText { text: detail.team ? detail.team.name : ""; variant: "title"; font.pixelSize: 26 }
                                HubStatus { visible: detail.myRole !== ""; status: detail.myRole }
                            }
                            TfText {
                                text: detail.team ? detail.team.tagline : ""
                                variant: "secondary"
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                                Layout.fillWidth: true
                            }
                            TfText {
                                text: detail.team ? qsTr("%1 / %2 members").arg(detail.team.memberCount).arg(detail.team.maxMembers)
                                                    + (detail.team.leader ? " · " + qsTr("led by %1").arg(detail.team.leader.name) : "")
                                                    + (detail.team.event ? " · " + detail.team.event.name : "") : ""
                                variant: "muted"
                                font.pixelSize: Theme.fontXs
                                Layout.fillWidth: true
                            }
                        }
                        Chip {
                            text: detail.team && detail.team.full ? qsTr("Full") : detail.team && detail.team.recruiting ? qsTr("Recruiting") : qsTr("Not recruiting")
                            tone: detail.team && !detail.team.full && detail.team.recruiting ? "success" : "muted"
                            large: true
                        }
                    }
                    // actions
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        TfButton {
                            visible: detail.myRole === ""
                            text: detail.team && detail.team.myApplication && detail.team.myApplication.status === "pending"
                                  ? qsTr("Application pending") : qsTr("Apply to join")
                            variant: "primary"
                            compact: true
                            enabled: detail.team && Hub.teamApplyBlocker(detail.team) === ""
                            ToolTip.visible: hovered && !enabled
                            ToolTip.text: detail.team ? Hub.teamApplyBlocker(detail.team) : ""
                            onClicked: applyDialog.openFor("team", String(detail.team.id), detail.team.name)
                        }
                        TfButton {
                            visible: detail.myRole === "" && detail.team && detail.team.myApplication && detail.team.myApplication.status === "pending"
                            text: qsTr("Withdraw application")
                            variant: "danger"
                            compact: true
                            onClicked: {
                                confirm.pendingAction = () => detail.act("/team-applications/" + detail.team.myApplication.id + "/withdraw", {}, qsTr("Application withdrawn"))
                                confirm.ask(qsTr("Withdraw your application?"), qsTr("You can apply again later."), qsTr("Withdraw"))
                            }
                        }
                        TfButton {
                            visible: detail.myRole === ""
                            text: qsTr("Message anonymously")
                            compact: true
                            onClicked: anonDialog.openFor("team", detail.team.id, detail.team.name)
                        }
                        TfButton {
                            visible: detail.myRole !== ""
                            text: qsTr("Open team chat")
                            variant: "primary"
                            compact: true
                            onClicked: detail.navigate("messages/" + detail.teamId)
                        }
                        TfButton {
                            visible: detail.team && detail.team.canManage
                            text: qsTr("Edit team")
                            compact: true
                            onClicked: editForm.openFor(detail.team)
                        }
                        TfButton {
                            visible: detail.myRole !== "" && detail.myRole !== "leader"
                            text: qsTr("Leave team")
                            compact: true
                            variant: "ghost"
                            onClicked: {
                                confirm.pendingAction = () => Hub.post("/teams/" + detail.teamId + "/leave", {}, (status, data) => {
                                    if (status === 200) { detail.notify(qsTr("You left the team")); detail.load() }
                                    else detail.notify(data.error)
                                })
                                confirm.ask(qsTr("Leave %1?").arg(detail.team.name), qsTr("You lose access to the team chat. To come back you would apply again."), qsTr("Leave"))
                            }
                        }
                        TfButton {
                            visible: detail.myRole === "leader"
                            text: qsTr("Delete team")
                            compact: true
                            variant: "danger"
                            onClicked: { confirmName.text = ""; deleteDialog.open() }
                        }
                    }
                    // my application's outcome
                    TfText {
                        visible: detail.myRole === "" && detail.team && !!detail.team.myApplication && detail.team.myApplication.status !== "pending"
                        text: detail.team && detail.team.myApplication
                              ? qsTr("Your last application: %1").arg(Hub.statusLabel(detail.team.myApplication.status))
                                + (detail.team.myApplication.response ? " — “" + detail.team.myApplication.response + "”" : "") : ""
                        variant: "muted"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                }
            }

            // ------------------------------------------------------------ tabs
            Segmented {
                Layout.leftMargin: 24
                options: [{ text: qsTr("Overview"), value: "overview" }, { text: qsTr("Members  %1").arg(detail.team ? detail.team.memberCount : 0), value: "members" }]
                         .concat(detail.team && detail.team.canManage
                                 ? [{ text: qsTr("Applications  %1").arg(detail.team.pendingApplications ?? 0), value: "applications" }] : [])
                currentValue: detail.currentTab
                onActivated: value => detail.navigate("teams/" + detail.teamId + (value === "overview" ? "" : "/" + value))
            }

            // ------------------------------------------------------------ overview
            GridLayout {
                visible: detail.currentTab === "overview"
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                columns: detail.width > 1000 ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap
                Panel {
                    title: qsTr("About")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        TfText {
                            text: detail.team && detail.team.description !== "" ? detail.team.description : qsTr("No description yet.")
                            variant: detail.team && detail.team.description !== "" ? "body" : "muted"
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                            Layout.fillWidth: true
                        }
                        TfText { visible: detail.team && detail.team.objectives !== ""; text: qsTr("Objectives"); variant: "label" }
                        TfText {
                            visible: detail.team && detail.team.objectives !== ""
                            text: detail.team ? detail.team.objectives : ""
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                            Layout.fillWidth: true
                        }
                    }
                }
                Panel {
                    title: qsTr("Opportunities")
                    subtitle: qsTr("Roles the team is looking for")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: detail.team ? detail.team.openRoles : []
                                delegate: Chip { required property string modelData; text: modelData; tone: "info"; large: true }
                            }
                        }
                        TfText {
                            visible: detail.team && detail.team.openRoles.length === 0
                            text: qsTr("No specific roles listed.")
                            variant: "muted"
                        }
                        TfText { visible: detail.team && detail.team.skills.length > 0; text: qsTr("Skills"); variant: "label" }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: detail.team ? detail.team.skills : []
                                delegate: Chip { required property string modelData; text: modelData; skill: true }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------------ members
            ColumnLayout {
                visible: detail.currentTab === "members"
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                spacing: 8
                Repeater {
                    model: detail.team ? detail.team.members : []
                    delegate: Surface {
                        id: memberRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 68
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 12
                            HubAvatar {
                                person: memberRow.modelData
                                size: 42
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: personDialog.show(memberRow.modelData.id) }
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                TfText { text: memberRow.modelData.name; font.weight: Font.Bold; Layout.fillWidth: true }
                                TfText {
                                    text: (memberRow.modelData.headline || memberRow.modelData.role) + " · " + qsTr("joined %1").arg(Hub.relativeTime(memberRow.modelData.joinedAt))
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                    Layout.fillWidth: true
                                }
                            }
                            HubStatus { status: memberRow.modelData.teamRole }
                            TfButton {
                                visible: Hub.canChangeRoles(detail.myRole) && memberRow.modelData.teamRole !== "leader"
                                text: memberRow.modelData.teamRole === "admin" ? qsTr("Make member") : qsTr("Make admin")
                                compact: true
                                variant: "ghost"
                                onClicked: detail.act("/teams/" + detail.teamId + "/members/" + memberRow.modelData.id + "/role",
                                                      { role: memberRow.modelData.teamRole === "admin" ? "member" : "admin" }, qsTr("Role changed"))
                            }
                            TfButton {
                                visible: Hub.canChangeRoles(detail.myRole) && memberRow.modelData.teamRole !== "leader"
                                text: qsTr("Make leader")
                                compact: true
                                variant: "ghost"
                                onClicked: {
                                    confirm.pendingAction = () => detail.act("/teams/" + detail.teamId + "/members/" + memberRow.modelData.id + "/role",
                                                                     { role: "leader" }, qsTr("%1 is now the leader").arg(memberRow.modelData.name))
                                    confirm.ask(qsTr("Hand leadership to %1?").arg(memberRow.modelData.name),
                                                qsTr("You stay in the team as an admin."), qsTr("Make leader"))
                                }
                            }
                            TfButton {
                                visible: detail.myRole !== "" && memberRow.modelData.id !== (Hub.profile.id ?? -1)
                                         && Hub.canRemoveMember(detail.myRole, memberRow.modelData.teamRole)
                                text: qsTr("Remove")
                                compact: true
                                variant: "danger"
                                onClicked: {
                                    confirm.pendingAction = () => detail.act("/teams/" + detail.teamId + "/members/" + memberRow.modelData.id + "/remove", {},
                                                                     qsTr("%1 was removed").arg(memberRow.modelData.name))
                                    confirm.ask(qsTr("Remove %1?").arg(memberRow.modelData.name),
                                                qsTr("They lose access to the team chat."), qsTr("Remove"))
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------------ applications (managers)
            ColumnLayout {
                visible: detail.currentTab === "applications"
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24
                spacing: 10
                Segmented {
                    options: [{ text: qsTr("Pending"), value: "pending" }, { text: qsTr("Approved"), value: "approved" },
                              { text: qsTr("Rejected"), value: "rejected" }, { text: qsTr("Withdrawn"), value: "withdrawn" },
                              { text: qsTr("All"), value: "" }]
                    currentValue: detail.appStatus
                    onActivated: value => { detail.appStatus = value; detail.loadApps(1) }
                }
                HubSkeleton { visible: detail.appsLoading && detail.apps.items.length === 0; rows: 2; Layout.fillWidth: true }
                EmptyState {
                    visible: !detail.appsLoading && detail.apps.items.length === 0
                    title: detail.appStatus === "pending" ? qsTr("No applications waiting") : qsTr("Nothing here")
                    message: qsTr("New applications appear here and in your notifications.")
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                }
                Repeater {
                    model: detail.apps.items
                    delegate: ApplicationCard {
                        required property var modelData
                        application: modelData
                        reviewer: true
                        Layout.fillWidth: true
                        onDecide: (app, choice) => decisionDialog.openFor(app, choice)
                        onOpenProfile: id => personDialog.show(id)
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: detail.apps.page; pages: detail.apps.pages; total: detail.apps.total
                    onPageRequested: p => detail.loadApps(p)
                }
            }
        }
    }
}
