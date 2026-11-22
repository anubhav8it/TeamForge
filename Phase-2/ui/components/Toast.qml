import QtQuick
import QtQuick.Layouts
import TeamForge

// Short confirmation message (e.g. "Team saved") that slides in and fades out on its own.
Rectangle {
    id: toast

    function hide() {
        hideTimer.stop()
        opacity = 0
    }

    function show(message: string) {
        label.text = message
        opacity = 1
        hideTimer.restart()
    }

    implicitWidth: row.implicitWidth + 32
    implicitHeight: 44
    radius: height / 2
    color: Theme.surfaceRaised
    border.color: Theme.borderStrong
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(Theme.surfaceRaised, 1.2) }
        GradientStop { position: 1.0; color: Theme.surfaceRaised }
    }
    // shade under the toast
    Rectangle { z: -1; anchors.fill: parent; anchors.topMargin: 6; anchors.bottomMargin: -10; radius: parent.radius; color: Theme.shadow; opacity: 0.7 }
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
    transform: Translate { y: (1 - toast.opacity) * -8 }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Rectangle {
            implicitWidth: 22
            implicitHeight: 22
            radius: 11
            color: Theme.success
            Text {
                anchors.centerIn: parent
                text: "✓"
                font.pixelSize: 13
                font.weight: Font.Black
                color: "#0E2A1E"
            }
        }
        TfText {
            id: label
            font.weight: Font.Bold
        }
    }
    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: toast.opacity = 0
    }
}
