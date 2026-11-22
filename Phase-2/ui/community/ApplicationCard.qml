pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// One application, for its reviewer (`reviewer: true`: the applicant's profile and the decision
// buttons the rules allow) or for the applicant (the team or event, the status, the response, and
// Withdraw while allowed).
Surface {
    id: card

    property var application: ({})
    property bool reviewer: true
    property bool deadlinePassed: false
    signal decide(var application, string choice)
    signal withdraw(var application)
    signal openProfile(int personId)
    signal openTarget(string link)

    readonly property var person: application.applicant ?? ({})
    readonly property var actions: reviewer ? Hub.reviewerActions(application.kind ?? "team", application.status ?? "") : []
    property bool expanded: false

    implicitHeight: column.implicitHeight + 32
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            HubAvatar {
                visible: card.reviewer
                person: card.person
                size: 44
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: card.openProfile(card.person.id) }
            }
            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    TfText {
                        text: card.reviewer ? (card.person.name ?? "") : (card.application.targetName ?? "")
                        variant: "heading"
                        Layout.fillWidth: true
                    }
                    Chip { text: card.application.kind === "event" ? qsTr("Event") : qsTr("Team"); tone: card.application.kind === "event" ? "info" : "neutral" }
                    HubStatus { status: card.application.status ?? "" }
                }
                TfText {
                    text: (card.reviewer ? qsTr("for %1").arg(card.application.targetName) + (card.person.headline ? " · " + card.person.headline : "")
                                         : (card.application.position ? qsTr("Position: %1").arg(card.application.position) + " · " : ""))
                          + (card.reviewer ? "" : qsTr("Applied %1").arg(Hub.relativeTime(card.application.createdAt ?? 0)))
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    Layout.fillWidth: true
                }
                TfText {
                    visible: card.reviewer
                    text: (card.application.position ? qsTr("Position: %1").arg(card.application.position) + " · " : "")
                          + qsTr("Applied %1").arg(Hub.relativeTime(card.application.createdAt ?? 0))
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    Layout.fillWidth: true
                }
            }
        }

        // what the applicant wrote (reviewer view)
        Repeater {
            model: card.reviewer ? [
                { label: qsTr("Introduction"), text: card.application.introduction ?? "" },
                { label: qsTr("Why they want to join"), text: card.application.reason ?? "" },
                { label: qsTr("Why they want to take part"), text: card.application.motivation ?? "" },
                { label: qsTr("Experience"), text: card.application.experience ?? "" },
                { label: qsTr("Interests"), text: card.application.interests ?? "" },
                { label: qsTr("Availability"), text: card.application.availability ?? "" }
            ].filter(f => f.text !== "") : []
            delegate: ColumnLayout {
                id: field
                required property var modelData
                required property int index
                visible: card.expanded || index < 2
                spacing: 1
                Layout.fillWidth: true
                TfText { text: field.modelData.label; variant: "label" }
                TfText {
                    text: field.modelData.text
                    variant: "secondary"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: card.expanded ? 1000 : 3
                    Layout.fillWidth: true
                }
            }
        }
        Flow {
            visible: card.reviewer && (card.application.skills ?? []).length > 0
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: card.application.skills ?? []
                delegate: Chip { required property string modelData; text: modelData; skill: true }
            }
        }

        // the decision message
        Rectangle {
            visible: (card.application.response ?? "") !== ""
            Layout.fillWidth: true
            implicitHeight: responseText.implicitHeight + 20
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            ColumnLayout {
                id: responseText
                anchors.fill: parent
                anchors.margins: 10
                spacing: 2
                TfText { text: card.reviewer ? qsTr("Your message") : qsTr("Message from the reviewer"); variant: "label" }
                TfText {
                    text: card.application.response ?? ""
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
        }

        // Actions: one line on wide cards, wrapping onto a second line on phones.
        GridLayout {
            Layout.fillWidth: true
            columns: card.width < 520 ? 3 : 8
            columnSpacing: 8
            rowSpacing: 8
            TfButton {
                visible: card.reviewer
                text: card.expanded ? qsTr("Show less") : qsTr("Read all")
                compact: true
                variant: "ghost"
                onClicked: card.expanded = !card.expanded
            }
            TfButton {
                visible: card.reviewer
                text: qsTr("Profile")
                compact: true
                variant: "ghost"
                onClicked: card.openProfile(card.person.id)
            }
            TfButton {
                visible: !card.reviewer
                text: card.application.kind === "event" ? qsTr("Open event") : qsTr("Open team")
                compact: true
                variant: "ghost"
                onClicked: card.openTarget(card.application.link)
            }
            Item { Layout.fillWidth: true; visible: card.width >= 520 }
            Repeater {
                model: card.actions
                delegate: TfButton {
                    required property var modelData
                    text: modelData.text
                    compact: true
                    variant: modelData.value === "approve" ? "primary" : modelData.value === "reject" ? "danger" : "secondary"
                    onClicked: card.decide(card.application, modelData.value)
                }
            }
            TfButton {
                visible: !card.reviewer && Hub.canWithdraw(card.application.kind ?? "team", card.application.status ?? "", card.deadlinePassed)
                text: qsTr("Withdraw")
                compact: true
                variant: "danger"
                onClicked: card.withdraw(card.application)
            }
        }
    }
}
