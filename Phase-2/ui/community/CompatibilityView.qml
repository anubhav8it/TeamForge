pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// A compatibility report (Backend.teamCompatibility, core/Compatibility.h): the match as a ring
// and a band, the suggested role, skills that match (with how each is known), skills below the
// level the team asks for, skills not shown in the profile, strengths, open questions, and how
// the score was made. Advice for the team leader, who decides.
ColumnLayout {
    id: view

    property var report: ({})
    property bool forCandidate: false   // the person reads their own report (invitation, link)
    property bool showDetails: false

    readonly property bool known: report.enoughInformation === true
    readonly property color bandColor: !known ? Theme.textMuted : Theme.scoreColor(report.score ?? 0)

    function evidenceText(fit) {
        const how = fit.evidence === "projects" ? qsTr("Backed by projects or resume")
                  : fit.evidence === "unrated" ? qsTr("Level not stated") : qsTr("Self-assessed")
        return how + (fit.fillsGap ? " · " + qsTr("fills a gap") : "")
    }

    spacing: 12

    // ---- score and role
    RowLayout {
        Layout.fillWidth: true
        spacing: 14
        Item {
            implicitWidth: 76
            implicitHeight: 76
            CoverageRing {
                anchors.fill: parent
                visible: view.known
                size: 76
                lineWidth: 7
                value: view.report.score ?? 0
                fillColor: view.bandColor
                caption: qsTr("match")
                Accessible.role: Accessible.Indicator
                Accessible.name: qsTr("Compatibility %1 percent").arg(view.report.percent ?? 0)
            }
            Rectangle {
                anchors.fill: parent
                visible: !view.known
                radius: width / 2
                color: "transparent"
                border.color: Theme.border
                border.width: 7
                TfText { anchors.centerIn: parent; text: "?"; font.pixelSize: 24; font.weight: Font.ExtraBold; color: Theme.textMuted }
            }
        }
        ColumnLayout {
            spacing: 3
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            TfText {
                text: view.report.bandLabel ?? ""
                font.pixelSize: Theme.fontLg
                font.weight: Font.ExtraBold
                color: view.bandColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TfText {
                visible: (view.report.suggestedRole ?? "") !== ""
                text: (view.forCandidate ? qsTr("Your possible role: %1") : qsTr("Suggested role: %1")).arg(view.report.suggestedRole ?? "")
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TfText {
                visible: (view.report.roleReason ?? "") !== ""
                text: view.report.roleReason ?? ""
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
            }
        }
    }

    TfText {
        visible: view.report.teamHasSkills === false
        text: (view.report.explanation ?? [])[0] ?? ""
        variant: "muted"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        Layout.fillWidth: true
    }

    // ---- matching skills
    TfText { visible: (view.report.matching ?? []).length > 0; text: qsTr("Matching skills"); variant: "label" }
    Repeater {
        model: view.report.matching ?? []
        delegate: RowLayout {
            id: matchRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                TfText { text: Theme.skill(matchRow.modelData.skill); font.weight: Font.Bold; Layout.fillWidth: true }
                TfText {
                    text: view.evidenceText(matchRow.modelData)
                    font.pixelSize: Theme.fontXs
                    color: matchRow.modelData.evidence === "projects" ? Theme.success : Theme.textMuted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            LevelPips {
                level: matchRow.modelData.level
                minLevel: matchRow.modelData.required
                Accessible.name: qsTr("Level %1, team asks for %2").arg(matchRow.modelData.level).arg(matchRow.modelData.required)
            }
        }
    }

    // ---- below the requested level
    TfText { visible: (view.report.belowLevel ?? []).length > 0; text: qsTr("Below the level the team asks for"); variant: "label" }
    Repeater {
        model: view.report.belowLevel ?? []
        delegate: RowLayout {
            id: belowRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                TfText { text: Theme.skill(belowRow.modelData.skill); font.weight: Font.Bold; Layout.fillWidth: true }
                TfText {
                    text: qsTr("Level %1 of the %2 asked for").arg(belowRow.modelData.level).arg(belowRow.modelData.required)
                    font.pixelSize: Theme.fontXs
                    color: Theme.warning
                    Layout.fillWidth: true
                }
            }
            LevelPips { level: belowRow.modelData.level; minLevel: belowRow.modelData.required }
        }
    }

    // ---- not shown in the profile
    TfText {
        visible: (view.report.notShown ?? []).length > 0
        text: view.forCandidate ? qsTr("Not in your profile yet") : qsTr("Not shown in their profile")
        variant: "label"
    }
    Flow {
        visible: (view.report.notShown ?? []).length > 0
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: view.report.notShown ?? []
            delegate: Chip { required property string modelData; text: Theme.skill(modelData); tone: "muted" }
        }
    }

    // ---- strengths and open questions
    TfText { visible: (view.report.strengths ?? []).length > 0; text: view.forCandidate ? qsTr("What you bring") : qsTr("Strengths"); variant: "label" }
    Repeater {
        model: view.report.strengths ?? []
        delegate: TfText {
            required property string modelData
            text: "✓  " + modelData
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
    }
    TfText {
        visible: (view.report.improvements ?? []).length > 0
        text: view.forCandidate ? qsTr("Ways to strengthen your fit") : qsTr("Gaps and questions to ask")
        variant: "label"
    }
    Repeater {
        model: view.report.improvements ?? []
        delegate: TfText {
            required property string modelData
            text: "•  " + modelData
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
    }

    // ---- how the score was made
    TfButton {
        visible: view.report.teamHasSkills !== false
        text: view.showDetails ? qsTr("Hide how this score was made") : qsTr("How this score was made")
        variant: "ghost"
        compact: true
        onClicked: view.showDetails = !view.showDetails
    }
    Repeater {
        model: view.showDetails && view.known ? [
            { label: qsTr("Skill coverage"), value: view.report.factors.coverage },
            { label: qsTr("Fills gaps"), value: view.report.factors.complementarity },
            { label: qsTr("Experience"), value: view.report.factors.experience },
            { label: qsTr("Interest"), value: view.report.factors.engagement },
            { label: qsTr("Role variety"), value: view.report.factors.diversity }
        ] : []
        delegate: RowLayout {
            id: factorRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 10
            TfText { text: factorRow.modelData.label; Layout.preferredWidth: 110; font.pixelSize: Theme.fontXs; font.weight: Font.Bold }
            MeterBar { value: factorRow.modelData.value; fillColor: Theme.scoreColor(factorRow.modelData.value); Layout.fillWidth: true }
            TfText { text: Theme.percent(factorRow.modelData.value); font.pixelSize: Theme.fontXs; Layout.preferredWidth: 38 }
        }
    }
    Repeater {
        model: view.showDetails ? (view.report.explanation ?? []) : []
        delegate: TfText {
            required property string modelData
            text: modelData
            variant: "muted"
            font.pixelSize: Theme.fontXs
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
    }
    TfText {
        visible: !view.forCandidate
        text: qsTr("Advice only, from the profile data. You decide who joins.")
        variant: "muted"
        font.pixelSize: Theme.fontXs
        font.italic: true
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
