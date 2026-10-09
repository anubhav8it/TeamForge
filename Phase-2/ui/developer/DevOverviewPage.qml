pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Developer overview: the local database at a glance, two plain status lines, and quick ways
// into the data and the product. Technical detail lives under Workspace -> System tools.
TfPage {
    id: page

    signal openPage(int index)
    signal notify(string message)

    readonly property var status: Backend.systemStatus
    readonly property var index: status.index ?? ({})
    readonly property var error: Backend.lastErrorDetail
    readonly property bool structuresAgree: (index.indexSkills ?? 0) === (index.treeSkills ?? -1)
                                            && (index.graphSkills ?? 0) === (index.treeSkills ?? -1)
    readonly property bool systemOk: (status.healthy ?? false) && structuresAgree && error.type === undefined

    component Figure: Rectangle {
        id: figure
        property string label
        property int value
        property string detail
        property color tone
        property int target: -1
        implicitHeight: figureColumn.implicitHeight + 52
        radius: Theme.radius
        color: figureHover.hovered && target >= 0 ? Theme.surfaceRaised : Theme.surface
        border.color: figureHover.hovered && target >= 0 ? tone : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        transform: Translate {
            y: figureHover.hovered && figure.target >= 0 ? -2 : 0
            Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        }
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: if (figure.target >= 0) page.openPage(figure.target)
        Rectangle { x: 20; y: 20; width: 28; height: 4; radius: 2; color: figure.tone }
        ColumnLayout {
            id: figureColumn
            anchors.fill: parent
            anchors.margins: 20
            anchors.topMargin: 32
            spacing: 0
            TfText {
                text: figure.value
                font.pixelSize: Theme.fontDisplay + 6
                font.weight: Font.ExtraBold
                font.letterSpacing: -1
            }
            TfText { text: figure.label; font.pixelSize: Theme.fontMd; font.weight: Font.ExtraBold; font.letterSpacing: 1.2 }
            TfText { text: figure.detail; variant: "muted"; Layout.fillWidth: true }
        }
        HoverHandler { id: figureHover; cursorShape: figure.target >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { onTapped: if (figure.target >= 0) page.openPage(figure.target) }
    }

    component Status: Rectangle {
        id: statusCard
        property string label
        property bool ok
        property string headline
        property string detail
        property string actionText
        signal action()
        implicitHeight: statusColumn.implicitHeight + 40
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            id: statusColumn
            anchors.fill: parent
            anchors.margins: 20
            spacing: 6
            RowLayout {
                spacing: 8
                Rectangle { implicitWidth: 10; implicitHeight: 10; radius: 5; color: statusCard.ok ? Theme.success : Theme.warning }
                TfText { text: statusCard.label; variant: "label"; font.letterSpacing: 0.8 }
            }
            TfText { text: statusCard.headline; font.pixelSize: Theme.fontLg; font.weight: Font.ExtraBold; Layout.fillWidth: true }
            TfText { text: statusCard.detail; variant: "secondary"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TfButton {
                visible: statusCard.actionText !== ""
                text: statusCard.actionText
                compact: true
                Layout.topMargin: 4
                onClicked: statusCard.action()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 48
            x: 24
            spacing: Theme.gap

            Item { Layout.preferredHeight: 4 }

            GridLayout {
                Layout.fillWidth: true
                columns: page.width >= 900 ? 4 : 2
                columnSpacing: Theme.gap
                rowSpacing: Theme.gap
                Figure {
                    label: qsTr("PARTICIPANTS"); value: page.status.participants ?? 0; tone: Theme.info; target: 1
                    detail: qsTr("every record, numbered")
                    Layout.fillWidth: true
                }
                Figure {
                    label: qsTr("PROJECTS"); value: page.status.projects ?? 0; tone: Theme.accent; target: 2
                    detail: qsTr("%1 interest requests").arg(page.status.interests ?? 0)
                    Layout.fillWidth: true
                }
                Figure {
                    label: qsTr("SKILLS"); value: page.status.skills ?? 0; tone: Theme.teal; target: 3
                    detail: qsTr("open-ended: any new skill joins")
                    Layout.fillWidth: true
                }
                Figure {
                    label: qsTr("TEAMS"); value: page.status.savedTeams ?? 0; tone: Theme.success; target: 2
                    detail: qsTr("saved by hosts")
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gap
                Status {
                    label: qsTr("DATA STATUS")
                    ok: !page.status.dirty
                    headline: page.status.dirty ? qsTr("Unsaved changes") : qsTr("Everything is saved")
                    detail: qsTr("Loaded at %1").arg(page.status.lastLoaded ?? "")
                            + (page.status.lastSaved ? qsTr(", last saved at %1").arg(page.status.lastSaved) : "")
                    actionText: page.status.dirty ? qsTr("Save now") : ""
                    onAction: if (Backend.save()) page.notify(qsTr("Saved to disk"))
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                }
                Status {
                    label: qsTr("SYSTEM STATUS")
                    ok: page.systemOk
                    headline: page.systemOk ? qsTr("Running normally") : qsTr("Needs a look")
                    detail: page.error.type !== undefined ? qsTr("Latest problem: %1").arg(page.error.message ?? "")
                                                           : qsTr("No problems this session")
                    actionText: qsTr("System tools")
                    onAction: page.openPage(4)
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                }
            }

            Panel {
                title: qsTr("Go anywhere")
                subtitle: qsTr("Open the data, or see the product as a participant or the host")
                Layout.fillWidth: true
                Flow {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 10
                    TfButton { text: qsTr("Participant database"); onClicked: page.openPage(1) }
                    TfButton { text: qsTr("Projects and teams"); onClicked: page.openPage(2) }
                    TfButton { text: qsTr("Skill database"); onClicked: page.openPage(3) }
                    TfButton {
                        text: qsTr("Preview as %1").arg(Backend.demoIdentities[0]?.name ?? qsTr("demo participant"))
                        variant: "primary"
                        onClicked: Backend.previewWorkspace("participant", Backend.demoIdentities[0]?.id ?? "")
                    }
                    TfButton {
                        text: qsTr("Preview as %1").arg(Backend.hostIdentity.name ?? qsTr("host"))
                        variant: "primary"
                        onClicked: Backend.previewWorkspace("host")
                    }
                    TfButton { text: qsTr("Choose another identity"); variant: "ghost"; onClicked: page.openPage(4) }
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
