import QtQuick
import QtQuick.Layouts
import TeamForge

// One chat message. Own messages sit on the right in the accent colour; others on the left with
// the sender's picture and name at the start of each group. A day line and the "New messages"
// divider come before the message when they apply.
Item {
    id: bubble

    required property int index
    required property string body
    required property string authorName
    required property var authorColor
    required property var authorAvatar
    required property bool mine
    required property string timeText
    required property string dayLabel
    required property bool startsGroup
    required property bool startsDay
    property bool showUnreadDivider: false

    implicitHeight: column.implicitHeight
    height: implicitHeight
    Accessible.role: Accessible.StaticText
    Accessible.name: (mine ? qsTr("You") : authorName) + ", " + timeText + ": " + body

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 4

        // day separator
        RowLayout {
            visible: bubble.startsDay
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.bottomMargin: 4
            spacing: 10
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
            TfText { text: bubble.dayLabel; variant: "label"; font.pixelSize: 12 }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
        }
        // new messages divider
        RowLayout {
            visible: bubble.showUnreadDivider
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.accent }
            TfText { text: qsTr("New messages"); color: Theme.accentHover; font.pixelSize: 12; font.weight: Font.Bold }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.accent }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: bubble.startsGroup ? 8 : 0
            layoutDirection: bubble.mine ? Qt.RightToLeft : Qt.LeftToRight
            spacing: 8

            Item {
                visible: !bubble.mine
                implicitWidth: 32
                implicitHeight: 32
                Layout.alignment: Qt.AlignTop
                HubAvatar {
                    visible: bubble.startsGroup
                    person: ({ name: bubble.authorName, color: bubble.authorColor, avatar: bubble.authorAvatar })
                    size: 32
                }
            }

            ColumnLayout {
                spacing: 2
                Layout.maximumWidth: bubble.width * 0.72
                Layout.alignment: bubble.mine ? Qt.AlignRight : Qt.AlignLeft

                RowLayout {
                    visible: bubble.startsGroup
                    spacing: 8
                    layoutDirection: bubble.mine ? Qt.RightToLeft : Qt.LeftToRight
                    Layout.alignment: bubble.mine ? Qt.AlignRight : Qt.AlignLeft
                    TfText {
                        text: bubble.mine ? qsTr("You") : bubble.authorName
                        font.weight: Font.Bold
                        font.pixelSize: Theme.fontXs
                        color: bubble.mine ? Theme.textSecondary : (bubble.authorColor ? Qt.lighter(bubble.authorColor, 1.25) : Theme.textSecondary)
                    }
                    TfText { text: bubble.timeText; variant: "muted"; font.pixelSize: 12 }
                }

                Rectangle {
                    Layout.alignment: bubble.mine ? Qt.AlignRight : Qt.AlignLeft
                    implicitWidth: Math.min(text.implicitWidth + 24, bubble.width * 0.72)
                    implicitHeight: text.implicitHeight + 16
                    radius: 14
                    color: bubble.mine ? Theme.accent : Theme.surfaceRaised
                    border.width: bubble.mine ? 0 : 1
                    border.color: Theme.border
                    TextEdit {
                        id: text
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        text: bubble.body
                        textFormat: TextEdit.PlainText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: bubble.mine ? "#FFFFFF" : Theme.text
                        selectionColor: bubble.mine ? "#80FFFFFF" : Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBase
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }
    }
}
