pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// All applications in one place. "To review": applications to the teams the person leads or
// helps run and to the events they organise. "My applications": the person's own, with their
// status, the reviewer's message and Withdraw while allowed. Filters by type and status, search
// and pages.
Item {
    id: apps

    signal navigate(string route)
    signal notify(string message)

    property string view: Hub.route.indexOf("tab=mine") >= 0 || Hub.pendingDecisions === 0 && !(Hub.summary.canOrganize ?? false) ? "mine" : "received"
    property string kind: ""
    property string status: view === "received" ? "pending" : ""
    property string query: ""
    property var result: ({ items: [], page: 1, pages: 1, total: 0 })
    property bool loading: true
    readonly property bool narrow: width < 720

    function load(page) {
        loading = true
        const path = (view === "received" ? "/applications/received" : "/applications/mine")
                   + "?page=" + (page ?? 1) + "&status=" + status + (kind !== "" ? "&kind=" + kind : "") + "&q=" + encodeURIComponent(query)
        Hub.get(path, (code, data) => {
            loading = false
            if (code === 200) result = data
            else apps.notify(data.error ?? qsTr("Applications could not be loaded."))
        })
    }
    Component.onCompleted: load(1)
    onViewChanged: { status = view === "received" ? "pending" : ""; load(1) }
    Timer { id: searchDelay; interval: 300; onTriggered: apps.load(1) }

    DecisionDialog { id: decisionDialog; onDecided: s => { apps.notify(qsTr("Application %1").arg(Hub.statusLabel(s).toLowerCase())); apps.load(apps.result.page) } }
    PersonDialog { id: personDialog }
    ConfirmDialog {
        id: confirm
        property var target: null
        onConfirmed: {
            const app = target
            Hub.post((app.kind === "team" ? "/team-applications/" : "/event-applications/") + app.id + "/withdraw", {}, (code, data) => {
                if (code === 200) { apps.notify(qsTr("Application withdrawn")); apps.load(apps.result.page) }
                else apps.notify(data.error)
            })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: apps.narrow ? 14 : 24
        anchors.topMargin: apps.narrow ? 14 : 18
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: apps.narrow ? 1 : 3
            columnSpacing: 12
            rowSpacing: 10
            Segmented {
                options: [{ text: qsTr("To review  %1").arg(Hub.pendingDecisions), value: "received" }, { text: qsTr("My applications"), value: "mine" }]
                currentValue: apps.view
                onActivated: value => apps.view = value
            }
            Item { Layout.fillWidth: true; visible: !apps.narrow }
            TfTextField {
                placeholderText: apps.view === "received" ? qsTr("Search applicant, team or event") : qsTr("Search team or event")
                Layout.preferredWidth: 280
                Layout.fillWidth: apps.narrow
                onTextChanged: { apps.query = text.trim(); searchDelay.restart() }
            }
        }
        ScrollRow {
            Layout.fillWidth: true
            spacing: 12
            Segmented {
                options: [{ text: qsTr("All"), value: "" }, { text: qsTr("Teams"), value: "team" }, { text: qsTr("Events"), value: "event" }]
                currentValue: apps.kind
                onActivated: value => { apps.kind = value; apps.load(1) }
            }
            Segmented {
                options: [{ text: qsTr("Any status"), value: "" }, { text: qsTr("Pending"), value: "pending" }, { text: qsTr("Waitlisted"), value: "waitlisted" },
                          { text: qsTr("Approved"), value: "approved" }, { text: qsTr("Rejected"), value: "rejected" }, { text: qsTr("Withdrawn"), value: "withdrawn" }]
                currentValue: apps.status
                onActivated: value => { apps.status = value; apps.load(1) }
            }
        }

        HubSkeleton { visible: apps.loading && apps.result.items.length === 0; rows: 3; rowHeight: 120; Layout.fillWidth: true }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            visible: apps.result.items.length > 0
            ColumnLayout {
                width: parent.width
                spacing: 10
                Repeater {
                    model: apps.result.items
                    delegate: ApplicationCard {
                        required property var modelData
                        application: modelData
                        reviewer: apps.view === "received"
                        Layout.fillWidth: true
                        onDecide: (app, choice) => decisionDialog.openFor(app, choice)
                        onWithdraw: app => {
                            confirm.target = app
                            confirm.ask(qsTr("Withdraw your application to %1?").arg(app.targetName),
                                        app.status === "approved" ? qsTr("Your place goes to someone else.") : qsTr("You can apply again later."),
                                        qsTr("Withdraw"))
                        }
                        onOpenProfile: id => personDialog.show(id)
                        onOpenTarget: link => Hub.openNotification({ link: link, read: true })
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: apps.result.page; pages: apps.result.pages; total: apps.result.total
                    onPageRequested: p => apps.load(p)
                }
            }
        }
        EmptyState {
            visible: !apps.loading && apps.result.items.length === 0
            title: apps.view === "received" ? qsTr("Nothing to review") : qsTr("No applications yet")
            message: apps.view === "received" ? qsTr("Applications to teams you lead and events you organise appear here.")
                                              : qsTr("Find a team or an event and apply; you can follow every application here.")
            actionText: apps.view === "received" ? "" : qsTr("Find a team")
            onActionTriggered: apps.navigate("teams")
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
