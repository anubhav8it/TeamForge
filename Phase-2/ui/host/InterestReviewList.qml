pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// The interest requests for one project (Backend.projectInterests), by review stage, with
// Accept and Decline. "Why this match?" also marks a new request as under review. Accepting
// offers the person to team building in Matching; it does not add them to the team.
ColumnLayout {
    id: list

    property string requirementId
    property string stage: "interested"
    property var requests: []
    signal whyThisMatch(string studentId, string requirementId)
    signal notify(string message)

    readonly property var stages: [
        { value: "interested", text: qsTr("Interested") },
        { value: "under_review", text: qsTr("Under review") },
        { value: "accepted", text: qsTr("Accepted") },
        { value: "declined", text: qsTr("Declined") }
    ]
    readonly property var shown: requests.filter(r => r.status === stage)

    function countOf(status) {
        return requests.filter(r => r.status === status).length
    }
    function reload() {
        requests = requirementId !== "" ? Backend.projectInterests(requirementId) : []
    }
    function act(ok, message) {
        if (ok)
            notify(message)
    }

    onRequirementIdChanged: reload()
    Component.onCompleted: reload()
    Connections {
        target: Backend
        function onInterestsChanged() { list.reload() }
        function onDataChanged() { list.reload() }
    }

    spacing: 12

    Segmented {
        options: list.stages.map(s => ({ text: s.text + "  " + list.countOf(s.value), value: s.value }))
        currentValue: list.stage
        onActivated: value => list.stage = value
    }

    ListView {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: list.shown
        spacing: 2
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal } }
        displaced: Transition { NumberAnimation { property: "y"; duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        delegate: Rectangle {
            id: row
            required property var modelData
            width: ListView.view.width
            height: 72
            radius: Theme.radiusSmall
            color: rowHover.hovered ? Theme.surfaceRaised : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 14
                Avatar {
                    name: row.modelData.name
                    seed: row.modelData.studentId
                    size: 42
                    highlighted: row.modelData.status === "accepted"
                }
                ColumnLayout {
                    spacing: 4
                    Layout.preferredWidth: 260
                    Layout.fillWidth: false
                    TfText { text: row.modelData.name; font.pixelSize: Theme.fontMd; font.weight: Font.ExtraBold; Layout.fillWidth: true }
                    TfText {
                        text: row.modelData.experience + (row.modelData.program ? "  ·  " + row.modelData.program : "")
                        variant: "muted"
                        Layout.fillWidth: true
                    }
                }
                // Top skills: one line; a chip that doesn't fit drops out whole.
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
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
                                tone: (row.modelData.coveredSkills ?? []).indexOf(modelData) >= 0 ? "success" : "neutral"
                            }
                        }
                    }
                }
                TfText {
                    text: Theme.percent(row.modelData.score)
                    font.pixelSize: Theme.fontLg + 2
                    font.weight: Font.ExtraBold
                    color: Theme.scoreColor(row.modelData.score)
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: 58
                }
                TfButton {
                    text: qsTr("Why this match?")
                    variant: "ghost"
                    compact: true
                    onClicked: {
                        Backend.reviewInterest(row.modelData.id)
                        list.whyThisMatch(row.modelData.studentId, list.requirementId)
                    }
                }
                TfButton {
                    visible: row.modelData.status !== "accepted"
                    text: qsTr("Accept")
                    variant: "primary"
                    compact: true
                    Accessible.name: qsTr("Accept %1").arg(row.modelData.name)
                    onClicked: list.act(Backend.acceptInterest(row.modelData.id),
                                        qsTr("%1 accepted. Add them to the team in Matching.").arg(row.modelData.name))
                }
                TfButton {
                    visible: row.modelData.status !== "declined"
                    text: qsTr("Decline")
                    variant: "danger"
                    compact: true
                    Accessible.name: qsTr("Decline %1").arg(row.modelData.name)
                    onClicked: list.act(Backend.declineInterest(row.modelData.id), qsTr("%1 declined").arg(row.modelData.name))
                }
            }
            HoverHandler { id: rowHover }
        }

        EmptyState {
            anchors.centerIn: parent
            width: 360
            visible: view.count === 0
            title: list.stage === "interested" ? qsTr("No new interest")
                 : list.stage === "under_review" ? qsTr("Nothing under review")
                 : list.stage === "accepted" ? qsTr("Nobody accepted yet") : qsTr("Nobody declined")
            message: list.stage === "interested" ? qsTr("Participants who express interest in this project appear here.") : ""
        }
    }
}
