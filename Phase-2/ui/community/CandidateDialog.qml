pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// A candidate's or applicant's profile as a team leader sees it: identity, introduction, links,
// teams, the compatibility report for this team, skills with levels, projects and education.
// Only what the person put in their profiles (never an email address).
HubDialog {
    id: dialog

    property var person: ({})
    property var report: ({})
    property var participant: ({})
    property bool canInvite: false
    signal inviteRequested(var person, var report)

    title: person.name ?? ""
    subtitle: [participant.role || person.headline, participant.program || person.program].filter(t => !!t).join(" · ")
    primaryText: canInvite ? qsTr("Invite to the team") : qsTr("Done")
    onAccepted: {
        if (canInvite)
            dialog.inviteRequested(person, report)
        close()
    }

    function show(candidate, compatibility, invitable) {
        person = candidate ?? {}
        report = compatibility ?? {}
        participant = Backend.participantCard(person.studentId ?? "")
        canInvite = invitable === true
        open()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 14
        Avatar { person: dialog.person; size: 64 }
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            TfText {
                visible: text !== ""
                text: dialog.person.bio || dialog.participant.summary || ""
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
            }
            TfText {
                visible: (dialog.person.teams ?? []).length > 0
                text: qsTr("Teams: %1").arg((dialog.person.teams ?? []).map(t => t.name).join(", "))
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    // links (opened in the browser)
    Repeater {
        model: dialog.person.links ?? []
        delegate: TfText {
            id: linkText
            required property string modelData
            text: "↗  " + modelData
            color: Theme.info
            font.weight: Font.Bold
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Qt.openUrlExternally(linkText.modelData) }
            Accessible.role: Accessible.Link
            Accessible.name: modelData
        }
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    TfText { text: qsTr("Compatibility with this team"); variant: "section"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    CompatibilityView { report: dialog.report; Layout.fillWidth: true }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
    TfText { text: qsTr("Skills"); variant: "section"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    TfText {
        visible: (dialog.participant.skills ?? []).length === 0 && (dialog.person.skills ?? []).length === 0
        text: qsTr("No skills in their profile yet.")
        variant: "muted"
    }
    Repeater {
        model: dialog.participant.skills ?? []
        delegate: RowLayout {
            id: skillRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            TfText { text: Theme.skill(skillRow.modelData.skill); font.weight: Font.Bold; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            TfText { text: Theme.levelName(skillRow.modelData.level); variant: "muted"; font.pixelSize: Theme.fontXs }
            LevelPips { level: skillRow.modelData.level }
        }
    }
    Flow {
        // Community profile skills without levels (people without a participant profile)
        visible: (dialog.participant.skills ?? []).length === 0 && (dialog.person.skills ?? []).length > 0
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: dialog.person.skills ?? []
            delegate: Chip { required property string modelData; text: Theme.skill(modelData); skill: true }
        }
    }
    TfText {
        text: qsTr("Levels are self-assessed by the person. TeamForge does not verify skills.")
        variant: "muted"
        font.pixelSize: Theme.fontXs
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    // experience, projects, education
    Repeater {
        model: [
            { label: qsTr("Experience"), items: (dialog.person.experience ?? "") !== "" ? [dialog.person.experience] : [] },
            { label: qsTr("Projects"), items: (dialog.participant.details ?? {}).projects ?? [] },
            { label: qsTr("Education"), items: (dialog.participant.details ?? {}).education ?? [] },
            { label: qsTr("Certifications"), items: (dialog.participant.details ?? {}).certifications ?? [] },
            { label: qsTr("Interests"), items: (dialog.person.interests ?? "") !== "" ? [dialog.person.interests] : [] }
        ].filter(s => s.items.length > 0)
        delegate: ColumnLayout {
            id: section
            required property var modelData
            Layout.fillWidth: true
            spacing: 4
            TfText { text: section.modelData.label; variant: "label" }
            Repeater {
                model: section.modelData.items
                delegate: TfText {
                    required property string modelData
                    text: (section.modelData.items.length > 1 ? "•  " : "") + modelData
                    variant: "secondary"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
        }
    }
}
