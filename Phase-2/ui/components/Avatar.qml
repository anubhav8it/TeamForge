import QtQuick
import TeamForge

// Generated profile avatar: initials on a two-colour gradient disc whose colours and angle are
// picked deterministically from the participant id, so the same person always looks the same.
// No image files. `highlighted` (e.g. in the team) adds a red ring; `selected` a white one.
Item {
    id: avatar

    property string name
    property string seed: name
    property int size: 32
    property bool highlighted: false
    property bool selected: false

    readonly property var style: Theme.avatarStyle(seed)
    readonly property bool ringed: highlighted || selected

    implicitWidth: size
    implicitHeight: size

    Item {
        id: disc
        anchors.fill: parent
        anchors.margins: avatar.ringed ? 3 : 0
        Behavior on anchors.margins { NumberAnimation { duration: Theme.animFast } }

        // Gradients in Qt Quick run vertically; rotating the round disc turns that into any angle.
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            rotation: avatar.style.angle
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: avatar.style.from }
                GradientStop { position: 1.0; color: avatar.style.to }
            }
        }
        // Soft highlight in the upper half gives the disc some depth.
        Rectangle {
            x: disc.width * 0.2
            y: disc.height * 0.08
            width: disc.width * 0.6
            height: disc.height * 0.34
            radius: height / 2
            color: "#1CFFFFFF"
            visible: avatar.size >= 30
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: avatar.ringed ? 2 : 0
        border.color: avatar.highlighted ? Theme.accent : Theme.text
        visible: avatar.ringed
    }

    Text {
        anchors.centerIn: parent
        text: Theme.initials(avatar.name)
        textFormat: Text.PlainText
        font.family: Theme.fontFamily
        font.pixelSize: Math.round(avatar.size * (avatar.ringed ? 0.34 : 0.38))
        font.weight: Font.ExtraBold
        font.letterSpacing: 0.3
        color: "#FFFFFF"
        style: Text.Raised
        styleColor: "#33000000"
    }
}
