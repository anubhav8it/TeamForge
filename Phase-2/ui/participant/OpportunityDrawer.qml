pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// "View opportunity": one project in full from the participant's side: what it needs, how
// their skills compare, and where their interest stands. Expressing interest is the one action.
Drawer {
    id: drawer

    signal whyThisMatch(string requirementId)
    signal expressInterest(string requirementId)
    readonly property string status: opportunity.interestStatus ?? ""

    property string requirementId
    readonly property var opportunity: Backend.opportunities.find(o => o.id === requirementId) ?? ({})
    readonly property var me: Backend.myProfile

    function show(id) {
        requirementId = id
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

    contentItem: ScrollView {
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
                            TfButton {
                                visible: drawer.status === ""
                                text: qsTr("Express interest")
                                variant: "primary"
                                compact: true
                                onClicked: drawer.expressInterest(drawer.requirementId)
                            }
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
}
