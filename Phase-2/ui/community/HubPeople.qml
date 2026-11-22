pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// People who accept anonymous messages (developers, hosts and others who opted in), to ask a
// question before applying or collaborating.
Item {
    id: people

    signal navigate(string route)
    signal notify(string message)

    property var result: ({ items: [], page: 1, pages: 1, total: 0 })
    property bool loading: true
    property string query: ""

    function load(page) {
        loading = true
        Hub.get("/people?anonymous=1&page=" + (page ?? 1) + "&q=" + encodeURIComponent(query), (code, data) => {
            loading = false
            if (code === 200) result = data
        })
    }
    Component.onCompleted: load(1)
    Timer { id: searchDelay; interval: 300; onTriggered: people.load(1) }

    AnonymousDialog {
        id: anonDialog
        onSent: threadId => { people.notify(qsTr("Sent anonymously.")); people.navigate("anonymous/" + threadId) }
    }
    PersonDialog { id: personDialog; onMessageAnonymously: p => anonDialog.openFor("person", p.id, p.name) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                TfText { text: qsTr("People you can ask anonymously"); variant: "section" }
                TfText { text: qsTr("Developers, hosts and others who accept anonymous questions."); variant: "muted" }
            }
            TfTextField {
                placeholderText: qsTr("Search people or skills")
                Layout.preferredWidth: 280
                onTextChanged: { people.query = text.trim(); searchDelay.restart() }
            }
        }
        HubSkeleton { visible: people.loading && people.result.items.length === 0; rows: 3; rowHeight: 70; Layout.fillWidth: true }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            visible: people.result.items.length > 0
            ColumnLayout {
                width: parent.width
                spacing: 8
                Repeater {
                    model: people.result.items
                    delegate: Surface {
                        id: row
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 74
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: Theme.border
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 12
                            HubAvatar { person: row.modelData; size: 44 }
                            ColumnLayout {
                                spacing: 1
                                Layout.fillWidth: true
                                RowLayout {
                                    spacing: 8
                                    TfText { text: row.modelData.name; font.weight: Font.Bold }
                                    Chip { text: row.modelData.role; tone: row.modelData.role === "Developer" ? "tint" : "neutral"; tint: Theme.violet }
                                }
                                TfText { text: row.modelData.headline; variant: "muted"; Layout.fillWidth: true }
                            }
                            TfButton { text: qsTr("Profile"); compact: true; variant: "ghost"; onClicked: personDialog.show(row.modelData.id) }
                            TfButton {
                                visible: !row.modelData.isMe
                                text: qsTr("Message anonymously")
                                compact: true
                                onClicked: anonDialog.openFor("person", row.modelData.id, row.modelData.name)
                            }
                        }
                    }
                }
                HubPager {
                    Layout.fillWidth: true
                    page: people.result.page; pages: people.result.pages; total: people.result.total
                    onPageRequested: p => people.load(p)
                }
            }
        }
        EmptyState {
            visible: !people.loading && people.result.items.length === 0
            title: qsTr("Nobody to show")
            message: qsTr("People appear here when they turn on “Accept anonymous messages” in their Community profile.")
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
