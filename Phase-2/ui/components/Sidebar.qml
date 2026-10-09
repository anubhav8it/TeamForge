pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// Brand, the active workspace's navigation, a context card for that workspace and the way
// back to the workspace choice. Collapses to icons on small windows.
Rectangle {
    id: sidebar

    property int currentIndex: 0
    property bool compact: false
    property var pages: []          // [{title, nav, glyph}]
    property bool navigationEnabled: true
    signal navigate(int index)

    readonly property string workspace: Backend.workspace
    readonly property color workspaceColor: Theme.workspaceColor(workspace)
    readonly property var project: Backend.currentRequirement
    readonly property var me: Backend.myProfile

    color: Theme.sidebar

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: sidebar.compact ? 10 : 16
        spacing: 4

        Image {
            source: sidebar.compact ? Theme.mark : Theme.logo
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            sourceSize.height: 96
            Layout.preferredHeight: sidebar.compact ? 28 : 42
            Layout.fillWidth: true
            Layout.topMargin: 8
            horizontalAlignment: sidebar.compact ? Image.AlignHCenter : Image.AlignLeft
        }

        // Which workspace this is.
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 14
            Layout.bottomMargin: 14
            implicitHeight: 30
            radius: 15
            color: Qt.rgba(sidebar.workspaceColor.r, sidebar.workspaceColor.g, sidebar.workspaceColor.b, 0.16)
            border.color: Qt.rgba(sidebar.workspaceColor.r, sidebar.workspaceColor.g, sidebar.workspaceColor.b, 0.45)
            TfText {
                anchors.centerIn: parent
                text: sidebar.compact ? Theme.workspaceName(sidebar.workspace).charAt(0)
                                      : Theme.workspaceName(sidebar.workspace) + (Backend.previewing ? qsTr(" · preview") : qsTr(" workspace"))
                font.pixelSize: Theme.fontXs
                font.weight: Font.ExtraBold
                font.letterSpacing: 0.4
                color: Qt.lighter(sidebar.workspaceColor, 1.25)
            }
        }

        Repeater {
            model: sidebar.pages
            delegate: NavItem {
                required property var modelData
                required property int index
                step: index + 1
                glyph: modelData.glyph ?? String(index + 1)
                text: modelData.title
                detail: modelData.nav
                compact: sidebar.compact
                selected: sidebar.currentIndex === index
                selectedColor: sidebar.workspaceColor
                enabled: sidebar.navigationEnabled
                Layout.fillWidth: true
                onClicked: sidebar.navigate(index)
            }
        }

        // Context: the host's active project, or the participant's own profile.
        Rectangle {
            visible: !sidebar.compact && sidebar.workspace === "host" && sidebar.project.id !== undefined
            Layout.fillWidth: true
            Layout.topMargin: 18
            implicitHeight: hostColumn.implicitHeight + 28
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 12
                width: 3
                radius: 1.5
                color: Theme.typeColor(sidebar.project.type ?? "")
            }
            ColumnLayout {
                id: hostColumn
                anchors.fill: parent
                anchors.margins: 14
                anchors.leftMargin: 24
                spacing: 3
                // The one local host (no accounts): shown so it is clear who is acting.
                RowLayout {
                    spacing: 8
                    Layout.bottomMargin: 8
                    Avatar {
                        name: Backend.hostIdentity.name ?? ""
                        seed: Backend.hostIdentity.id ?? ""
                        size: 26
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TfText { text: Backend.hostIdentity.name ?? ""; font.weight: Font.Bold; font.pixelSize: Theme.fontSm; wrapMode: Text.WordWrap; elide: Text.ElideNone; Layout.fillWidth: true }
                        TfText { text: Backend.hostIdentity.label ?? ""; variant: "muted"; font.pixelSize: Theme.fontXs }
                    }
                }
                TfText { text: qsTr("Active project"); variant: "label" }
                TfText {
                    text: sidebar.project.name ?? ""
                    font.weight: Font.ExtraBold
                    Layout.fillWidth: true
                }
                TfText {
                    text: [sidebar.project.type, qsTr("%1–%2 members").arg(sidebar.project.minTeamSize ?? 0)
                                                                   .arg(sidebar.project.maxTeamSize ?? 0)]
                          .filter(part => part).join(" · ")
                    variant: "muted"
                    font.pixelSize: Theme.fontXs
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
        }
        Rectangle {
            visible: !sidebar.compact && sidebar.workspace === "participant" && sidebar.me.id !== undefined
            Layout.fillWidth: true
            Layout.topMargin: 18
            implicitHeight: meRow.implicitHeight + 24
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            RowLayout {
                id: meRow
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                Avatar {
                    name: sidebar.me.name ?? ""
                    seed: sidebar.me.id ?? ""
                    size: 36
                }
                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true
                    TfText {
                        text: sidebar.me.name ?? ""
                        font.weight: Font.ExtraBold
                        Layout.fillWidth: true
                    }
                    TfText {
                        text: qsTr("Profile %1 complete").arg(Theme.percent(sidebar.me.completion ?? 0))
                        variant: "muted"
                        font.pixelSize: Theme.fontXs
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Save state (host and developer edit data that is saved explicitly).
        RowLayout {
            visible: !sidebar.compact && sidebar.workspace !== "participant"
            spacing: 8
            Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: Backend.dirty ? Theme.warning : Theme.success
            }
            TfText {
                text: Backend.dirty ? qsTr("Unsaved changes") : qsTr("All changes saved")
                variant: "muted"
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }
        TfButton {
            visible: sidebar.workspace !== "participant"
            text: sidebar.compact ? qsTr("Save") : qsTr("Save changes")
            variant: Backend.dirty ? "primary" : "secondary"
            enabled: Backend.dirty
            compact: true
            leftPadding: sidebar.compact ? 2 : 14
            rightPadding: sidebar.compact ? 2 : 14
            Layout.fillWidth: true
            onClicked: Backend.save()
        }
        TfButton {
            text: sidebar.compact ? "⇄" : Backend.previewing ? qsTr("End preview") : qsTr("Switch workspace")
            variant: "ghost"
            compact: true
            Layout.fillWidth: true
            Accessible.name: Backend.previewing ? qsTr("End preview") : qsTr("Switch workspace")
            onClicked: Backend.previewing ? Backend.endPreview() : Backend.leaveWorkspace()
        }
    }
}
