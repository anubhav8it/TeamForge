pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import TeamForge

ComboBox {
    id: control

    // Optional display formatter for entries (e.g. skills in capitals); values are unchanged.
    property var format: null

    function shown(text) {
        return control.format ? control.format(text) : text
    }

    implicitHeight: 40
    leftPadding: 12
    rightPadding: 30
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBase
    font.weight: Font.DemiBold
    hoverEnabled: true

    // A text field, as in the Basic style: typing works when `editable`; otherwise it only shows
    // the current entry (disabled, so clicks reach the combo box).
    contentItem: TextField {
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        text: control.editable ? control.editText : control.shown(control.displayText)
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        selectByMouse: true
        font: control.font
        color: control.enabled ? Theme.text : Theme.textMuted
        selectionColor: Theme.accent
        selectedTextColor: "#FFFFFF"
        verticalAlignment: Text.AlignVCenter
        background: null
        Accessible.name: control.Accessible.name
        Accessible.ignored: !control.editable // a plain combo box is one control to assistive tech
    }

    indicator: Canvas {
        x: control.width - width - 12
        y: (control.height - height) / 2
        width: 10
        height: 6
        contextType: "2d"
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = Theme.textSecondary
            ctx.lineWidth = 1.8
            ctx.beginPath()
            ctx.moveTo(0.5, 0.5)
            ctx.lineTo(width / 2, height - 0.5)
            ctx.lineTo(width - 0.5, 0.5)
            ctx.stroke()
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.hovered ? Theme.surfaceRaised : Theme.input
        border.color: control.activeFocus || control.popup.visible ? Theme.accent
                    : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    delegate: ItemDelegate {
        id: option
        required property int index
        required property var model
        width: ListView.view.width
        height: 36
        hoverEnabled: true
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            readonly property var entry: option.model.modelData
            text: control.shown(entry !== null && typeof entry === "object" ? (entry[control.textRole] ?? "") : (entry ?? ""))
            textFormat: Text.PlainText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBase
            font.weight: control.currentIndex === option.index ? Font.Bold : Font.Medium
            color: control.currentIndex === option.index ? Theme.text : Theme.textSecondary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.radiusSmall - 2
            color: option.highlighted ? Theme.surfaceHover : "transparent"
            Rectangle {
                visible: control.currentIndex === option.index
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: parent.height - 14
                radius: 1.5
                color: Theme.accent
            }
        }
    }

    popup: Popup {
        y: control.height + 4
        width: Math.max(control.width, 220)
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 360)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surfaceRaised
            border.color: Theme.borderStrong
        }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animFast }
                NumberAnimation { property: "y"; from: control.height; to: control.height + 4; duration: Theme.animFast }
            }
        }
    }
}
