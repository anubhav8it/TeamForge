pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Someone's Community profile (an applicant, a team member): picture, headline, about, skills,
// experience, interests and teams. Email addresses are never part of a profile.
Popup {
    id: dialog

    property var person: ({})
    property bool loading: false
    signal messageAnonymously(var person)

    function show(personId) {
        person = ({})
        loading = true
        open()
        Hub.get("/people/" + personId, (status, data) => {
            loading = false
            if (status === 200)
                person = data.person
        })
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 48)
    height: Math.min(implicitHeight, parent.height - 48)
    modal: true
    dim: true
    padding: 24
    Overlay.modal: Rectangle { color: "#B3050810" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }

    background: Surface { radius: Theme.radiusLarge; baseColor: Theme.surfaceRaised; border.color: Theme.borderStrong }

    contentItem: Flickable {
        implicitHeight: body.implicitHeight
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: body
            width: parent.width
            spacing: 12

            HubSkeleton { visible: dialog.loading; rows: 2; Layout.fillWidth: true }

            RowLayout {
                visible: !dialog.loading
                Layout.fillWidth: true
                spacing: 14
                HubAvatar { person: dialog.person; size: 72 }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    TfText { text: dialog.person.name ?? ""; variant: "section"; Layout.fillWidth: true }
                    TfText {
                        text: dialog.person.headline ?? ""
                        variant: "secondary"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                    Chip { text: dialog.person.role ?? ""; tone: "neutral"; visible: (dialog.person.role ?? "") !== "" }
                }
            }
            Repeater {
                model: [
                    { label: qsTr("About"), text: dialog.person.bio ?? "" },
                    { label: qsTr("Experience"), text: dialog.person.experience ?? "" },
                    { label: qsTr("Interests"), text: dialog.person.interests ?? "" }
                ]
                delegate: ColumnLayout {
                    id: section
                    required property var modelData
                    visible: !dialog.loading && modelData.text !== ""
                    spacing: 2
                    Layout.fillWidth: true
                    TfText { text: section.modelData.label; variant: "label" }
                    TfText {
                        text: section.modelData.text
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                }
            }
            TfText { visible: !dialog.loading && (dialog.person.skills ?? []).length > 0; text: qsTr("Skills"); variant: "label" }
            Flow {
                visible: !dialog.loading
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: dialog.person.skills ?? []
                    delegate: Chip { required property string modelData; text: modelData; skill: true }
                }
            }
            TfText { visible: !dialog.loading && (dialog.person.teams ?? []).length > 0; text: qsTr("Teams"); variant: "label" }
            Flow {
                visible: !dialog.loading
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: dialog.person.teams ?? []
                    delegate: Chip {
                        required property var modelData
                        text: modelData.name + " · " + Hub.statusLabel(modelData.role)
                        tone: "neutral"
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                Item { Layout.fillWidth: true }
                TfButton {
                    visible: !!dialog.person.acceptsAnonymous && !dialog.person.isMe
                    text: qsTr("Message anonymously")
                    compact: true
                    onClicked: { dialog.close(); dialog.messageAnonymously(dialog.person) }
                }
                TfButton { text: qsTr("Close"); compact: true; variant: "ghost"; onClicked: dialog.close() }
            }
        }
    }
}
