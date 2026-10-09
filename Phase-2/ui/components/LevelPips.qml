pragma ComponentBehavior: Bound

import QtQuick
import TeamForge

// Skill level 1-5 as five segments. With `minLevel`, segments up to the minimum are outlined
// and filled ones turn green (meets it) or amber (below it). `interactive` lets users pick.
Row {
    id: pips

    property int level: 0
    property int minLevel: 0
    property bool interactive: false
    property int segmentWidth: interactive ? 22 : 12
    property int segmentHeight: interactive ? 14 : 7
    signal picked(int level)

    spacing: 3

    Repeater {
        model: 5
        delegate: Rectangle {
            id: segment
            required property int index
            readonly property bool filled: index < pips.level
            Accessible.role: pips.interactive ? Accessible.Button : Accessible.Indicator
            Accessible.name: qsTr("Level %1").arg(index + 1)
            Accessible.onPressAction: if (pips.interactive) pips.picked(segment.index + 1)
            width: pips.segmentWidth
            height: pips.segmentHeight
            radius: height / 2
            color: {
                if (!filled)
                    return hover.hovered && pips.interactive ? Theme.surfaceHover : Theme.border
                if (pips.minLevel === 0)
                    return pips.interactive ? Theme.accent : Theme.warning
                return pips.level >= pips.minLevel ? Theme.success : Theme.warning
            }
            border.width: !filled && index < pips.minLevel ? 1 : 0
            border.color: Theme.textMuted
            Behavior on color { ColorAnimation { duration: Theme.animFast } }

            HoverHandler {
                id: hover
                enabled: pips.interactive
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                enabled: pips.interactive
                onTapped: pips.picked(segment.index + 1)
            }
        }
    }
}
