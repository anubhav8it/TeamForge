pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// The team's skill coverage (Backend.teamSkillCoverage): each required skill as covered (a
// member meets the level), partly covered (someone has it, below the level) or missing, from the
// members' participant profiles.
ColumnLayout {
    id: coverageView

    property var team: null
    readonly property var entries: team ? Backend.teamSkillCoverage(team) : []
    readonly property int coveredCount: entries.filter(e => e.state === "covered").length

    spacing: 8

    TfText {
        visible: coverageView.entries.length === 0
        text: qsTr("The team has not listed the skills it needs yet.")
        variant: "muted"
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    RowLayout {
        visible: coverageView.entries.length > 0
        Layout.fillWidth: true
        spacing: 10
        MeterBar {
            value: coverageView.entries.length > 0 ? coverageView.coveredCount / coverageView.entries.length : 0
            fillColor: Theme.success
            Layout.fillWidth: true
        }
        TfText {
            text: qsTr("%1 of %2 covered").arg(coverageView.coveredCount).arg(coverageView.entries.length)
            font.pixelSize: Theme.fontXs
            font.weight: Font.Bold
        }
    }
    Repeater {
        model: coverageView.entries
        delegate: RowLayout {
            id: row
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: row.modelData.state === "covered" ? Theme.success : row.modelData.state === "partial" ? Theme.warning : Theme.accent
            }
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                TfText { text: Theme.skill(row.modelData.skill); font.weight: Font.Bold; Layout.fillWidth: true }
                TfText {
                    text: row.modelData.state === "covered" ? qsTr("Covered by %1").arg(row.modelData.coveredBy.join(", "))
                        : row.modelData.state === "partial" ? qsTr("Partly: best level %1 of %2").arg(row.modelData.bestLevel).arg(row.modelData.required)
                        : qsTr("Missing") + (row.modelData.required > 1 ? " · " + qsTr("needs %1").arg(Theme.levelName(row.modelData.required).toLowerCase()) : "")
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            LevelPips { level: row.modelData.bestLevel; minLevel: row.modelData.required }
        }
    }
    TfText {
        visible: coverageView.entries.length > 0
        text: qsTr("From members' participant profiles (self-assessed levels).")
        variant: "muted"
        font.pixelSize: Theme.fontXs
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
