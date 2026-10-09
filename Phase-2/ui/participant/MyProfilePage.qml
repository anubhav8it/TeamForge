pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The participant's own profile as hosts see it: one sentence, top skills, experience,
// learning interests and project fit. Editing opens the profile editor.
TfPage {
    id: page

    signal editProfile()
    signal viewOpportunity(string requirementId)
    signal whyThisMatch(string requirementId)

    readonly property var me: Backend.myProfile
    readonly property var skills: me.rankedSkills ?? []

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            // Header
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                implicitHeight: header.implicitHeight + 48
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                RowLayout {
                    id: header
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 22
                    Avatar {
                        name: page.me.name ?? ""
                        seed: page.me.id ?? ""
                        size: 92
                        Layout.alignment: Qt.AlignTop
                    }
                    ColumnLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TfText {
                            text: page.me.name ?? ""
                            font.pixelSize: 32
                            font.weight: Font.ExtraBold
                            font.letterSpacing: -0.8
                            Layout.fillWidth: true
                        }
                        TfText {
                            visible: (page.me.program ?? "") !== ""
                            text: page.me.program ?? ""
                            variant: "muted"
                            font.weight: Font.Bold
                        }
                        TfText {
                            text: page.me.summary || qsTr("No summary yet.")
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.Bold
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            Layout.fillWidth: true
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 6
                            Chip {
                                text: Theme.experienceLabel(page.me.averageLevel ?? 0) + " · " + qsTr("avg level %1").arg((page.me.averageLevel ?? 0).toFixed(1))
                                tone: "success"
                            }
                            Chip {
                                text: qsTr("Profile %1 complete").arg(Theme.percent(page.me.completion ?? 0))
                                tone: (page.me.completion ?? 0) >= 1 ? "success" : "warning"
                            }
                        }
                    }
                    TfButton {
                        text: qsTr("Edit profile")
                        variant: "primary"
                        Layout.alignment: Qt.AlignTop
                        onClicked: page.editProfile()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: Theme.gap

                // Top skills and experience
                Panel {
                    id: skillsPanel
                    property bool showAll: false
                    title: qsTr("Top skills & experience")
                    subtitle: qsTr("Strongest first")
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop

                    actions: TfButton {
                        visible: page.skills.length > 5
                        text: skillsPanel.showAll ? qsTr("Show less") : qsTr("Show all %1").arg(page.skills.length)
                        variant: "ghost"
                        compact: true
                        onClicked: skillsPanel.showAll = !skillsPanel.showAll
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 0
                        Repeater {
                            model: skillsPanel.showAll ? page.skills : page.skills.slice(0, 5)
                            delegate: Rectangle {
                                id: skillRow
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 46
                                color: "transparent"
                                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 14
                                    TfText {
                                        text: Theme.skill(skillRow.modelData.skill)
                                        font.weight: Font.ExtraBold
                                        font.letterSpacing: 0.4
                                        Layout.fillWidth: true
                                    }
                                    LevelPips { level: skillRow.modelData.level }
                                    TfText {
                                        text: Theme.levelName(skillRow.modelData.level)
                                        variant: "secondary"
                                        font.weight: Font.Bold
                                        Layout.preferredWidth: 124
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    spacing: Theme.gap
                    Layout.preferredWidth: 400
                    Layout.alignment: Qt.AlignTop
                    Panel {
                        title: qsTr("Experience")
                        subtitle: qsTr("From your skill levels")
                        Layout.fillWidth: true
                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: 16
                            TfText {
                                text: (page.me.averageLevel ?? 0).toFixed(1)
                                font.pixelSize: Theme.fontDisplay
                                font.weight: Font.ExtraBold
                                color: Theme.warning
                            }
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                TfText { text: page.me.experience ?? ""; font.pixelSize: Theme.fontLg; font.weight: Font.ExtraBold }
                                TfText {
                                    text: qsTr("%1 at level 4 or above").arg(Theme.count(page.skills.filter(s => s.level >= 4).length, qsTr("skill"), qsTr("skills")))
                                    variant: "secondary"
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                    Panel {
                        title: qsTr("Learning interests")
                        Layout.fillWidth: true
                        SkillChips {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            skills: page.me.skillsWanted ?? []
                            maxVisible: 6
                            toneFor: () => "info"
                        }
                    }
                }
            }

            // Project fit
            Panel {
                title: qsTr("Project fit")
                subtitle: qsTr("Where your profile fits best right now")
                padded: false
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0
                    Repeater {
                        model: Backend.opportunities.slice(0, 4)
                        delegate: Rectangle {
                            id: fitRow
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: 64
                            color: fitHover.hovered ? Theme.surfaceRaised : "transparent"
                            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.pad
                                anchors.rightMargin: Theme.pad
                                spacing: 14
                                Rectangle {
                                    implicitWidth: 10
                                    implicitHeight: 10
                                    radius: 5
                                    color: Theme.typeColor(fitRow.modelData.type ?? "")
                                }
                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillWidth: true
                                    TfText {
                                        text: fitRow.modelData.name
                                        font.weight: Font.ExtraBold
                                        font.pixelSize: Theme.fontMd
                                        Layout.fillWidth: true
                                    }
                                    TfText {
                                        text: Theme.fitSummary(fitRow.modelData)
                                        variant: "muted"
                                        font.pixelSize: Theme.fontXs
                                        Layout.fillWidth: true
                                    }
                                }
                                Chip {
                                    visible: (fitRow.modelData.interestStatus ?? "") !== ""
                                    text: Theme.interestLabel(fitRow.modelData.interestStatus ?? "")
                                    tone: Theme.interestTone(fitRow.modelData.interestStatus ?? "")
                                }
                                TfText {
                                    text: Theme.percent(fitRow.modelData.score)
                                    variant: "score"
                                    color: Theme.scoreColor(fitRow.modelData.score)
                                }
                                TfButton {
                                    text: qsTr("Why this match?")
                                    compact: true
                                    onClicked: page.whyThisMatch(fitRow.modelData.id)
                                }
                                TfButton {
                                    text: qsTr("View")
                                    variant: "ghost"
                                    compact: true
                                    onClicked: page.viewOpportunity(fitRow.modelData.id)
                                }
                            }
                            HoverHandler { id: fitHover }
                        }
                    }
                    Item { Layout.preferredHeight: 6 }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
