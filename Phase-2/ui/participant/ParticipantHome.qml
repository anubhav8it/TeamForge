pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The participant's personal home: who am I, what am I good at, where do I fit.
TfPage {
    id: page

    signal editProfile()
    signal openProfile()
    signal openOpportunities()
    signal viewOpportunity(string requirementId)
    signal whyThisMatch(string requirementId)
    signal expressInterest(string requirementId)

    readonly property var me: Backend.myProfile
    readonly property var best: Backend.opportunities.slice(0, 3)
    readonly property real completion: me.completion ?? 0
    readonly property string firstName: (me.name ?? "").split(" ")[0]

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            // Who am I
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                implicitHeight: hero.implicitHeight + 48
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius
                    opacity: 0.10
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.info }
                        GradientStop { position: 0.75; color: "transparent" }
                    }
                }
                RowLayout {
                    id: hero
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 22
                    Avatar {
                        name: page.me.name ?? ""
                        seed: page.me.id ?? ""
                        size: 84
                        Layout.alignment: Qt.AlignTop
                    }
                    ColumnLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TfText {
                            text: qsTr("Hi, %1").arg(page.firstName)
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
                            text: page.me.summary || qsTr("Add a one-line summary so hosts know what you do.")
                            variant: "secondary"
                            font.pixelSize: Theme.fontMd
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            spacing: 8
                            Layout.topMargin: 6
                            TfButton {
                                text: qsTr("Edit profile")
                                compact: true
                                onClicked: page.editProfile()
                            }
                            TfButton {
                                text: qsTr("View my profile")
                                variant: "ghost"
                                compact: true
                                onClicked: page.openProfile()
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 8
                        Layout.alignment: Qt.AlignTop
                        CoverageRing {
                            size: 96
                            value: page.completion
                            fillColor: page.completion >= 1 ? Theme.success : Theme.info
                            caption: qsTr("profile")
                            Layout.alignment: Qt.AlignHCenter
                        }
                        TfText {
                            text: page.completion >= 1 ? qsTr("Profile complete") : qsTr("Profile completion")
                            variant: "muted"
                            font.weight: Font.Bold
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }

            // What am I good at + when am I free
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: Theme.gap

                Panel {
                    title: qsTr("Top skills")
                    subtitle: Theme.count((page.me.rankedSkills ?? []).length, qsTr("skill"), qsTr("skills"))
                              + " · " + Theme.experienceLabel(page.me.averageLevel ?? 0)
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 10
                        SkillChips {
                            Layout.fillWidth: true
                            skills: page.me.rankedSkills ?? []
                            maxVisible: 6
                            large: true
                            toneFor: entry => entry.level >= 4 ? "success" : "neutral"
                        }
                        TfText {
                            visible: (page.me.skillsWanted ?? []).length > 0
                            text: qsTr("Learning next")
                            variant: "label"
                            Layout.topMargin: 8
                        }
                        SkillChips {
                            visible: (page.me.skillsWanted ?? []).length > 0
                            Layout.fillWidth: true
                            skills: page.me.skillsWanted ?? []
                            maxVisible: 4
                            toneFor: () => "info"
                        }
                    }
                }
                // Your interests: where each request stands with the host.
                Panel {
                    title: qsTr("Your interests")
                    subtitle: Backend.myInterests.length > 0
                              ? Theme.count(Backend.myInterests.length, qsTr("project"), qsTr("projects"))
                              : qsTr("Express interest in a project to join its team")
                    Layout.preferredWidth: 420
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 2
                        Repeater {
                            model: Backend.myInterests.slice(0, 4)
                            delegate: Rectangle {
                                id: interestRow
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 48
                                radius: Theme.radiusSmall
                                color: interestHover.hovered ? Theme.surfaceRaised : "transparent"
                                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                Accessible.role: Accessible.Button
                                Accessible.name: interestRow.modelData.name
                                Accessible.onPressAction: page.viewOpportunity(interestRow.modelData.id)
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 10
                                    Rectangle {
                                        implicitWidth: 10
                                        implicitHeight: 10
                                        radius: 5
                                        color: Theme.typeColor(interestRow.modelData.type ?? "")
                                    }
                                    TfText {
                                        text: interestRow.modelData.name
                                        font.weight: Font.ExtraBold
                                        Layout.fillWidth: true
                                    }
                                    Chip {
                                        text: Theme.interestLabel(interestRow.modelData.interestStatus)
                                        tone: Theme.interestTone(interestRow.modelData.interestStatus)
                                    }
                                }
                                HoverHandler { id: interestHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: page.viewOpportunity(interestRow.modelData.id) }
                            }
                        }
                        TfButton {
                            visible: Backend.myInterests.length === 0 || Backend.myInterests.length > 4
                            text: Backend.myInterests.length === 0 ? qsTr("Browse opportunities") : qsTr("See all")
                            variant: Backend.myInterests.length === 0 ? "primary" : "ghost"
                            compact: true
                            Layout.topMargin: 6
                            onClicked: page.openOpportunities()
                        }
                    }
                }
                Panel {
                    visible: page.completion < 1
                    title: qsTr("Finish your profile")
                    subtitle: qsTr("Complete profiles rank better")
                    Layout.preferredWidth: 300
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 8
                        Repeater {
                            model: page.me.checklist ?? []
                            delegate: RowLayout {
                                id: check
                                required property var modelData
                                spacing: 10
                                Rectangle {
                                    implicitWidth: 20
                                    implicitHeight: 20
                                    radius: 10
                                    color: check.modelData.done ? Theme.success : "transparent"
                                    border.width: check.modelData.done ? 0 : 1.5
                                    border.color: Theme.borderStrong
                                    Text {
                                        anchors.centerIn: parent
                                        visible: check.modelData.done
                                        text: "✓"
                                        font.pixelSize: 11
                                        font.weight: Font.Black
                                        color: "#0E2A1E"
                                    }
                                }
                                TfText {
                                    text: check.modelData.label
                                    font.weight: Font.Bold
                                    color: check.modelData.done ? Theme.textSecondary : Theme.text
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            // Where do I fit
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 6
                TfText { text: qsTr("Best opportunities for you"); variant: "section"; Layout.fillWidth: true }
                TfButton {
                    text: qsTr("See all opportunities")
                    variant: "ghost"
                    compact: true
                    onClicked: page.openOpportunities()
                }
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                columns: page.width >= 1400 ? 3 : 2
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap
                Repeater {
                    model: page.best
                    delegate: OpportunityCard {
                        required property var modelData
                        opportunity: modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        onViewOpportunity: id => page.viewOpportunity(id)
                        onWhyThisMatch: id => page.whyThisMatch(id)
                        onExpressInterest: id => page.expressInterest(id)
                    }
                }
            }

            Item { Layout.preferredHeight: 20 }
        }
    }
}
