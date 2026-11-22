pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Browse teams: search, "recruiting" and "my teams" filters, pages of team cards, and "Create a team".
Item {
    id: teams

    signal navigate(string route)
    signal notify(string message)

    property var result: ({ items: [], page: 1, pages: 1, total: 0 })
    property bool loading: true
    property string error: ""
    property string query: ""
    property bool recruitingOnly: false
    property bool mineOnly: false
    readonly property int columns: width > 1250 ? 3 : width > 720 ? 2 : 1
    readonly property bool narrow: width < 720

    function load(page) {
        loading = true
        const q = "?page=" + (page ?? 1) + "&q=" + encodeURIComponent(query)
                + (recruitingOnly ? "&recruiting=1" : "") + (mineOnly ? "&mine=1" : "")
        Hub.get("/teams" + q, (status, data) => {
            loading = false
            if (status === 200) { result = data; error = "" }
            else error = data.error ?? qsTr("Teams could not be loaded.")
        })
    }

    Component.onCompleted: {
        load(1)
        if (Hub.route.indexOf("create=1") >= 0)
            createForm.openFor(null)
    }
    Timer { id: searchDelay; interval: 300; onTriggered: teams.load(1) }

    TeamFormDialog {
        id: createForm
        onSaved: team => {
            teams.notify(qsTr("%1 created. You are its leader.").arg(team.name))
            teams.navigate("teams/" + team.id)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: teams.narrow ? 14 : 24
        anchors.topMargin: teams.narrow ? 14 : 18
        spacing: Theme.gap

        GridLayout {
            Layout.fillWidth: true
            columns: teams.narrow ? 2 : 5
            columnSpacing: 10
            rowSpacing: 10
            TfTextField {
                Layout.columnSpan: teams.narrow ? 2 : 1
                Layout.fillWidth: teams.narrow
                placeholderText: qsTr("Search teams, skills or roles")
                Layout.preferredWidth: 320
                onTextChanged: { teams.query = text.trim(); searchDelay.restart() }
            }
            Chip {
                large: true
                text: qsTr("Recruiting")
                tone: teams.recruitingOnly ? "success" : "muted"
                Accessible.role: Accessible.CheckBox
                Accessible.checked: teams.recruitingOnly
                Accessible.name: text
                Accessible.onPressAction: { teams.recruitingOnly = !teams.recruitingOnly; teams.load(1) }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { teams.recruitingOnly = !teams.recruitingOnly; teams.load(1) } }
            }
            Chip {
                large: true
                text: qsTr("My teams")
                tone: teams.mineOnly ? "info" : "muted"
                Accessible.role: Accessible.CheckBox
                Accessible.checked: teams.mineOnly
                Accessible.name: text
                Accessible.onPressAction: { teams.mineOnly = !teams.mineOnly; teams.load(1) }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { teams.mineOnly = !teams.mineOnly; teams.load(1) } }
            }
            Item { Layout.fillWidth: true; visible: !teams.narrow }
            TfButton {
                text: qsTr("Create a team")
                variant: "primary"
                compact: true
                Layout.columnSpan: teams.narrow ? 2 : 1
                Layout.fillWidth: teams.narrow
                onClicked: createForm.openFor(null)
            }
        }

        HubSkeleton { visible: teams.loading && teams.result.items.length === 0; rows: 3; rowHeight: 150; Layout.fillWidth: true }
        ErrorBanner { message: teams.error; Layout.fillWidth: true; onDismissed: teams.error = "" }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            visible: teams.result.items.length > 0

            ColumnLayout {
                width: parent.width
                spacing: Theme.gap
                GridLayout {
                    Layout.fillWidth: true
                    columns: teams.columns
                    columnSpacing: Theme.gap
                    rowSpacing: Theme.gap
                    Repeater {
                        model: teams.result.items
                        delegate: HubTeamCard {
                            required property var modelData
                            team: modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            onOpened: teams.navigate("teams/" + modelData.id)
                        }
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: teams.result.page; pages: teams.result.pages; total: teams.result.total
                    onPageRequested: p => teams.load(p)
                }
            }
        }

        EmptyState {
            visible: !teams.loading && teams.result.items.length === 0 && teams.error === ""
            title: teams.query !== "" || teams.recruitingOnly || teams.mineOnly ? qsTr("No teams match") : qsTr("No teams yet")
            message: teams.query !== "" || teams.recruitingOnly || teams.mineOnly ? qsTr("Try another search or filter.")
                                                                               : qsTr("Be the first: create a team and invite people to apply.")
            actionText: qsTr("Create a team")
            onActionTriggered: createForm.openFor(null)
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
