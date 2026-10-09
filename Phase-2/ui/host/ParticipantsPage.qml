pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Searchable participant list and a concise profile. Search, filtering and sorting run in C++
// (Backend.searchStudents); fit for the active project comes from Backend.evaluateCandidate.
TfPage {
    id: page

    signal explain(string studentId)
    signal notify(string message)

    property string selectedId: ""
    property string sortMode: "name"
    property string query: "" // search text, applied 150 ms after typing stops

    readonly property var selected: Backend.students.find(s => s.id === selectedId) ?? null
    readonly property var project: Backend.currentRequirement
    readonly property var requiredNames: (project.requiredSkills ?? []).map(s => s.skill)
    readonly property var memberIds: Backend.currentTeam.memberIds ?? []
    readonly property bool isMember: memberIds.indexOf(selectedId) >= 0
    property var fit: ({})

    // Re-evaluated when the query, filter, sort or the data (Backend.students) changes.
    readonly property var filtered: Backend.students.length >= 0
                                    ? Backend.searchStudents(page.query,
                                                             skillFilter.currentIndex > 0 ? Backend.skills[skillFilter.currentIndex - 1] : "",
                                                             page.sortMode)
                                    : []

    // A person filtered out of the list is no longer shown as selected.
    onFilteredChanged: {
        if (!filtered.some(s => s.id === selectedId))
            selectedId = filtered.length > 0 ? filtered[0].id : ""
    }

    function refreshFit() {
        fit = selectedId !== "" && project.id !== undefined ? Backend.evaluateCandidate(selectedId) : ({})
    }
    function experienceTone(level) {
        return level >= 4 ? "success" : level >= 3 ? "info" : level >= 2 ? "warning" : "muted"
    }

    onSelectedIdChanged: refreshFit()
    onActiveChanged: if (active) refreshFit()
    Connections {
        target: Backend
        function onSelectionChanged() { if (page.active) page.refreshFit() }
    }
    Component.onCompleted: if (Backend.students.length > 0) selectedId = filtered[0]?.id ?? ""

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: Theme.gap

        // List
        Panel {
            title: qsTr("People")
            subtitle: page.filtered.length === Backend.students.length
                      ? qsTr("%1 participants").arg(Backend.students.length)
                      : qsTr("%1 of %2 shown").arg(page.filtered.length).arg(Backend.students.length)
            padded: false
            Layout.preferredWidth: 430
            Layout.minimumWidth: 360
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pad
                    Layout.rightMargin: Theme.pad
                    Layout.bottomMargin: 12
                    spacing: 10
                    TfTextField {
                        id: searchField
                        placeholderText: qsTr("Search by name or skill")
                        Layout.fillWidth: true
                        onTextChanged: searchDelay.restart()
                        Timer {
                            id: searchDelay
                            interval: 150
                            onTriggered: page.query = searchField.text
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        TfComboBox {
                            id: skillFilter
                            model: [qsTr("Any skill")].concat(Backend.skills)
                            format: text => text === qsTr("Any skill") ? text : Theme.skill(text)
                            Layout.fillWidth: true
                        }
                        Segmented {
                            options: [{ text: qsTr("A–Z"), value: "name" }, { text: qsTr("Most skilled"), value: "level" }]
                            currentValue: page.sortMode
                            onActivated: value => page.sortMode = value
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    reuseItems: true
                    model: page.filtered
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        readonly property bool selected: modelData.id === page.selectedId
                        readonly property bool inTeam: page.memberIds.indexOf(modelData.id) >= 0
                        Accessible.role: Accessible.ListItem
                        Accessible.name: modelData.name
                        Accessible.onPressAction: page.selectedId = row.modelData.id
                        width: ListView.view.width
                        implicitHeight: 86
                        color: selected ? Theme.selection : rowHover.hovered ? Theme.surfaceRaised : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                        Rectangle {
                            visible: row.selected
                            width: 4
                            height: parent.height
                            color: Theme.accent
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.pad
                            anchors.rightMargin: Theme.pad
                            spacing: 12
                            Avatar {
                                name: row.modelData.name
                                seed: row.modelData.id
                                size: 44
                                highlighted: row.inTeam
                            }
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    TfText {
                                        text: row.modelData.name
                                        font.pixelSize: Theme.fontMd
                                        font.weight: Font.ExtraBold
                                        Layout.fillWidth: true
                                    }
                                    Chip {
                                        visible: row.inTeam
                                        text: qsTr("In team")
                                        tone: "accent"
                                    }
                                }
                                // Up to two lines so the one-sentence summary stays readable.
                                TfText {
                                    text: row.modelData.summary || row.modelData.skillsOffered.map(s => Theme.skill(s.skill)).join(" · ")
                                    variant: "muted"
                                    font.pixelSize: Theme.fontXs
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    lineHeight: 1.1
                                    Layout.fillWidth: true
                                }
                            }
                            ColumnLayout {
                                spacing: 0
                                Layout.preferredWidth: 84
                                Layout.fillWidth: false
                                TfText {
                                    text: row.modelData.averageLevel.toFixed(1)
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.ExtraBold
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    text: Theme.experienceLabel(row.modelData.averageLevel)
                                    variant: "muted"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        HoverHandler {
                            id: rowHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: page.selectedId = row.modelData.id
                        }
                    }

                    EmptyState {
                        anchors.centerIn: parent
                        width: parent.width - 40
                        visible: page.filtered.length === 0
                        title: qsTr("No participants match")
                        message: qsTr("Try a different name, skill or filter.")
                    }
                }
            }
        }

        // Profile
        Panel {
            visible: page.selected !== null
            padded: false
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 22

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 24
                        Layout.bottomMargin: 0
                        spacing: 18
                        Avatar {
                            name: page.selected?.name ?? ""
                            seed: page.selectedId
                            size: 80
                            highlighted: page.isMember
                            Layout.alignment: Qt.AlignTop
                        }
                        ColumnLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            TfText {
                                text: page.selected?.name ?? ""
                                variant: "title"
                                Layout.fillWidth: true
                            }
                            TfText {
                                visible: (page.selected?.program ?? "") !== ""
                                text: page.selected?.program ?? ""
                                variant: "muted"
                                font.weight: Font.Bold
                                Layout.fillWidth: true
                            }
                            TfText {
                                visible: (page.selected?.summary ?? "") !== ""
                                text: page.selected?.summary ?? ""
                                variant: "secondary"
                                font.pixelSize: Theme.fontMd
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                Layout.fillWidth: true
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.topMargin: 2
                                spacing: 6
                                Chip {
                                    text: Theme.experienceLabel(page.selected?.averageLevel ?? 0)
                                          + " · " + qsTr("avg level %1").arg((page.selected?.averageLevel ?? 0).toFixed(1))
                                    tone: page.experienceTone(page.selected?.averageLevel ?? 0)
                                }
                                Chip {
                                    visible: page.isMember
                                    text: qsTr("In your team")
                                    tone: "accent"
                                }
                            }
                        }
                    }

                    // Fit for the active project
                    Rectangle {
                        visible: page.fit.score !== undefined
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
                            spacing: 20
                            ColumnLayout {
                                spacing: -4
                                TfText {
                                    text: Theme.percent(page.fit.score ?? 0)
                                    font.pixelSize: 40
                                    font.weight: Font.ExtraBold
                                    font.letterSpacing: -1
                                    color: Theme.scoreColor(page.fit.score ?? 0)
                                }
                                TfText {
                                    text: qsTr("project fit")
                                    variant: "muted"
                                    font.weight: Font.Bold
                                }
                            }
                            ColumnLayout {
                                spacing: 6
                                Layout.fillWidth: true
                                TfText {
                                    text: page.project.name ?? ""
                                    font.weight: Font.ExtraBold
                                    Layout.fillWidth: true
                                }
                                TfText {
                                    text: qsTr("Meets %1 of %2 required skills")
                                          .arg((page.fit.skillBreakdown ?? []).filter(s => s.status === "covered").length)
                                          .arg((page.fit.skillBreakdown ?? []).length)
                                    variant: "secondary"
                                    font.weight: Font.Bold
                                    Layout.fillWidth: true
                                }
                                MeterBar {
                                    Layout.fillWidth: true
                                    value: page.fit.score ?? 0
                                    fillColor: Theme.scoreColor(page.fit.score ?? 0)
                                }
                            }
                            ColumnLayout {
                                spacing: 8
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 168
                                Layout.fillWidth: false
                                TfButton {
                                    text: page.isMember ? qsTr("Remove from team") : qsTr("Add to team")
                                    variant: page.isMember ? "secondary" : "primary"
                                    enabled: page.isMember || !(Backend.currentTeam.isFull ?? true)
                                    Layout.fillWidth: true
                                    onClicked: {
                                        const name = page.selected?.name ?? ""
                                        if (page.isMember) {
                                            if (Backend.removeFromTeam(page.selectedId))
                                                page.notify(qsTr("%1 removed from your team").arg(name))
                                        } else if (Backend.addToTeam(page.selectedId)) {
                                            page.notify(qsTr("%1 added to your team").arg(name))
                                        }
                                    }
                                }
                                TfButton {
                                    text: qsTr("Why this match?")
                                    variant: "secondary"
                                    Layout.fillWidth: true
                                    onClicked: page.explain(page.selectedId)
                                }
                            }
                        }
                    }

                    // Strongest skills
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        spacing: 0
                        TfText {
                            text: qsTr("Skills")
                            variant: "heading"
                            font.pixelSize: Theme.fontLg
                            Layout.bottomMargin: 8
                        }
                        Repeater {
                            model: (page.selected?.skillsOffered ?? []).slice().sort((a, b) => b.level - a.level)
                            delegate: Rectangle {
                                id: skillRow
                                required property var modelData
                                readonly property var requirement: (page.project.requiredSkills ?? [])
                                                                   .find(s => s.skill === modelData.skill)
                                Layout.fillWidth: true
                                implicitHeight: 42
                                color: "transparent"
                                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 14
                                    TfText {
                                        text: Theme.skill(skillRow.modelData.skill)
                                        font.pixelSize: Theme.fontSm
                                        font.weight: Font.ExtraBold
                                        font.letterSpacing: 0.4
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 150
                                        Layout.maximumWidth: 200
                                    }
                                    LevelPips {
                                        level: skillRow.modelData.level
                                        minLevel: skillRow.requirement?.minLevel ?? 0
                                    }
                                    TfText {
                                        text: qsTr("Level %1").arg(skillRow.modelData.level)
                                        variant: "secondary"
                                        font.weight: Font.Bold
                                        Layout.preferredWidth: 64
                                    }
                                    Item { Layout.fillWidth: true }
                                    Chip {
                                        visible: skillRow.requirement !== undefined
                                        text: skillRow.modelData.level >= (skillRow.requirement?.minLevel ?? 0)
                                              ? qsTr("Meets project need") : qsTr("Below project need")
                                        tone: skillRow.modelData.level >= (skillRow.requirement?.minLevel ?? 0) ? "success" : "warning"
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 24
                        Layout.rightMargin: 24
                        Layout.bottomMargin: 24
                        spacing: 36

                        ColumnLayout {
                            spacing: 10
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            TfText { text: qsTr("Wants to learn"); variant: "heading"; font.pixelSize: Theme.fontLg }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: page.selected?.skillsWanted ?? []
                                    delegate: Chip {
                                        required property string modelData
                                        skill: true
                                        text: modelData
                                        tone: "muted"
                                    }
                                }
                            }
                            TfText {
                                visible: (page.selected?.skillsWanted ?? []).length === 0
                                text: qsTr("Nothing listed")
                                variant: "muted"
                            }
                        }
                    }
                }
            }
        }

        Panel {
            visible: page.selected === null
            Layout.fillWidth: true
            Layout.fillHeight: true
            EmptyState {
                anchors.centerIn: parent
                width: 340
                title: qsTr("Select a participant")
                message: qsTr("Their skills, experience and learning interests appear here.")
            }
        }
    }
}
