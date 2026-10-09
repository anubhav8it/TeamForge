pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Host: who wants to join the active project. Review each request and accept or decline it;
// accepted participants are offered in Matching, where the host decides the team.
TfPage {
    id: page

    signal whyThisMatch(string studentId, string requirementId)
    signal openMatching()
    signal notify(string message)

    readonly property var project: Backend.currentRequirement
    readonly property var interest: project.interest ?? ({})

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 20
        spacing: Theme.gap

        Panel {
            Layout.fillWidth: true
            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 24
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    TfText { text: page.project.name ?? ""; font.pixelSize: Theme.fontXl; font.weight: Font.ExtraBold }
                    TfText {
                        text: qsTr("Accepted participants are offered in Matching. You still choose the team.")
                        variant: "secondary"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                Repeater {
                    model: [[qsTr("New"), page.interest.interested ?? 0, Theme.info],
                            [qsTr("In review"), page.interest.under_review ?? 0, Theme.warning],
                            [qsTr("Accepted"), page.interest.accepted ?? 0, Theme.success]]
                    delegate: ColumnLayout {
                        id: figure
                        required property var modelData
                        spacing: 0
                        TfText {
                            text: figure.modelData[1]
                            font.pixelSize: Theme.fontXl
                            font.weight: Font.ExtraBold
                            color: figure.modelData[2]
                            Layout.alignment: Qt.AlignHCenter
                        }
                        TfText { text: figure.modelData[0]; variant: "muted"; font.weight: Font.Bold; Layout.alignment: Qt.AlignHCenter }
                    }
                }
                TfButton {
                    text: qsTr("Build the team")
                    variant: "primary"
                    enabled: (page.interest.accepted ?? 0) > 0
                    onClicked: page.openMatching()
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            InterestReviewList {
                anchors.fill: parent
                requirementId: page.project.id ?? ""
                onWhyThisMatch: (studentId, requirementId) => page.whyThisMatch(studentId, requirementId)
                onNotify: message => page.notify(message)
            }
        }
    }
}
