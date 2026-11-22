pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Invite via link: what people who open the link will see (the team's public details), and the
// link itself with Copy, a validity choice, Make a new link (the old one stops working) and
// Revoke. The server stores only a fingerprint of the link, so the address can be copied right
// after it is made (it is kept for this session); later, make a new one.
HubDialog {
    id: dialog

    property var team: null
    property var status: ({})          // GET /teams/<id>/invite-link
    property var made: ({})            // team id -> { url, expiresAt } made in this session
    property int days: 14
    property bool copied: false
    signal notify(string message)

    readonly property var link: team ? (made[team.id] ?? null) : null
    readonly property bool active: status.active === true
    readonly property bool showsUrl: active && link !== null && link.expiresAt === status.expiresAt
    readonly property var skills: team ? Object.keys(team.requiredSkills ?? {}) : []

    title: qsTr("Invite via link")
    subtitle: qsTr("Anyone with the link can see this team's public details and apply. You review every application; nobody joins automatically.")
    primaryText: qsTr("Done")
    onAccepted: close()

    function openFor(theTeam) {
        team = theTeam
        copied = false
        status = {}
        refresh()
        open()
    }
    function refresh() {
        Hub.get("/teams/" + team.id + "/invite-link", (code, data) => {
            if (code === 200) status = data
            else error = data.error ?? qsTr("The link could not be loaded.")
        })
    }
    function make() {
        busy = true
        Hub.post("/teams/" + team.id + "/invite-link", { days: dialog.days }, (code, data) => {
            busy = false
            if (code !== 201) {
                error = data.error ?? qsTr("The link could not be made.")
                return
            }
            const copy = Object.assign({}, made)
            copy[team.id] = { url: Hub.absoluteUrl(data.path), expiresAt: data.expiresAt }
            made = copy
            copied = false
            refresh()
            dialog.notify(qsTr("New invitation link ready"))
        })
    }
    function revoke() {
        busy = true
        Hub.post("/teams/" + team.id + "/invite-link/revoke", {}, (code, data) => {
            busy = false
            if (code === 200) { refresh(); dialog.notify(qsTr("Link revoked: it no longer works")) }
            else error = data.error ?? qsTr("The link could not be revoked.")
        })
    }

    // ---- what people will see
    Surface {
        Layout.fillWidth: true
        implicitHeight: preview.implicitHeight + 28
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            id: preview
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TeamAvatar { name: dialog.team ? dialog.team.name : ""; size: 46 }
                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    TfText { text: dialog.team ? dialog.team.name : ""; variant: "heading"; Layout.fillWidth: true }
                    TfText {
                        text: dialog.team ? [dialog.team.leader ? qsTr("Led by %1").arg(dialog.team.leader.name) : "",
                                             dialog.team.full ? qsTr("Full right now") : qsTr("%n place(s) left", "", dialog.team.vacancies ?? 0)]
                                            .filter(t => t !== "").join(" · ") : ""
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        Layout.fillWidth: true
                    }
                }
            }
            TfText {
                visible: text !== ""
                text: dialog.team ? (dialog.team.description || dialog.team.tagline || "") : ""
                variant: "secondary"
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 3
                Layout.fillWidth: true
            }
            Flow {
                visible: dialog.skills.length > 0
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: dialog.skills
                    delegate: Chip { required property string modelData; text: Theme.skill(modelData); skill: true }
                }
            }
        }
    }

    // ---- the link
    TfText {
        visible: dialog.team && !dialog.team.recruiting
        text: qsTr("Recruiting is off for this team, so people who open the link cannot apply. Turn it on under Edit team.")
        color: Theme.warning
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    HubLabel {
        text: dialog.active ? qsTr("Active link") : qsTr("No active link")
        hint: dialog.active ? qsTr("Expires %1 · opened %n time(s)", "", dialog.status.opened ?? 0).arg(Hub.dateText(dialog.status.expiresAt ?? 0))
                              + ((dialog.status.applicationsViaLinks ?? 0) > 0 ? " · " + qsTr("%n application(s) through links", "", dialog.status.applicationsViaLinks) : "")
                            : qsTr("Make one and share it in your group chat or by email.")
    }
    RowLayout {
        visible: dialog.showsUrl
        Layout.fillWidth: true
        spacing: 8
        TfTextField {
            text: dialog.link ? dialog.link.url : ""
            readOnly: true
            selectByMouse: true
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Accessible.name: qsTr("Invitation link")
        }
        TfButton {
            text: dialog.copied ? qsTr("Copied ✓") : qsTr("Copy link")
            variant: "primary"
            compact: true
            onClicked: { Hub.copyText(dialog.link.url); dialog.copied = true; dialog.notify(qsTr("Link copied")) }
        }
    }
    TfText {
        visible: dialog.active && !dialog.showsUrl
        text: qsTr("For security TeamForge keeps only a fingerprint of the link, so it cannot be shown again. To share it again, make a new link (the old one stops working).")
        variant: "muted"
        font.pixelSize: Theme.fontXs
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    HubLabel { text: qsTr("Valid for") }
    ScrollRow {
        Layout.fillWidth: true
        Segmented {
            options: [{ text: qsTr("1 day"), value: 1 }, { text: qsTr("7 days"), value: 7 },
                      { text: qsTr("14 days"), value: 14 }, { text: qsTr("30 days"), value: 30 }]
            currentValue: dialog.days
            onActivated: value => dialog.days = value
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        TfButton {
            text: dialog.active ? qsTr("Make a new link") : qsTr("Create link")
            variant: dialog.active ? "secondary" : "primary"
            compact: true
            enabled: !dialog.busy
            onClicked: dialog.make()
        }
        TfButton {
            visible: dialog.active
            text: qsTr("Revoke link")
            variant: "danger"
            compact: true
            enabled: !dialog.busy
            onClicked: dialog.revoke()
        }
    }
}
