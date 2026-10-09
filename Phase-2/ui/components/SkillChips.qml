pragma ComponentBehavior: Bound

import QtQuick
import TeamForge

// A row of skill chips, most relevant first (callers pass them in order). Shows `maxVisible`
// and a "+N more" chip that expands the rest. Entries are skill names or {skill, level, ...}.
Flow {
    id: chips

    property var skills: []
    property int maxVisible: 4
    property bool showLevels: true
    property bool expandable: true
    property bool expanded: false
    // Optional (entry) => tone name for each chip; defaults to "neutral".
    property var toneFor: null
    property bool large: false

    readonly property int hiddenCount: Math.max(0, skills.length - maxVisible)
    readonly property var visibleSkills: expanded ? skills : skills.slice(0, maxVisible)

    spacing: 6

    Repeater {
        model: chips.visibleSkills
        delegate: Chip {
            required property var modelData
            readonly property bool isObject: typeof modelData === "object"
            skill: true
            large: chips.large
            text: isObject ? modelData.skill + (chips.showLevels && modelData.level > 0 ? "  " + modelData.level : "")
                           : modelData
            tone: chips.toneFor ? chips.toneFor(modelData) : "neutral"
        }
    }
    Chip {
        visible: chips.hiddenCount > 0 && (chips.expandable || !chips.expanded)
        large: chips.large
        text: chips.expanded ? qsTr("Show less") : qsTr("+%1 more").arg(chips.hiddenCount)
        tone: "muted"
        Accessible.role: chips.expandable ? Accessible.Button : Accessible.StaticText
        Accessible.name: text
        Accessible.onPressAction: if (chips.expandable) chips.expanded = !chips.expanded
        HoverHandler { enabled: chips.expandable; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            enabled: chips.expandable
            onTapped: chips.expanded = !chips.expanded
        }
    }
}
