pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Someone opened a team's invitation link (the server remembered it when they signed in): the
// team's public details, their own compatibility, and the application. The leader decides;
// nothing is automatic.
Item {
    id: join

    signal navigate(string route)
    signal notify(string message)

    readonly property bool narrow: width < 720
    readonly property int margin: narrow ? 14 : 24
    property var claim: null
    property bool expired: false
    property bool loading: true
    property bool sent: false
    readonly property var team: claim ? claim.team : null
    readonly property var report: team ? Backend.teamCompatibility(team, Object.assign({}, Hub.profile,
                                         { studentId: Backend.myProfileId || Hub.profile.studentId })) : ({})

    function load() {
        loading = true
        Hub.get("/invite-link/claim", (code, data) => {
            loading = false
            if (code === 200) {
                claim = data.claim
                expired = data.expired === true
            }
        })
    }
    Component.onCompleted: load()

    ApplyDialog {
        id: applyDialog
        customPath: "/invite-link/claim/apply"
        onApplied: { join.sent = true; Hub.refresh(); join.notify(qsTr("Application sent. You'll be notified when the team decides.")) }
    }

    HubSkeleton { anchors.fill: parent; anchors.margins: 24; visible: join.loading; rows: 3 }
    EmptyState {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        visible: !join.loading && !join.claim && !join.sent
        title: join.expired ? qsTr("This invitation link has expired") : qsTr("No team invitation open")
        message: join.expired ? qsTr("The team made a new link or turned it off. Ask them for a new one, or find the team under Teams.")
                              : qsTr("Open a team's invitation link to see it here.")
        actionText: qsTr("Browse teams")
        onActionTriggered: join.navigate("teams")
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        visible: !join.loading && (!!join.claim || join.sent)

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Surface {
                Layout.fillWidth: true
                Layout.leftMargin: join.margin
                Layout.rightMargin: join.margin
                Layout.topMargin: 18
                implicitHeight: hero.implicitHeight + 44
                radius: Theme.radiusLarge
                tint: Theme.accent
                clip: true
                AmbientGlow { tone: Theme.accent; strength: 0.2; width: parent.width * 0.7; height: width; x: -width * 0.35; y: -height * 0.6 }
                AmbientGlow { tone: Theme.violet; strength: 0.14; width: parent.width * 0.6; height: width; x: parent.width - width * 0.45; y: -height * 0.4 }
                ColumnLayout {
                    id: hero
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 22
                    spacing: 10
                    TfText { text: qsTr("YOU'RE INVITED TO JOIN"); font.pixelSize: 12; font.weight: Font.ExtraBold; color: Theme.accentWarm; font.letterSpacing: 1 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        TeamAvatar { name: join.team ? join.team.name : ""; size: join.narrow ? 52 : 64 }
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            TfText { text: join.team ? join.team.name : ""; variant: "title"; font.pixelSize: join.narrow ? 22 : 28; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            TfText {
                                text: join.team ? join.team.tagline : ""
                                variant: "secondary"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            TfText {
                                text: join.team ? [join.team.leader ? qsTr("Led by %1").arg(join.team.leader.name) : "",
                                                   join.team.full ? qsTr("Full right now") : qsTr("%n place(s) left", "", join.team.vacancies)]
                                                  .filter(t => t !== "").join(" · ") : ""
                                variant: "muted"
                                font.pixelSize: Theme.fontXs
                                Layout.fillWidth: true
                            }
                        }
                    }
                    TfText {
                        visible: text !== ""
                        text: join.team ? (join.team.description || join.team.objectives || "") : ""
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                    // the decision area
                    TfText {
                        visible: join.sent
                        text: qsTr("✓  Application sent. The team leader reviews it and decides; you'll get a notification.")
                        color: Theme.success
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TfText {
                        visible: !join.sent && join.claim && (join.claim.member || !!join.claim.pendingApplication)
                        text: join.claim && join.claim.member ? qsTr("You are already a member of this team.")
                                                              : qsTr("You have already applied to this team.")
                        color: Theme.info
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TfText {
                        visible: !join.sent && join.team && !join.team.recruiting
                        text: qsTr("This team is not recruiting right now, so applications are closed.")
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        TfButton {
                            visible: !join.sent && join.claim && !join.claim.member && !join.claim.pendingApplication
                            text: Hub.needsProfile ? qsTr("Set up your profile first") : qsTr("Apply to join")
                            variant: "primary"
                            enabled: join.team && join.team.recruiting && !join.team.full
                            onClicked: Hub.needsProfile ? join.navigate("profile")
                                                        : applyDialog.openFor("team", "", join.team.name)
                        }
                        TfButton {
                            visible: join.sent || (join.claim && (join.claim.member || !!join.claim.pendingApplication))
                            text: qsTr("My applications")
                            onClicked: join.navigate("applications")
                        }
                        TfButton {
                            visible: !join.sent && !!join.claim
                            text: qsTr("Not interested")
                            variant: "ghost"
                            onClicked: Hub.post("/invite-link/claim/dismiss", {}, () => { Hub.refresh(); join.navigate("home") })
                        }
                    }
                }
            }

            GridLayout {
                visible: !!join.team
                Layout.fillWidth: true
                Layout.leftMargin: join.margin
                Layout.rightMargin: join.margin
                Layout.bottomMargin: 24
                columns: join.width > 980 ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap
                Panel {
                    title: qsTr("How you fit this team")
                    subtitle: qsTr("From your participant profile and the team's needs. The leader decides.")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    CompatibilityView { width: parent.width; report: join.report; forCandidate: true }
                }
                Panel {
                    title: qsTr("What the team needs")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    ColumnLayout {
                        width: parent.width
                        spacing: 8
                        TeamSkillCoverage { team: join.team; Layout.fillWidth: true }
                        TfText { visible: join.team && join.team.openRoles.length > 0; text: qsTr("Open roles"); variant: "label" }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: join.team ? join.team.openRoles : []
                                delegate: Chip { required property string modelData; text: modelData; tone: "info"; large: true }
                            }
                        }
                    }
                }
            }
        }
    }
}
