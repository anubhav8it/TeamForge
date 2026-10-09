pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The complete participant database: every record, filtered and sorted in C++
// (Backend.queryParticipants) and shown in a virtualised table. A row opens the raw record.
TfPage {
    id: page

    signal inspect(string kind, string id)
    signal notify(string message)

    property string query: ""
    property string skill: ""
    property string experience: ""
    property string sort: "name"
    property var result: ({})
    readonly property bool wide: width >= 1180

    function requery() {
        result = Backend.queryParticipants({ query: query, skill: skill, experience: experience, sort: sort })
    }
    onActiveChanged: if (active) requery()
    onSkillChanged: requery()
    onExperienceChanged: requery()
    onSortChanged: requery()
    Timer {
        id: searchDelay
        interval: 150
        onTriggered: page.requery()
    }
    Connections {
        target: Backend
        function onDataChanged() { if (page.active) page.requery() }
    }

    ConfirmDialog {
        id: confirmReload
        onConfirmed: if (Backend.reload()) page.notify(qsTr("Reloaded from disk"))
    }

    // Column widths, shared by the header and the rows.
    readonly property int numberWidth: 56
    readonly property int idWidth: 92
    readonly property int nameWidth: width >= 1500 ? 270 : 240
    readonly property int programWidth: wide ? (width >= 1500 ? 260 : 215) : 0
    readonly property int experienceWidth: 150

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: 14

        // Filters
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TfTextField {
                placeholderText: qsTr("Search name, id or skill")
                Layout.preferredWidth: page.wide ? 280 : 220
                onTextChanged: { page.query = text; searchDelay.restart() }
            }
            SkillField {
                placeholderText: qsTr("Skill filter")
                Layout.preferredWidth: page.wide ? 200 : 160
                onTextChanged: page.skill = text.trim()
            }
            TfComboBox {
                Layout.preferredWidth: 170
                model: [{ text: qsTr("All levels"), value: "" }, { text: qsTr("Beginner"), value: "Beginner" },
                        { text: qsTr("Developing"), value: "Developing" }, { text: qsTr("Intermediate"), value: "Intermediate" },
                        { text: qsTr("Advanced"), value: "Advanced" }]
                textRole: "text"
                valueRole: "value"
                Accessible.name: qsTr("Experience filter")
                onActivated: page.experience = currentValue
            }
            TfComboBox {
                Layout.preferredWidth: 170
                model: [{ text: qsTr("Sort: name"), value: "name" }, { text: qsTr("Sort: id"), value: "id" },
                        { text: qsTr("Sort: experience"), value: "experience" }, { text: qsTr("Sort: most skills"), value: "skills" }]
                textRole: "text"
                valueRole: "value"
                Accessible.name: qsTr("Sort participants")
                onActivated: page.sort = currentValue
            }
            Item { Layout.fillWidth: true }
            TfButton {
                text: qsTr("Reload")
                compact: true
                onClicked: Backend.dirty
                           ? confirmReload.ask(qsTr("Reload from disk?"), qsTr("Unsaved changes in memory are discarded."), qsTr("Reload"))
                           : (Backend.reload() && page.notify(qsTr("Reloaded from disk")))
            }
        }

        // Record count
        RowLayout {
            spacing: 10
            TfText {
                text: String(page.result.count ?? 0)
                font.pixelSize: Theme.fontXl
                font.weight: Font.ExtraBold
            }
            TfText {
                text: (page.result.count ?? 0) === (page.result.total ?? 0)
                      ? qsTr("participant records")
                      : qsTr("of %1 participant records match").arg(page.result.total ?? 0)
                variant: "secondary"
                font.pixelSize: Theme.fontMd
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignBaseline
            }
            Item { Layout.fillWidth: true }
            TfText {
                text: qsTr("Click a record to inspect, edit and save it")
                variant: "muted"
            }
        }

        Panel {
            padded: false
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pad
                    Layout.rightMargin: Theme.pad
                    Layout.topMargin: 14
                    Layout.bottomMargin: 10
                    spacing: 12
                    TfText { text: qsTr("NO."); variant: "label"; Layout.preferredWidth: page.numberWidth }
                    TfText { text: qsTr("ID"); variant: "label"; Layout.preferredWidth: page.idWidth }
                    TfText { text: qsTr("NAME"); variant: "label"; Layout.preferredWidth: page.nameWidth }
                    TfText { text: qsTr("ACADEMIC DETAILS"); variant: "label"; visible: page.wide; Layout.preferredWidth: page.programWidth }
                    TfText { text: qsTr("TOP SKILLS"); variant: "label"; Layout.fillWidth: true }
                    TfText { text: qsTr("EXPERIENCE"); variant: "label"; Layout.preferredWidth: page.experienceWidth }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.result.rows ?? []
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 56
                        color: rowHover.hovered ? Theme.surfaceRaised : index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.018)
                        Accessible.role: Accessible.ListItem
                        Accessible.name: modelData.name
                        Accessible.onPressAction: page.inspect("participant", row.modelData.id)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.pad
                            anchors.rightMargin: Theme.pad + 10
                            spacing: 12
                            TfText {
                                text: "#" + String(row.modelData.number).padStart(3, "0")
                                font.weight: Font.ExtraBold
                                color: Theme.textSecondary
                                Layout.preferredWidth: page.numberWidth
                            }
                            TfText {
                                text: row.modelData.id
                                variant: "muted"
                                font.family: Theme.monoFamily
                                Layout.preferredWidth: page.idWidth
                            }
                            RowLayout {
                                spacing: 10
                                Layout.fillWidth: false // the name inside fills; this column must not
                                Layout.preferredWidth: page.nameWidth
                                Avatar { name: row.modelData.name; seed: row.modelData.id; size: 32 }
                                TfText { text: row.modelData.name; font.weight: Font.ExtraBold; Layout.fillWidth: true }
                            }
                            TfText {
                                visible: page.wide
                                text: row.modelData.program || qsTr("—")
                                variant: "secondary"
                                Layout.preferredWidth: page.programWidth
                            }
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                // One line of chips; a chip that doesn't fit wraps out of view whole.
                                Flow {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    height: 28
                                    clip: true
                                    spacing: 6
                                    Repeater {
                                        model: row.modelData.topSkills
                                        delegate: Chip {
                                            required property string modelData
                                            skill: true
                                            text: modelData
                                        }
                                    }

                                }
                            }
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: false
                                Layout.preferredWidth: page.experienceWidth
                                TfText { text: row.modelData.experience; font.weight: Font.Bold }
                                TfText { text: row.modelData.averageLevel.toFixed(1); variant: "muted" }
                            }
                        }
                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: page.inspect("participant", row.modelData.id) }
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        width: 340
                        visible: list.count === 0
                        title: qsTr("No matching records")
                        message: qsTr("Clear a filter to see more participants.")
                    }
                }
            }
        }
    }
}
