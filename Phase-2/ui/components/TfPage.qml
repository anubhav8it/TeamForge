import QtQuick
import TeamForge

// Base for the main screens. The incoming page fades and lifts in; the outgoing page hides
// immediately, so two pages are never drawn on top of each other mid-transition.
Item {
    id: page

    property bool active: false

    visible: opacity > 0
    opacity: 0

    onActiveChanged: {
        if (active) {
            enter.restart()
        } else {
            enter.stop()
            opacity = 0
        }
    }
    Component.onCompleted: if (active) enter.restart()

    transform: Translate { id: lift }

    ParallelAnimation {
        id: enter
        NumberAnimation { target: page; property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { target: lift; property: "y"; from: 10; to: 0; duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }
}
