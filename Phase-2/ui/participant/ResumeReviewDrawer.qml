pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Review what the resume analysis found (Backend.resume.result) before any of it reaches the
// profile. Every value shows the resume line it came from; skills start ticked, items can be
// edited or removed, and skills TeamForge does not know yet are only added on request. Levels
// come from the years the resume states for that skill (core rule, Theme.levelName); where it
// states none, the person picks. "Add to profile" only fills the profile draft: saving the
// profile is still the editor's job.
Drawer {
    id: drawer

    // The editor's current draft skills ([{skill, level}]), to show what is already there.
    property var draftSkills: []
    signal applied(var changes)

    // A snapshot taken when the drawer opens (load()); a binding could still hold the previous
    // result when "ready" opens the drawer.
    property var result: ({})
    property string nameText: ""
    property bool useName: false
    property bool useTotal: false
    property bool keepCopy: false
    property var unmatched: []
    property var lists: ({})
    property int revision: 0

    readonly property var listSections: [
        { key: "projects", title: qsTr("Projects") },
        { key: "education", title: qsTr("Education") },
        { key: "certifications", title: qsTr("Certifications") },
        { key: "achievements", title: qsTr("Achievements") }
    ]

    ListModel { id: rows }

    function currentLevel(skill) {
        const found = draftSkills.find(s => s.skill.toLowerCase() === skill.toLowerCase())
        return found ? found.level : 0
    }
    function load() {
        result = Backend.resume.result ?? {}
        rows.clear()
        for (const s of result.skills ?? []) {
            const current = currentLevel(s.skill)
            rows.append({ skill: s.skill, category: s.category, evidence: s.evidence, years: s.years,
                          yearsEvidence: s.yearsEvidence, suggested: s.suggestedLevel, current: current,
                          level: s.suggestedLevel > 0 ? s.suggestedLevel : current, include: true, isNew: false })
        }
        nameText = result.name ?? ""
        useName = nameText !== ""
        useTotal = (result.totalYears ?? -1) >= 0
        keepCopy = false
        unmatched = (result.unmatchedSkills ?? []).slice()
        const copy = {}
        for (const section of listSections)
            copy[section.key] = (result[section.key] ?? []).slice()
        lists = copy
        revision++
    }
    function addUnmatched(name) {
        rows.append({ skill: name, category: qsTr("New to TeamForge"), evidence: "", years: -1, yearsEvidence: "",
                      suggested: 0, current: currentLevel(name), level: currentLevel(name), include: true, isNew: true })
        unmatched = unmatched.filter(u => u !== name)
        revision++
    }
    function removeItem(key, index) {
        const copy = Object.assign({}, lists)
        copy[key] = lists[key].filter((_, i) => i !== index)
        lists = copy
    }
    function missingLevels() {
        void revision
        let n = 0
        for (let i = 0; i < rows.count; ++i)
            n += rows.get(i).include && rows.get(i).level === 0 ? 1 : 0
        return n
    }
    // Skips the skills whose experience the resume does not state; no level is assumed for them.
    function untickUnrated() {
        for (let i = 0; i < rows.count; ++i)
            if (rows.get(i).level === 0)
                rows.setProperty(i, "include", false)
        revision++
    }
    function apply() {
        const skills = []
        const years = {}
        for (let i = 0; i < rows.count; ++i) {
            const r = rows.get(i)
            if (!r.include || r.level === 0)
                continue
            skills.push({ skill: r.skill, level: r.level })
            if (r.years >= 0)
                years[r.skill] = r.years
        }
        const changes = { name: useName ? nameText.trim() : "", skills: skills, skillYears: years,
                          totalYears: useTotal ? result.totalYears : -1 }
        for (const section of listSections)
            changes[section.key] = lists[section.key].map(t => t.trim()).filter(t => t !== "")
        if (keepCopy)
            Backend.keepResumeCopy()
        applied(changes)
        Backend.clearResume()
        close()
    }

    onAboutToShow: load()

    parent: Overlay.overlay
    edge: Qt.RightEdge
    readonly property real panelWidth: Math.min(680, Math.max(480, (parent?.width ?? 1200) * 0.5))
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

    component SectionTitle: TfText {
        variant: "heading"
        font.pixelSize: Theme.fontLg
        Layout.topMargin: 6
    }
    component Evidence: TfText {
        variant: "muted"
        font.pixelSize: Theme.fontXs
        Layout.fillWidth: true
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        visible: text !== ""
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
                spacing: 14

                // Header
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 24
                    Layout.bottomMargin: 0
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        TfText { text: qsTr("From your resume"); variant: "eyebrow" }
                        TfText {
                            text: qsTr("Review before adding")
                            font.pixelSize: 28
                            font.weight: Font.ExtraBold
                            Layout.fillWidth: true
                        }
                        TfText {
                            text: qsTr("Everything below was read from %1. Untick or correct anything that is wrong; nothing is saved until you save your profile.")
                                  .arg(Backend.resume.fileName ?? qsTr("your file"))
                            variant: "secondary"
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
                        Accessible.name: qsTr("Close review")
                        onClicked: drawer.close()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 10

                    // Name
                    SectionTitle { text: qsTr("Name") }
                    TfText {
                        visible: (drawer.result.name ?? "") === ""
                        text: qsTr("No name was found in the resume.")
                        variant: "muted"
                    }
                    RowLayout {
                        visible: (drawer.result.name ?? "") !== ""
                        Layout.fillWidth: true
                        spacing: 12
                        TfCheck {
                            checked: drawer.useName
                            Accessible.name: qsTr("Use this name")
                            onToggled: value => drawer.useName = value
                        }
                        TfTextField {
                            text: drawer.nameText
                            placeholderText: qsTr("Full name")
                            enabled: drawer.useName
                            Layout.fillWidth: true
                            onTextChanged: if (text !== drawer.nameText) drawer.nameText = text
                        }
                    }
                    Evidence { text: (drawer.result.nameEvidence ?? "") !== "" ? qsTr("Found: “%1”").arg(drawer.result.nameEvidence) : "" }
                    TfText {
                        visible: (drawer.result.age ?? -1) >= 0
                        text: qsTr("An age (%1) is written in the resume. TeamForge does not store it and never uses age for matching.")
                              .arg(drawer.result.age ?? 0)
                        variant: "muted"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    // Skills
                    SectionTitle {
                        text: rows.count > 0 ? qsTr("Skills (%1)").arg(rows.count) : qsTr("Skills")
                    }
                    TfText {
                        text: rows.count > 0
                              ? qsTr("Levels follow the experience the resume states for each skill: up to 1 year Beginner, up to 2 Basic, up to 4 Intermediate, up to 5 Advanced, more than 5 Expert. Where it states none, choose a level.")
                              : qsTr("No skills TeamForge knows were found. You can add skills by hand in the next step.")
                        variant: "muted"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: rows
                        delegate: Rectangle {
                            id: row
                            required property int index
                            required property string skill
                            required property string category
                            required property string evidence
                            required property real years
                            required property string yearsEvidence
                            required property int suggested
                            required property int current
                            required property int level
                            required property bool include
                            required property bool isNew
                            Layout.fillWidth: true
                            implicitHeight: rowBody.implicitHeight + 20
                            radius: Theme.radiusSmall
                            color: include ? Theme.surfaceRaised : "transparent"
                            border.color: include && level === 0 ? Theme.warning : Theme.border
                            opacity: include ? 1 : 0.6
                            ColumnLayout {
                                id: rowBody
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    TfCheck {
                                        checked: row.include
                                        Accessible.name: qsTr("Add %1").arg(row.skill)
                                        onToggled: value => { rows.setProperty(row.index, "include", value); drawer.revision++ }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        TfText {
                                            text: Theme.skill(row.skill)
                                            font.weight: Font.ExtraBold
                                            font.letterSpacing: 0.4
                                            Layout.fillWidth: true
                                        }
                                        TfText {
                                            text: row.category + " · "
                                                  + (row.years >= 0 ? qsTr("%1 yrs → %2").arg(row.years).arg(Theme.levelName(row.suggested))
                                                                    : qsTr("experience not stated"))
                                                  + (row.current > 0 ? " · " + qsTr("in your profile: %1").arg(Theme.levelName(row.current)) : "")
                                            variant: "muted"
                                            font.pixelSize: Theme.fontXs
                                            Layout.fillWidth: true
                                        }
                                    }
                                    LevelPips {
                                        interactive: true
                                        level: row.level
                                        onPicked: value => { rows.setProperty(row.index, "level", value); drawer.revision++ }
                                    }
                                    TfText {
                                        text: row.level > 0 ? Theme.levelName(row.level) : qsTr("Choose")
                                        color: row.level > 0 ? Theme.textSecondary : Theme.warning
                                        font.weight: Font.Bold
                                        elide: Text.ElideNone
                                        Layout.preferredWidth: 116
                                    }
                                }
                                Evidence {
                                    Layout.leftMargin: 34
                                    text: row.yearsEvidence !== "" && row.yearsEvidence !== row.evidence
                                          ? qsTr("“%1” · %2").arg(row.evidence).arg(row.yearsEvidence)
                                          : row.evidence !== "" ? qsTr("“%1”").arg(row.evidence) : ""
                                }
                            }
                        }
                    }

                    // Skills TeamForge does not know yet
                    TfText {
                        visible: drawer.unmatched.length > 0
                        text: qsTr("Listed in your resume but new to TeamForge: add the ones that are real skills")
                        variant: "label"
                        Layout.topMargin: 6
                    }
                    Flow {
                        visible: drawer.unmatched.length > 0
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: drawer.unmatched
                            delegate: Chip {
                                id: newSkill
                                required property string modelData
                                skill: true
                                text: "+ " + modelData
                                tone: newHover.hovered ? "info" : "muted"
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Add %1").arg(modelData)
                                Accessible.onPressAction: drawer.addUnmatched(newSkill.modelData)
                                HoverHandler { id: newHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: drawer.addUnmatched(newSkill.modelData) }
                            }
                        }
                    }

                    // Overall experience
                    SectionTitle {
                        visible: (drawer.result.totalYears ?? -1) >= 0
                        text: qsTr("Experience")
                    }
                    TfCheck {
                        visible: (drawer.result.totalYears ?? -1) >= 0
                        checked: drawer.useTotal
                        text: qsTr("Add total experience: %1 years").arg(drawer.result.totalYears ?? 0)
                        onToggled: value => drawer.useTotal = value
                    }
                    Evidence {
                        visible: (drawer.result.totalYears ?? -1) >= 0
                        text: drawer.result.totalYearsEvidence ?? ""
                    }

                    // Projects, education, certifications, achievements
                    Repeater {
                        model: drawer.listSections
                        delegate: ColumnLayout {
                            id: listSection
                            required property var modelData
                            readonly property var items: drawer.lists[modelData.key] ?? []
                            Layout.fillWidth: true
                            spacing: 8
                            SectionTitle {
                                text: listSection.items.length > 0 ? qsTr("%1 (%2)").arg(listSection.modelData.title).arg(listSection.items.length)
                                                                  : listSection.modelData.title
                            }
                            TfText {
                                visible: listSection.items.length === 0
                                text: qsTr("None found.")
                                variant: "muted"
                            }
                            Repeater {
                                model: listSection.items
                                delegate: RowLayout {
                                    id: itemRow
                                    required property string modelData
                                    required property int index
                                    Layout.fillWidth: true
                                    spacing: 8
                                    TfTextField {
                                        text: itemRow.modelData
                                        Layout.fillWidth: true
                                        // Edits stay in the list without rebuilding the rows.
                                        onTextChanged: drawer.lists[listSection.modelData.key][itemRow.index] = text
                                    }
                                    TfButton {
                                        text: "✕"
                                        variant: "ghost"
                                        compact: true
                                        Layout.preferredWidth: 36
                                        Accessible.name: qsTr("Remove")
                                        onClicked: drawer.removeItem(listSection.modelData.key, itemRow.index)
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                }
            }
        }

        // Footer, always in view.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footer.implicitHeight + 32
            color: Theme.surface
            Rectangle { width: parent.width; height: 1; color: Theme.border }
            ColumnLayout {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 10
                TfCheck {
                    visible: Backend.resume.canKeepCopy ?? false
                    checked: drawer.keepCopy
                    text: qsTr("Keep a private copy of the file on this computer")
                    onToggled: value => drawer.keepCopy = value
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    TfText {
                        text: drawer.missingLevels() === 1 ? qsTr("Choose a level for 1 skill, or untick it.")
                            : drawer.missingLevels() > 1 ? qsTr("Choose a level for %1 skills, or untick them.").arg(drawer.missingLevels())
                            : ""
                        color: Theme.warning
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    TfButton {
                        visible: drawer.missingLevels() > 0
                        text: qsTr("Untick unrated")
                        variant: "ghost"
                        ToolTip.visible: hovered
                        ToolTip.delay: 400
                        ToolTip.text: qsTr("Leave out the skills whose experience the resume does not state")
                        onClicked: drawer.untickUnrated()
                    }
                    TfButton {
                        text: qsTr("Cancel")
                        variant: "ghost"
                        onClicked: drawer.close()
                    }
                    TfButton {
                        text: qsTr("Add to profile")
                        variant: "primary"
                        enabled: drawer.missingLevels() === 0
                        onClicked: drawer.apply()
                    }
                }
            }
        }
    }
}
