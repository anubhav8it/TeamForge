pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import TeamForge

// Team chat: the person's teams on the left (latest message, unread count), the chosen team's
// conversation on the right (Hub.chat: chronological, older messages on request, new ones live).
// Only members can open a chat; the server checks that for every request.
Item {
    id: messages

    property int teamId: 0
    property bool pageActive: true
    signal navigate(string route)
    signal notify(string message)

    // Phones: the list first; choosing a chat opens it full screen, with a back button.
    readonly property bool narrow: width < 720
    property var conversations: []
    property bool loadingList: true
    readonly property var current: conversations.find(c => c.teamId === teamId) ?? null

    function loadList() {
        Hub.get("/conversations", (status, data) => {
            loadingList = false
            if (status === 200) {
                conversations = data.items
                if (teamId === 0 && conversations.length > 0 && !narrow)
                    messages.navigate("messages/" + conversations[0].teamId)
            }
        })
    }

    onTeamIdChanged: if (teamId > 0) Hub.chat.open(teamId); else Hub.chat.close()
    Component.onCompleted: {
        loadList()
        if (teamId > 0)
            Hub.chat.open(teamId)
    }
    Component.onDestruction: {
        Hub.chat.active = false
        Hub.chat.close()
    }
    Binding { target: Hub.chat; property: "active"; value: messages.pageActive && messages.visible }

    Connections {
        target: Hub
        // unread counts and previews follow the summary refresh
        function onSummaryChanged() { messages.loadList() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: messages.narrow ? 8 : 20
        spacing: Theme.gap

        // ---------------------------------------------------------------- conversations
        Surface {
            visible: !messages.narrow || messages.teamId === 0
            Layout.fillWidth: messages.narrow
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4
                TfText { text: qsTr("Team chats"); variant: "label"; Layout.leftMargin: 6; Layout.topMargin: 4 }
                HubSkeleton { visible: messages.loadingList; rows: 3; rowHeight: 60; Layout.fillWidth: true }
                EmptyState {
                    visible: !messages.loadingList && messages.conversations.length === 0
                    title: qsTr("No team chats")
                    message: qsTr("Every team has a private chat for its members. Join or create a team to start one.")
                    actionText: qsTr("Find a team")
                    Layout.fillWidth: true
                    Layout.topMargin: 30
                    onActionTriggered: messages.navigate("teams")
                }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: messages.conversations
                    delegate: Rectangle {
                        id: conv
                        required property var modelData
                        readonly property bool selected: modelData.teamId === messages.teamId
                        width: ListView.view.width
                        height: 64
                        radius: Theme.radiusSmall
                        color: selected ? Theme.surfaceRaised : convHover.hovered ? Theme.surfaceHover : "transparent"
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name + (modelData.unread > 0 ? qsTr(", %1 unread").arg(modelData.unread) : "")
                        Accessible.onPressAction: messages.navigate("messages/" + conv.modelData.teamId)
                        Rectangle {
                            visible: conv.selected
                            x: 0; width: 3; height: parent.height - 18
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 1.5
                            color: Theme.accent
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 10
                            TeamAvatar { name: conv.modelData.name; size: 38 }
                            ColumnLayout {
                                spacing: 0
                                Layout.fillWidth: true
                                RowLayout {
                                    Layout.fillWidth: true
                                    TfText { text: conv.modelData.name; font.weight: Font.Bold; Layout.fillWidth: true }
                                    TfText {
                                        text: conv.modelData.lastMessage ? Hub.relativeTime(conv.modelData.lastMessage.createdAt) : ""
                                        variant: "muted"; font.pixelSize: 12
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    TfText {
                                        text: conv.modelData.lastMessage
                                              ? (conv.modelData.lastMessage.mine ? qsTr("You: ") : conv.modelData.lastMessage.author + ": ")
                                                + conv.modelData.lastMessage.body.replace(/\n/g, " ")
                                              : qsTr("No messages yet")
                                        variant: "muted"
                                        font.pixelSize: Theme.fontXs
                                        font.weight: conv.modelData.unread > 0 ? Font.Bold : Font.Medium
                                        color: conv.modelData.unread > 0 ? Theme.textSecondary : Theme.textMuted
                                        Layout.fillWidth: true
                                    }
                                    HubBadge { count: conv.modelData.unread }
                                }
                            }
                        }
                        HoverHandler { id: convHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: messages.navigate("messages/" + conv.modelData.teamId) }
                    }
                }
            }
        }

        // ---------------------------------------------------------------- the conversation
        Surface {
            visible: !messages.narrow || messages.teamId > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            EmptyState {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 40)
                visible: messages.teamId === 0
                title: qsTr("Choose a team chat")
                message: qsTr("Messages are visible only to the team's members.")
            }

            ColumnLayout {
                anchors.fill: parent
                visible: messages.teamId > 0
                spacing: 0

                // header
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: messages.narrow ? 8 : 14
                    spacing: 10
                    TfButton {
                        visible: messages.narrow
                        text: "‹"
                        variant: "ghost"
                        compact: true
                        font.pixelSize: 22
                        Accessible.name: qsTr("Back to the list")
                        onClicked: messages.navigate("messages")
                    }
                    TeamAvatar {
                        name: messages.current ? messages.current.name : ""
                        size: 36
                    }
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TfText { text: messages.current ? messages.current.name : qsTr("Team chat"); variant: "heading"; Layout.fillWidth: true }
                        RowLayout {
                            spacing: 6
                            Rectangle { width: 7; height: 7; radius: 3.5; color: Hub.chat.live ? Theme.success : Theme.textMuted }
                            TfText {
                                text: (messages.current ? Theme.count(messages.current.memberCount, qsTr("member"), qsTr("members")) + " · " : "")
                                      + (Hub.chat.live ? qsTr("live") : Hub.chat.loading ? qsTr("connecting…") : qsTr("offline"))
                                variant: "muted"
                                font.pixelSize: Theme.fontXs
                            }
                        }
                    }
                    TfButton { text: qsTr("Team page"); compact: true; variant: "ghost"; onClicked: messages.navigate("teams/" + messages.teamId) }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                // messages
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    HubSkeleton {
                        anchors.fill: parent
                        anchors.margins: 16
                        visible: Hub.chat.loading
                        rows: 4
                        rowHeight: 56
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        width: Math.min(420, parent.width - 40)
                        visible: !Hub.chat.loading && Hub.chat.error === "" && chatView.count === 0
                        title: qsTr("No messages yet")
                        message: qsTr("Start the conversation. Everyone in the team will see it.")
                    }
                    EmptyState {
                        anchors.centerIn: parent
                        width: Math.min(420, parent.width - 40)
                        visible: Hub.chat.error !== "" && chatView.count === 0
                        title: qsTr("Chat unavailable")
                        message: Hub.chat.error
                    }

                    ListView {
                        id: chatView
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        clip: true
                        model: Hub.chat
                        spacing: 2
                        visible: !Hub.chat.loading
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Messages")

                        header: Item {
                            width: chatView.width
                            height: Hub.chat.hasOlder ? 48 : 12
                            TfButton {
                                anchors.centerIn: parent
                                visible: Hub.chat.hasOlder
                                text: Hub.chat.loadingOlder ? qsTr("Loading…") : qsTr("Load earlier messages")
                                compact: true
                                variant: "ghost"
                                enabled: !Hub.chat.loadingOlder
                                onClicked: {
                                    chatView.keepFrom = chatView.contentHeight - chatView.contentY
                                    Hub.chat.loadOlder()
                                }
                            }
                        }
                        footer: Item { width: chatView.width; height: 12 }

                        // Keep the view where it was when older messages are added above.
                        property real keepFrom: -1
                        onContentHeightChanged: {
                            if (keepFrom >= 0) {
                                contentY = contentHeight - keepFrom
                                keepFrom = -1
                            }
                        }

                        delegate: HubChatBubble {
                            width: chatView.width
                            showUnreadDivider: index === Hub.chat.firstUnreadRow
                        }

                        Connections {
                            target: Hub.chat
                            function onAppended(mine) {
                                // follow the conversation when the person is at the bottom or wrote it
                                if (mine || chatView.atYEnd || chatView.contentHeight - chatView.contentY - chatView.height < 140)
                                    Qt.callLater(chatView.positionViewAtEnd)
                            }
                        }
                    }
                }

                // composer
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                TfText {
                    visible: Hub.chat.error !== "" && chatView.count > 0
                    text: Hub.chat.error
                    color: Theme.accentHover
                    font.pixelSize: Theme.fontXs
                    Layout.leftMargin: 16
                    Layout.topMargin: 8
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    spacing: 10
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Math.min(140, Math.max(44, input.implicitHeight + 18))
                        radius: Theme.radiusSmall
                        color: Theme.input
                        border.width: input.activeFocus ? 2 : 1
                        border.color: input.activeFocus ? Theme.accent : Theme.border
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 2
                            TextArea {
                                id: input
                                enabled: Hub.chat.live || Hub.chat.error === ""
                                placeholderText: qsTr("Write a message…  (Enter sends, Shift+Enter adds a line)")
                                placeholderTextColor: Theme.textMuted
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBase
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                background: null
                                Accessible.name: qsTr("Message")
                                Keys.onPressed: event => {
                                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                                        event.accepted = true
                                        sendButton.clicked()
                                    }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 2
                        TfButton {
                            id: sendButton
                            text: Hub.chat.sending ? qsTr("Sending…") : qsTr("Send")
                            variant: "primary"
                            enabled: !Hub.chat.sending && input.text.trim() !== "" && Hub.characterCount(input.text) <= 2000
                            onClicked: {
                                const problem = Hub.chat.send(input.text)
                                if (problem === "")
                                    input.text = ""
                                else
                                    messages.notify(problem)
                            }
                        }
                        TfText {
                            visible: input.text.length > 1700
                            text: Hub.characterCount(input.text) + " / 2000"
                            variant: "muted"
                            font.pixelSize: 12
                            color: Hub.characterCount(input.text) > 2000 ? Theme.warning : Theme.textMuted
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
