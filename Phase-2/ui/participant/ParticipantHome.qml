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
    // Layout for the width: phones get one column and tighter margins.
    readonly property bool narrow: width < 720
    readonly property int margin: narrow ? 14 : 24

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            // Who am I
            Surface {
                Layout.fillWidth: true
                Layout.leftMargin: page.margin
                Layout.rightMargin: page.margin
                implicitHeight: hero.implicitHeight + (page.narrow ? 36 : 52)
                radius: Theme.radiusLarge
                tint: Theme.info
                clip: true
                // layered light: the workspace's blue from the left, a touch of coral top right
                AmbientGlow { tone: Theme.info; strength: 0.22; width: parent.width * 0.9; height: width; x: -width * 0.45; y: -height * 0.55 }
                AmbientGlow { tone: Theme.accent; strength: 0.14; width: parent.width * 0.6; height: width; x: parent.width - width * 0.5; y: -height * 0.6 }
                GridLayout {
                    id: hero
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: page.narrow ? 18 : 26
                    columns: page.narrow ? 2 : 3
                    columnSpacing: page.narrow ? 16 : 24
                    rowSpacing: 16
                    // photo with a soft halo (the same identity as everywhere else)
                    Item {
                        implicitWidth: page.narrow ? 68 : 96
                        implicitHeight: implicitWidth
                        Layout.alignment: Qt.AlignTop
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -6
                            radius: width / 2
                            color: "transparent"
                            border.width: 2
                            border.color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.45)
                        }
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -12
                            radius: width / 2
                            color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.08)
                        }
                        Avatar {
                            anchors.fill: parent
                            name: page.me.name ?? ""
                            seed: page.me.id ?? ""
                            size: parent.implicitWidth
                        }
                    }
                    ColumnLayout {
                        spacing: 6
                        Layout.fillWidth: true
                        TfText {
                            text: qsTr("Hi, %1").arg(page.firstName)
                            font.pixelSize: page.narrow ? 26 : 34
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
                        Flow {
                            spacing: 8
                            Layout.topMargin: 6
                            Layout.fillWidth: true
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
                    // profile completion: under the text on phones, at the right on wider screens
                    GridLayout {
                        columns: page.narrow ? 2 : 1
                        columnSpacing: 14
                        rowSpacing: 8
                        Layout.columnSpan: page.narrow ? 2 : 1
                        Layout.alignment: page.narrow ? Qt.AlignLeft : Qt.AlignTop
                        CoverageRing {
                            size: page.narrow ? 72 : 100
                            value: page.completion
                            fillColor: page.completion >= 1 ? Theme.success : Theme.info
                            caption: qsTr("profile")
                            Layout.alignment: Qt.AlignHCenter
                        }
                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: page.narrow ? Qt.AlignVCenter : Qt.AlignHCenter
                            TfText {
                                text: page.completion >= 1 ? qsTr("Profile complete") : qsTr("Profile completion")
                                font.weight: Font.Bold
                                Layout.alignment: page.narrow ? Qt.AlignLeft : Qt.AlignHCenter
                            }
                            TfText {
                                readonly property var todo: (page.me.checklist ?? []).filter(c => !c.done)
                                visible: todo.length > 0
                                text: todo.length > 0 ? qsTr("Next: %1").arg(todo[0].label) : ""
                                variant: "muted"
                                font.pixelSize: Theme.fontXs
                                Layout.maximumWidth: 220
                                Layout.alignment: page.narrow ? Qt.AlignLeft : Qt.AlignHCenter
                            }
                        }
                    }
                }
            }

            // What am I good at, what I asked to join, what is left to do: 3, 2 or 1 columns
            GridLayout {
                id: panels
                Layout.fillWidth: true
                Layout.leftMargin: page.margin
                Layout.rightMargin: page.margin
                columns: page.width >= 1200 ? 3 : page.width >= 760 ? 2 : 1
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap

                Panel {
                    title: qsTr("Top skills")
                    subtitle: Theme.count((page.me.rankedSkills ?? []).length, qsTr("skill"), qsTr("skills"))
                              + " · " + Theme.experienceLabel(page.me.averageLevel ?? 0)
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 520
                    Layout.columnSpan: panels.columns === 2 ? 2 : 1
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
                    Layout.fillWidth: true
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
                                        visible: (interestRow.modelData.interestReply ?? "") !== ""
                                        text: qsTr("Host replied")
                                        tone: "info"
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
                    Layout.fillWidth: true
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
                Layout.leftMargin: page.margin
                Layout.rightMargin: page.margin
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
                Layout.leftMargin: page.margin
                Layout.rightMargin: page.margin
                columns: page.width >= 1400 ? 3 : page.width >= 760 ? 2 : 1
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
