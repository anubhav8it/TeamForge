pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Find members: search the registered TeamForge people who are open to team invitations (the
// server decides who is listed and checks that you lead or help run the team), see each person's
// compatibility with the team, open their profile and invite them. Ranked by the compatibility
// score, which already favours people who cover skills the team lacks.
Popup {
    id: finder

    property var team: null
    property var candidates: []       // [{ person, report, participant }]
    property bool loading: false
    property string error: ""
    property bool truncated: false
    property string sort: "match"     // match | gaps | name
    signal invited(string name)
    signal reviewApplications()

    readonly property bool phone: parent ? parent.width < 600 : false
    readonly property var coverage: team ? Backend.teamSkillCoverage(team) : []
    readonly property var missing: coverage.filter(c => c.state !== "covered").map(c => Theme.skill(c.skill))
    readonly property var sorted: {
        const list = candidates.slice()
        if (sort === "name")
            return list.sort((a, b) => a.person.name.localeCompare(b.person.name))
        const known = c => c.report.enoughInformation ? 1 : 0
        const gaps = c => (c.report.matching ?? []).filter(f => f.fillsGap).length
        if (sort === "gaps")
            return list.sort((a, b) => gaps(b) - gaps(a) || (b.report.score ?? 0) - (a.report.score ?? 0))
        return list.sort((a, b) => known(b) - known(a) || (b.report.score ?? 0) - (a.report.score ?? 0)
                                   || a.person.name.localeCompare(b.person.name))
    }

    function openFor(theTeam) {
        team = theTeam
        search.text = ""
        candidates = []
        open()
        load()
    }
    function load() {
        if (!team)
            return
        loading = true
        error = ""
        const q = search.text.trim()
        Hub.get("/teams/" + team.id + "/candidates" + (q !== "" ? "?q=" + encodeURIComponent(q) : ""), (status, data) => {
            loading = false
            if (status !== 200) {
                error = data.error ?? qsTr("People could not be loaded.")
                candidates = []
                return
            }
            truncated = data.truncated
            candidates = data.items.map(p => ({
                person: p,
                report: Backend.teamCompatibility(finder.team, p),
                participant: Backend.participantCard(p.studentId ?? "")
            }))
        })
    }
    function markInvited(personId) {
        candidates = candidates.map(c => c.person.id === personId
                                    ? Object.assign({}, c, { person: Object.assign({}, c.person, { pendingInvitation: -1 }) }) : c)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: phone ? parent.width : Math.min(980, parent.width - 48)
    height: phone ? parent.height : Math.min(parent.height - 48, 860)
    modal: true
    dim: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#B3050810" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    background: Surface { radius: finder.phone ? 0 : Theme.radiusLarge; baseColor: Theme.surfaceRaised; border.color: Theme.borderStrong }

    Timer { id: debounce; interval: 350; onTriggered: finder.load() }
    InviteDialog { id: inviteDialog; onInvited: p => { finder.markInvited(p.id); finder.invited(p.name) } }
    CandidateDialog { id: profileDialog; onInviteRequested: (p, r) => inviteDialog.openFor(finder.team, p, r) }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- header
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: finder.phone ? 14 : 22
            Layout.bottomMargin: 12
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                TeamAvatar { name: finder.team ? finder.team.name : ""; size: 44 }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    TfText {
                        text: qsTr("Find members for %1").arg(finder.team ? finder.team.name : "")
                        variant: "section"
                        font.pixelSize: finder.phone ? 17 : 20
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TfText {
                        text: finder.team ? (finder.team.full ? qsTr("The team is full: raise the team size to invite more people.")
                                                              : qsTr("%n place(s) left", "", finder.team.vacancies ?? 0)
                                                                + (finder.missing.length > 0 ? " · " + qsTr("still needs %1").arg(finder.missing.join(", ")) : ""))
                                          : ""
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                TfButton {
                    text: "✕"
                    variant: "ghost"
                    compact: true
                    Accessible.name: qsTr("Close")
                    onClicked: finder.close()
                }
            }
            TfTextField {
                id: search
                placeholderText: qsTr("Search by name, skill, language, project, education or interest")
                Layout.fillWidth: true
                onTextChanged: debounce.restart()
                Keys.onReturnPressed: { debounce.stop(); finder.load() }
            }
            ScrollRow {
                Layout.fillWidth: true
                Segmented {
                    options: [{ text: qsTr("Best match"), value: "match" }, { text: qsTr("Fills gaps"), value: "gaps" },
                              { text: qsTr("Name"), value: "name" }]
                    currentValue: finder.sort
                    onActivated: value => finder.sort = value
                }
            }
            TfText {
                text: finder.loading ? qsTr("Searching…")
                      : qsTr("%n person/people", "", finder.candidates.length)
                        + (finder.truncated ? " · " + qsTr("showing the first 200: refine your search") : "")
                        + " · " + qsTr("scores compare each profile with the team's required skills and current members")
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

        // ---- results
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            HubSkeleton { anchors.fill: parent; anchors.margins: 20; visible: finder.loading && finder.candidates.length === 0; rows: 3 }
            EmptyState {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 32)
                visible: !finder.loading && finder.candidates.length === 0
                title: finder.error !== "" ? qsTr("Something went wrong") : search.text.trim() !== "" ? qsTr("Nobody matches that search")
                                                                                                      : qsTr("Nobody to invite yet")
                message: finder.error !== "" ? finder.error
                         : search.text.trim() !== "" ? qsTr("Try a skill (python), a language, a project word or part of a name.")
                                                     : qsTr("Only registered people who are open to team invitations appear here. Share an invitation link instead.")
            }
            Flickable {
                id: results
                anchors.fill: parent
                visible: finder.candidates.length > 0
                clip: true
                contentWidth: width
                contentHeight: grid.implicitHeight + 2 * grid.y
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                GridLayout {
                    id: grid
                    x: finder.phone ? 10 : 16
                    y: x
                    width: results.width - 2 * x
                    columns: width > 700 ? 2 : 1
                    columnSpacing: 12
                    rowSpacing: 12
                    Repeater {
                        model: finder.sorted
                        delegate: CandidateCard {
                            id: cell
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.minimumWidth: 0
                            Layout.alignment: Qt.AlignTop
                            person: modelData.person
                            report: modelData.report
                            participant: modelData.participant
                            teamFull: !!finder.team && finder.team.full
                            onOpenProfile: profileDialog.show(cell.modelData.person, cell.modelData.report,
                                                              !cell.modelData.person.pendingInvitation && !cell.modelData.person.pendingApplication
                                                              && !teamFull)
                            onInvite: inviteDialog.openFor(finder.team, cell.modelData.person, cell.modelData.report)
                            onReviewApplication: { finder.close(); finder.reviewApplications() }
                        }
                    }
                }
            }
        }
    }
}
