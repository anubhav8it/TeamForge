pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// "View opportunity": one project in full from the participant's side: what it needs, how
// their skills compare, a preview of the team, and where their interest stands. Expressing
// interest is the one action; it sits in a footer that is always in view, under a
// collaboration quote (MotivationQuote), so nothing has to be watched or scrolled first.
Drawer {
    id: drawer

    signal whyThisMatch(string requirementId)
    // `note`: the optional message for the host ("" for none).
    signal expressInterest(string requirementId, string note)
    readonly property string status: opportunity.interestStatus ?? ""
    readonly property string sentNote: opportunity.interestNote ?? ""
    readonly property string hostReply: opportunity.interestReply ?? ""
    readonly property bool noteEditable: opportunity.noteEditable ?? false
    property bool writingNote: false   // the note box is open in the footer (before sending)
    property bool editingNote: false   // the sent note is being changed (until the host reviews it)

    property string requirementId
    readonly property var opportunity: Backend.opportunities.find(o => o.id === requirementId) ?? ({})
    readonly property var me: Backend.myProfile
    // Key skills the participant has at any level, strongest first (from the backend's keySkills).
    readonly property var relevantSkills: (opportunity.keySkills ?? []).filter(k => k.level > 0)
        .map(k => ({ skill: k.skill, level: k.level })).sort((a, b) => b.level - a.level)
    // Members of the project's saved teams, each once.
    readonly property var teamMembers: {
        const seen = new Set()
        const members = []
        for (const team of Backend.savedTeams.filter(t => t.requirementId === requirementId)) {
            for (const member of team.members) {
                if (!seen.has(member.id)) {
                    seen.add(member.id)
                    members.push(member)
                }
            }
        }
        return members
    }

    function show(id) {
        requirementId = id
        writingNote = false
        editingNote = false
        noteBox.clear()
        open()
    }

    parent: Overlay.overlay
    edge: Qt.RightEdge
    // One computed width for the drawer and its content, so the drawer's implicit size never
    // depends on its own width or wrapped text (binding loop).
    readonly property real panelWidth: Math.min(540, Math.max(440, (parent?.width ?? 1200) * 0.4))
    width: panelWidth
    height: parent.height
    contentWidth: panelWidth
    modal: true
    dim: true
    padding: 0

    Overlay.modal: Rectangle { color: "#8C0B0D11" }
    background: Rectangle {
        color: Theme.surface
        Rectangle { width: 1; height: parent.height; color: Theme.borderStrong }
    }

    contentItem: ColumnLayout {
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 22

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true
                        Chip {
                            visible: (drawer.opportunity.type ?? "") !== ""
                            text: drawer.opportunity.type ?? ""
                            tone: "tint"
                            tint: Theme.typeColor(drawer.opportunity.type ?? "")
                        }
                        TfText {
                            text: drawer.opportunity.name ?? ""
                            font.pixelSize: 28
                            font.weight: Font.ExtraBold
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                        }
                        TfText {
                            visible: (drawer.opportunity.summary ?? "") !== ""
                            text: drawer.opportunity.summary ?? ""
                            variant: "secondary"
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                    TfButton {
                        text: "✕"
                        variant: "ghost"
                        compact: true
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 36
                        Accessible.name: qsTr("Close opportunity")
                        onClicked: drawer.close()
                    }
                }

                // Fit at a glance
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    implicitHeight: fitRow.implicitHeight + 36
                    radius: Theme.radius
                    color: Theme.surfaceRaised
                    border.color: Theme.border
                    RowLayout {
                        id: fitRow
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 18
                        ColumnLayout {
                            spacing: -4
                            TfText {
                                text: Theme.percent(drawer.opportunity.score ?? 0)
                                font.pixelSize: 44
                                font.weight: Font.ExtraBold
                                font.letterSpacing: -1
                                color: Theme.scoreColor(drawer.opportunity.score ?? 0)
                            }
                            TfText { text: qsTr("your match"); variant: "muted"; font.weight: Font.Bold }
                        }
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText {
                                text: Theme.fitSummary(drawer.opportunity)
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            TfText {
                                text: qsTr("Team of %1–%2").arg(drawer.opportunity.minTeamSize ?? 0).arg(drawer.opportunity.maxTeamSize ?? 0)
                                variant: "muted"
                                font.weight: Font.Bold
                            }
                            RowLayout {
                                spacing: 8
                                Chip {
                                    visible: drawer.status !== ""
                                    large: true
                                    text: Theme.interestLabel(drawer.status)
                                    tone: Theme.interestTone(drawer.status)
                                }
                                TfButton {
                                    text: qsTr("Why this match?")
                                    compact: true
                                    onClicked: drawer.whyThisMatch(drawer.requirementId)
                                }
                            }
                        }
                    }
                }

                // Skills: what it needs vs what you have
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 0
                    TfText {
                        text: qsTr("What the team needs")
                        variant: "heading"
                        font.pixelSize: Theme.fontLg
                        Layout.bottomMargin: 10
                    }
                    Repeater {
                        model: drawer.opportunity.keySkills ?? []
                        delegate: Rectangle {
                            id: needRow
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 48
                            color: "transparent"
                            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                            RowLayout {
                                anchors.fill: parent
                                spacing: 12
                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillWidth: true
                                    TfText {
                                        text: Theme.skill(needRow.modelData.skill)
                                        font.weight: Font.ExtraBold
                                        font.letterSpacing: 0.4
                                        Layout.fillWidth: true
                                    }
                                    TfText {
                                        text: needRow.modelData.level > 0
                                              ? qsTr("You: level %1 · needs %2+").arg(needRow.modelData.level).arg(needRow.modelData.minLevel)
                                              : qsTr("Needs level %1+").arg(needRow.modelData.minLevel)
                                        variant: "muted"
                                        font.pixelSize: Theme.fontXs
                                    }
                                }
                                LevelPips {
                                    level: needRow.modelData.level
                                    minLevel: needRow.modelData.minLevel
                                }
                                Chip {
                                    Layout.preferredWidth: 104
                                    text: needRow.modelData.covered ? qsTr("You have it")
                                                                    : needRow.modelData.level > 0 ? qsTr("Level up") : qsTr("Not yet")
                                    tone: needRow.modelData.covered ? "success" : needRow.modelData.level > 0 ? "warning" : "muted"
                                }
                            }
                        }
                    }
                }

                // Team preview: you, what you bring, and who is already on a team for this project.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 12
                    TfText {
                        text: qsTr("Team preview")
                        variant: "heading"
                        font.pixelSize: Theme.fontLg
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: youRow.implicitHeight + 28
                        radius: Theme.radius
                        color: Theme.surfaceRaised
                        border.color: Theme.border
                        RowLayout {
                            id: youRow
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14
                            Avatar {
                                name: drawer.me.name ?? "?"
                                seed: drawer.me.id ?? ""
                                size: 52
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                spacing: 6
                                Layout.fillWidth: true
                                TfText {
                                    text: drawer.me.name ?? ""
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.ExtraBold
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    text: drawer.relevantSkills.length > 0 ? qsTr("You bring")
                                                                           : qsTr("None of the key skills yet: a chance to learn")
                                    variant: "muted"
                                    font.weight: Font.Bold
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                SkillChips {
                                    visible: drawer.relevantSkills.length > 0
                                    skills: drawer.relevantSkills
                                    maxVisible: 4
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                    TfText {
                        text: drawer.teamMembers.length > 0 ? qsTr("Already on the team") : qsTr("No team saved for this project yet.")
                        variant: drawer.teamMembers.length > 0 ? "label" : "muted"
                    }
                    Flow {
                        visible: drawer.teamMembers.length > 0
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: drawer.teamMembers
                            delegate: Row {
                                id: member
                                required property var modelData
                                spacing: 8
                                Avatar {
                                    name: member.modelData.name
                                    seed: member.modelData.id
                                    size: 30
                                }
                                TfText {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: member.modelData.name
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }

                // Messages: the note sent with the interest and the host's reply.
                ColumnLayout {
                    visible: drawer.status !== ""
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 10
                    TfText {
                        text: qsTr("Messages")
                        variant: "heading"
                        font.pixelSize: Theme.fontLg
                    }
                    TfText { text: qsTr("Your note"); variant: "label" }
                    TfText {
                        visible: !drawer.editingNote
                        text: drawer.sentNote !== "" ? drawer.sentNote : qsTr("No note sent.")
                        variant: drawer.sentNote !== "" ? "body" : "muted"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                    TfTextArea {
                        id: editBox
                        visible: drawer.editingNote
                        placeholderText: qsTr("A short note for the host")
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        visible: drawer.noteEditable
                        spacing: 8
                        TfButton {
                            visible: !drawer.editingNote
                            text: drawer.sentNote !== "" ? qsTr("Edit note") : qsTr("Add a note")
                            compact: true
                            onClicked: {
                                editBox.text = drawer.sentNote
                                drawer.editingNote = true
                                editBox.focusField()
                            }
                        }
                        TfButton {
                            visible: drawer.editingNote
                            text: qsTr("Save note")
                            variant: "primary"
                            compact: true
                            enabled: !editBox.tooLong
                            onClicked: if (Backend.updateInterestNote(drawer.requirementId, editBox.text)) drawer.editingNote = false
                        }
                        TfButton {
                            visible: drawer.editingNote
                            text: qsTr("Cancel")
                            variant: "ghost"
                            compact: true
                            onClicked: drawer.editingNote = false
                        }
                    }
                    TfText {
                        visible: !drawer.noteEditable
                        text: qsTr("The host is reviewing your interest, so the note stays as they read it.")
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        visible: drawer.hostReply !== ""
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        implicitHeight: replyColumn.implicitHeight + 24
                        radius: Theme.radiusSmall
                        color: Theme.infoSubtle
                        border.color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.4)
                        ColumnLayout {
                            id: replyColumn
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4
                            TfText { text: qsTr("Reply from the host"); variant: "label"; color: Theme.info }
                            TfText {
                                text: drawer.hostReply
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // What happens next
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 28
                    spacing: 10
                    TfText {
                        text: qsTr("How joining works")
                        variant: "heading"
                        font.pixelSize: Theme.fontLg
                    }
                    Repeater {
                        model: [[qsTr("Express interest"), drawer.status !== ""],
                                [qsTr("The host reviews your profile"), drawer.status === "under_review" || drawer.status === "accepted" || drawer.status === "declined"],
                                [drawer.status === "declined" ? qsTr("Not selected this time") : qsTr("Accepted: you can be added to the team"),
                                 drawer.status === "accepted" || drawer.status === "declined"]]
                        delegate: RowLayout {
                            id: stepRow
                            required property var modelData
                            required property int index
                            spacing: 12
                            Rectangle {
                                implicitWidth: 26
                                implicitHeight: 26
                                radius: 13
                                color: stepRow.modelData[1] ? Theme.success : "transparent"
                                border.width: stepRow.modelData[1] ? 0 : 1.5
                                border.color: Theme.borderStrong
                                Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                                Text {
                                    anchors.centerIn: parent
                                    text: stepRow.modelData[1] ? "✓" : String(stepRow.index + 1)
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 13
                                    font.weight: Font.ExtraBold
                                    color: stepRow.modelData[1] ? "#0E2A1E" : Theme.textSecondary
                                }
                            }
                            TfText {
                                text: stepRow.modelData[0]
                                font.weight: Font.Bold
                                color: stepRow.modelData[1] ? Theme.text : Theme.textSecondary
                            }
                        }
                    }
                }
            }
        }

        // Footer, always in view: a collaboration quote, then the one action. Once interest is
        // sent the quote steps aside and the status takes its place.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footer.implicitHeight + 36
            color: Theme.surface
            Rectangle { width: parent.width; height: 1; color: Theme.border }
            ColumnLayout {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 14
                MotivationQuote {
                    visible: drawer.status === ""
                    moment: "join"
                    Layout.fillWidth: true
                }
                // An optional note for the host, opened on request so the footer stays small.
                TfButton {
                    visible: drawer.status === "" && !drawer.writingNote
                    text: qsTr("+ Add a note for the host (optional)")
                    variant: "ghost"
                    compact: true
                    onClicked: {
                        drawer.writingNote = true
                        noteBox.focusField()
                    }
                }
                TfTextArea {
                    id: noteBox
                    visible: drawer.status === "" && drawer.writingNote
                    placeholderText: qsTr("What you'd like to work on, or experience the skills list doesn't show")
                    Layout.fillWidth: true
                }
                TfButton {
                    visible: drawer.status === ""
                    text: qsTr("Express interest")
                    variant: "primary"
                    enabled: !noteBox.tooLong
                    Layout.fillWidth: true
                    onClicked: drawer.expressInterest(drawer.requirementId, drawer.writingNote ? noteBox.text : "")
                }
                TfText {
                    visible: drawer.status !== ""
                    text: qsTr("Interest sent · %1").arg(Theme.interestLabel(drawer.status))
                    variant: "secondary"
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                }
            }
        }
    }
}
