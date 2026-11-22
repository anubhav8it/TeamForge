pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// One team: what it is about, its members and skill coverage, and (for the leader and team
// admins) recruitment: Find members, Invite via link, and one list of applications and
// invitations with each person's compatibility. Visitors can apply or message the leaders
// anonymously; invited people accept or decline here; members open the chat. Every action is
// checked again by the server.
Item {
    id: detail

    property int teamId: 0
    property string tab: ""
    property bool applyRequested: false
    signal navigate(string route)
    signal notify(string message)

    readonly property bool narrow: width < 720
    readonly property int margin: narrow ? 14 : 24
    property var team: null
    property bool loading: true
    property string error: ""
    readonly property string myRole: team ? (team.myRole ?? "") : ""
    readonly property string currentTab: tab === "applications" && team && team.canManage ? "applications"
                                       : tab === "members" ? "members" : "overview"

    // applications and invitations (managers)
    property var apps: ({ items: [], page: 1, pages: 1, total: 0, counts: {} })
    property string appStatus: "pending"
    property bool appsLoading: false
    readonly property int vacancies: team ? Math.max(0, team.maxMembers - team.memberCount) : 0

    function reportFor(person, extra) {
        return Backend.teamCompatibility(team, Object.assign({}, person ?? {}, extra ?? {}))
    }

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
        Hub.get("/teams/" + teamId + "/recruitment?page=" + page + "&filter=" + (appStatus !== "" ? appStatus : "all"), (status, data) => {
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
    FindMembersDialog {
        id: finder
        onInvited: name => { detail.notify(qsTr("Invitation sent to %1").arg(name)); detail.load() }
        onReviewApplications: detail.navigate("teams/" + detail.teamId + "/applications")
    }
    InviteLinkDialog { id: linkDialog; onNotify: m => detail.notify(m) }
    CandidateDialog { id: reviewDialog }
    HubDialog {
        id: declineDialog
        title: qsTr("Decline the invitation to %1?").arg(detail.team ? detail.team.name : "")
        subtitle: qsTr("The team is told. You can add a short reply.")
        primaryText: qsTr("Decline")
        primaryVariant: "danger"
        onAccepted: { close(); detail.respondInvitation("decline", declineReply.text) }
        TfTextArea { id: declineReply; maxLength: 600; placeholderText: qsTr("Optional"); Layout.fillWidth: true }
    }
    function respondInvitation(decision, response) {
        Hub.post("/team-invitations/" + team.myInvitation.id + "/respond", { decision: decision, response: response ?? "" }, (status, data) => {
            if (status === 200) {
                Hub.refresh()
                if (decision === "accept") { detail.notify(qsTr("Welcome to %1!").arg(detail.team.name)); detail.navigate("messages/" + detail.teamId) }
                else { detail.notify(qsTr("Invitation declined")); detail.load() }
            } else {
                detail.notify(data.error ?? qsTr("That did not work."))
                detail.load()
            }
        })
    }
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
                Layout.leftMargin: detail.margin
                Layout.rightMargin: detail.margin
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
                        TeamAvatar { name: detail.team ? detail.team.name : ""; size: 60 }
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
                            visible: !detail.narrow
                            text: detail.team && detail.team.full ? qsTr("Full") : detail.team && detail.team.recruiting ? qsTr("Recruiting") : qsTr("Not recruiting")
                            tone: detail.team && !detail.team.full && detail.team.recruiting ? "success" : "muted"
                            large: true
                        }
                    }
                    // capacity
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        MeterBar {
                            value: detail.team ? detail.team.memberCount / Math.max(1, detail.team.maxMembers) : 0
                            fillColor: detail.team && detail.team.full ? Theme.warning : Theme.accent
                            Layout.fillWidth: true
                            Layout.maximumWidth: 320
                            Accessible.role: Accessible.ProgressBar
                            Accessible.name: qsTr("%1 of %2 members").arg(detail.team ? detail.team.memberCount : 0).arg(detail.team ? detail.team.maxMembers : 0)
                        }
                        TfText {
                            text: detail.team ? (detail.team.full ? qsTr("Team is full") : qsTr("%n place(s) left", "", detail.vacancies)) : ""
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.Bold
                            color: detail.team && detail.team.full ? Theme.warning : Theme.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // invited: accept or decline right here
                    Rectangle {
                        visible: detail.myRole === "" && detail.team && !!detail.team.myInvitation
                        Layout.fillWidth: true
                        implicitHeight: inviteBanner.implicitHeight + 24
                        radius: Theme.radius
                        color: Theme.accentSubtle
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5)
                        ColumnLayout {
                            id: inviteBanner
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 12
                            spacing: 8
                            TfText {
                                text: qsTr("You're invited to join this team")
                                font.weight: Font.ExtraBold
                                Layout.fillWidth: true
                            }
                            TfText {
                                visible: detail.team && detail.team.myInvitation && detail.team.myInvitation.message !== ""
                                text: detail.team && detail.team.myInvitation ? "“" + detail.team.myInvitation.message + "”" : ""
                                variant: "secondary"
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                                Layout.fillWidth: true
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                TfButton {
                                    text: qsTr("Accept and join")
                                    variant: "primary"
                                    compact: true
                                    enabled: detail.team && !detail.team.full
                                    onClicked: {
                                        confirm.pendingAction = () => detail.respondInvitation("accept")
                                        confirm.ask(qsTr("Join %1?").arg(detail.team.name), qsTr("You become a member and get the team chat."), qsTr("Join team"))
                                    }
                                }
                                TfButton {
                                    text: qsTr("Decline")
                                    variant: "ghost"
                                    compact: true
                                    onClicked: { declineReply.clear(); declineDialog.open() }
                                }
                            }
                        }
                    }

                    // recruitment (leader and team admins)
                    Rectangle {
                        visible: detail.team && detail.team.canManage
                        Layout.fillWidth: true
                        implicitHeight: recruitGrid.implicitHeight + 24
                        radius: Theme.radius
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) }
                            GradientStop { position: 1; color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b, 0.10) }
                        }
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                        GridLayout {
                            id: recruitGrid
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 12
                            columns: detail.width < 460 ? 1 : detail.narrow ? 2 : 4
                            columnSpacing: 10
                            rowSpacing: 8
                            ColumnLayout {
                                spacing: 1
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.columnSpan: detail.width < 460 ? 1 : detail.narrow ? 2 : 1
                                TfText { text: qsTr("Recruit"); font.weight: Font.ExtraBold; Layout.fillWidth: true }
                                TfText {
                                    text: detail.team ? [qsTr("%n pending application(s)", "", detail.team.pendingApplications ?? 0),
                                                         qsTr("%n open invitation(s)", "", detail.team.pendingInvitations ?? 0),
                                                         detail.team.inviteLinkActive ? qsTr("link active") : ""].filter(t => t !== "").join(" · ") : ""
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                            TfButton {
                                text: qsTr("Find members")
                                variant: "primary"
                                compact: true
                                Layout.fillWidth: detail.narrow
                                ToolTip.visible: hovered
                                ToolTip.delay: 400
                                ToolTip.text: qsTr("Search registered people and see how well each fits the team")
                                onClicked: finder.openFor(detail.team)
                            }
                            TfButton {
                                text: qsTr("Invite via link")
                                compact: true
                                Layout.fillWidth: detail.narrow
                                onClicked: linkDialog.openFor(detail.team)
                            }
                            TfButton {
                                visible: !detail.narrow
                                text: qsTr("Review")
                                variant: "ghost"
                                compact: true
                                onClicked: detail.navigate("teams/" + detail.teamId + "/applications")
                            }
                        }
                    }

                    // actions
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        TfButton {
                            visible: detail.myRole === "" && !(detail.team && detail.team.myInvitation)
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
            ScrollRow {
                Layout.fillWidth: true
                Layout.leftMargin: detail.margin
                Layout.rightMargin: detail.margin
            Segmented {
                options: [{ text: qsTr("Overview"), value: "overview" }, { text: qsTr("Members  %1").arg(detail.team ? detail.team.memberCount : 0), value: "members" }]
                         .concat(detail.team && detail.team.canManage
                                 ? [{ text: qsTr("Applications  %1").arg((detail.team.pendingApplications ?? 0) + (detail.team.pendingInvitations ?? 0)), value: "applications" }] : [])
                currentValue: detail.currentTab
                onActivated: value => detail.navigate("teams/" + detail.teamId + (value === "overview" ? "" : "/" + value))
            }
            }

            // ------------------------------------------------------------ overview
            GridLayout {
                visible: detail.currentTab === "overview"
                Layout.fillWidth: true
                Layout.leftMargin: detail.margin
                Layout.rightMargin: detail.margin
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
                    }
                }
                Panel {
                    title: qsTr("Skill coverage")
                    subtitle: qsTr("What the team needs and what its members already cover")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    Layout.columnSpan: detail.width > 1000 ? 2 : 1
                    TeamSkillCoverage { width: parent.width; team: detail.team }
                }
            }

            // ------------------------------------------------------------ members
            ColumnLayout {
                visible: detail.currentTab === "members"
                Layout.fillWidth: true
                Layout.leftMargin: detail.margin
                Layout.rightMargin: detail.margin
                Layout.bottomMargin: 24
                spacing: 8
                Repeater {
                    model: detail.team ? detail.team.members : []
                    delegate: Surface {
                        id: memberRow
                        required property var modelData
                        readonly property var skills: (Backend.participantCard(modelData.studentId ?? "").skills ?? [])
                                                      .slice().sort((a, b) => b.level - a.level).slice(0, 5)
                        Layout.fillWidth: true
                        implicitHeight: memberGrid.implicitHeight + 24
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: Theme.border
                        // On phones the actions wrap onto a second line.
                        GridLayout {
                            id: memberGrid
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            columns: detail.narrow ? 3 : 7
                            columnSpacing: 12
                            rowSpacing: 8
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
                                    text: (memberRow.modelData.headline || memberRow.modelData.role) + " · " + qsTr("joined %1").arg(Hub.dateText(memberRow.modelData.joinedAt))
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                    Layout.fillWidth: true
                                }
                            }
                            HubStatus { status: memberRow.modelData.teamRole }
                            Flow {
                                spacing: 6
                                // may wrap narrower than all its buttons in a row
                                Layout.fillWidth: detail.narrow
                                Layout.minimumWidth: 0
                                Layout.columnSpan: detail.narrow ? 3 : 1
                                Layout.alignment: Qt.AlignVCenter
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
                            Flow {
                                visible: memberRow.skills.length > 0
                                spacing: 6
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.columnSpan: memberGrid.columns
                                Repeater {
                                    model: memberRow.skills
                                    delegate: Chip {
                                        required property var modelData
                                        text: Theme.skill(modelData.skill) + " · " + modelData.level
                                        skill: true
                                    }
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
                Layout.leftMargin: detail.margin
                Layout.rightMargin: detail.margin
                Layout.bottomMargin: 24
                spacing: 10
                ScrollRow {
                    Layout.fillWidth: true
                Segmented {
                    options: [{ text: qsTr("Pending"), value: "pending" }, { text: qsTr("Applications"), value: "applications" },
                              { text: qsTr("Via link"), value: "link" }, { text: qsTr("Invitations"), value: "invitations" },
                              { text: qsTr("Accepted"), value: "accepted" }, { text: qsTr("Declined"), value: "declined" },
                              { text: qsTr("All"), value: "" }]
                    currentValue: detail.appStatus
                    onActivated: value => { detail.appStatus = value; detail.loadApps(1) }
                }
                }
                HubSkeleton { visible: detail.appsLoading && detail.apps.items.length === 0; rows: 2; Layout.fillWidth: true }
                EmptyState {
                    visible: !detail.appsLoading && detail.apps.items.length === 0
                    title: detail.appStatus === "pending" ? qsTr("Nothing waiting") : qsTr("Nothing here")
                    message: qsTr("Applications, link applications and the invitations you send appear here. Use Find members or Invite via link to recruit.")
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                }
                Repeater {
                    model: detail.apps.items
                    delegate: ColumnLayout {
                        id: entry
                        required property var modelData
                        readonly property bool invitation: modelData.kind === "invitation"
                        readonly property var report: detail.reportFor(modelData.person,
                                                                       invitation ? {} : { applicationSkills: modelData.skills, expressedInterest: true })
                        Layout.fillWidth: true
                        spacing: 0
                        ApplicationCard {
                            visible: !entry.invitation
                            application: entry.invitation ? ({}) : entry.modelData
                            report: entry.invitation ? null : entry.report
                            reviewer: true
                            Layout.fillWidth: true
                            onDecide: (app, choice) => decisionDialog.openFor(app, choice)
                            onOpenProfile: id => personDialog.show(id)
                            onOpenReport: app => reviewDialog.show(app.applicant, entry.report, false)
                        }
                        InvitationCard {
                            visible: entry.invitation
                            invitation: entry.invitation ? entry.modelData : ({})
                            leader: true
                            Layout.fillWidth: true
                            onOpenProfile: person => reviewDialog.show(person, entry.report, false)
                            onRevoke: inv => {
                                confirm.pendingAction = () => Hub.post("/team-invitations/" + inv.id + "/revoke", {}, (status, data) => {
                                    if (status === 200) { detail.notify(qsTr("Invitation withdrawn")); detail.load() }
                                    else detail.notify(data.error ?? qsTr("That did not work."))
                                })
                                confirm.ask(qsTr("Withdraw the invitation to %1?").arg(inv.person ? inv.person.name : ""),
                                            qsTr("They are told it was withdrawn."), qsTr("Withdraw"))
                            }
                        }
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
