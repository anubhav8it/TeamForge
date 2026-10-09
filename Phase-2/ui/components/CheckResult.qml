pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import TeamForge

// One developer check / maintenance result: {name, ok, message, details, durationMs}.
Rectangle {
    id: card

    property var result: ({})
    readonly property bool hasResult: result.name !== undefined

    visible: hasResult
    implicitHeight: column.implicitHeight + 24
    radius: Theme.radiusSmall
    color: Theme.surfaceRaised
    border.color: (result.ok ?? true) ? Theme.border : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5)

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Chip {
                text: card.result.ok ? qsTr("PASS") : qsTr("FAIL")
                tone: card.result.ok ? "success" : "accent"
            }
            TfText {
                text: card.result.name ?? ""
                font.weight: Font.ExtraBold
                Layout.fillWidth: true
            }
            TfText {
                text: qsTr("%1 ms").arg((card.result.durationMs ?? 0).toFixed(1))
                variant: "muted"
                font.pixelSize: Theme.fontXs
            }
        }
        TfText {
            text: card.result.message ?? ""
            variant: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }
        Repeater {
            model: card.result.details ?? []
            delegate: TfText {
                required property string modelData
                text: "·  " + modelData
                variant: "muted"
                font.pixelSize: Theme.fontXs
                wrapMode: Text.WrapAnywhere
                elide: Text.ElideNone
                Layout.fillWidth: true
            }
        }
    }
}
