pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Invitations I received from teams: open ones first, with the team, the message and my own
// compatibility; Accept joins the team (if there is still a place), Decline can carry a reply.
Item {
    id: invitations

    signal navigate(string route)
    signal notify(string message)

    readonly property bool narrow: width < 720
    readonly property int margin: narrow ? 14 : 24
    property var list: ({ items: [], page: 1, pages: 1, total: 0 })
    property string status: "pending"
    property bool loading: true

    function load(page) {
        loading = true
        Hub.get("/invitations/mine?page=" + (page ?? 1) + (status !== "" ? "&status=" + status : ""), (code, answer) => {
            loading = false
            if (code === 200)
                list = answer
        })
    }
    function myReport(team) {
        return Backend.teamCompatibility(team, Object.assign({}, Hub.profile, { studentId: Backend.myProfileId || Hub.profile.studentId }))
    }
    function respond(invitation, decision, response) {
        Hub.post("/team-invitations/" + invitation.id + "/respond", { decision: decision, response: response ?? "" }, (code, result) => {
            if (code === 200) {
                Hub.refresh()
                if (decision === "accept") {
                    invitations.notify(qsTr("Welcome to %1!").arg(invitation.targetName))
                    invitations.navigate("messages/" + result.teamId)
                } else {
                    invitations.notify(qsTr("Invitation declined"))
                    invitations.load(invitations.list.page)
                }
            } else {
                invitations.notify(result.error ?? qsTr("That did not work."))
                invitations.load(invitations.list.page)
            }
        })
    }

    Component.onCompleted: load(1)

    ConfirmDialog { id: confirm; property var pendingAction: null; onConfirmed: if (pendingAction) pendingAction() }
    HubDialog {
        id: declineDialog
        property var invitation: null
        title: qsTr("Decline the invitation to %1?").arg(invitation ? invitation.targetName : "")
        subtitle: qsTr("The team is told. You can add a short reply.")
        primaryText: qsTr("Decline")
        primaryVariant: "danger"
        onAccepted: { close(); invitations.respond(invitation, "decline", reply.text) }
        TfTextArea { id: reply; maxLength: 600; placeholderText: qsTr("Optional, e.g. I already joined another team."); Layout.fillWidth: true }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            spacing: 12
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: invitations.margin
                Layout.rightMargin: invitations.margin
                Layout.topMargin: 18
                columns: invitations.narrow ? 1 : 2
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    TfText { text: qsTr("Team invitations"); variant: "title"; font.pixelSize: 24 }
                    TfText {
                        text: qsTr("Teams that would like you to join. You decide; turn invitations off any time in Community › Profile.")
                        variant: "muted"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                ScrollRow {
                    Layout.fillWidth: invitations.narrow
                    Segmented {
                        options: [{ text: qsTr("Open"), value: "pending" }, { text: qsTr("Accepted"), value: "accepted" },
                                  { text: qsTr("Declined"), value: "declined" }, { text: qsTr("All"), value: "" }]
                        currentValue: invitations.status
                        onActivated: value => { invitations.status = value; invitations.load(1) }
                    }
                }
            }
            HubSkeleton { visible: invitations.loading && invitations.list.items.length === 0; rows: 2; Layout.fillWidth: true; Layout.margins: invitations.margin }
            EmptyState {
                visible: !invitations.loading && invitations.list.items.length === 0
                title: invitations.status === "pending" ? qsTr("No open invitations") : qsTr("Nothing here")
                message: qsTr("When a team leader invites you, it appears here and in your notifications.")
                actionText: qsTr("Browse teams")
                onActionTriggered: invitations.navigate("teams")
                Layout.fillWidth: true
                Layout.topMargin: 30
            }
            Repeater {
                model: invitations.list.items
                delegate: InvitationCard {
                    required property var modelData
                    invitation: modelData
                    leader: false
                    report: modelData.status === "pending" ? invitations.myReport(modelData.team) : ({})
                    Layout.fillWidth: true
                    Layout.leftMargin: invitations.margin
                    Layout.rightMargin: invitations.margin
                    onOpenTeam: id => invitations.navigate("teams/" + id)
                    onAccept: inv => {
                        confirm.pendingAction = () => invitations.respond(inv, "accept")
                        confirm.ask(qsTr("Join %1?").arg(inv.targetName), qsTr("You become a member and get the team chat."), qsTr("Join team"))
                    }
                    onDecline: inv => { declineDialog.invitation = inv; reply.clear(); declineDialog.open() }
                }
            }
            HubPager {
                Layout.fillWidth: true
                Layout.leftMargin: invitations.margin
                Layout.rightMargin: invitations.margin
                Layout.bottomMargin: 24
                page: invitations.list.page; pages: invitations.list.pages; total: invitations.list.total
                onPageRequested: p => invitations.load(p)
            }
        }
    }
}
