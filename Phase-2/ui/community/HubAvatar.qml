import QtQuick
import QtQuick.Effects
import TeamForge

// A Community profile picture: the person's uploaded picture in a circle, or their initials on
// the colour they chose. `person` is a profile from the server ({id, name, avatar, color}).
Item {
    id: avatar

    property var person: ({})
    property int size: 36
    readonly property string picture: person && person.avatar ? person.avatar : ""
    readonly property string name: person && person.name ? person.name : ""

    implicitWidth: size
    implicitHeight: size
    Accessible.role: Accessible.Graphic
    Accessible.name: name

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        visible: avatar.picture === ""
        color: avatar.person && avatar.person.color ? avatar.person.color : Theme.surfaceRaised
        TfText {
            anchors.centerIn: parent
            text: Theme.initials(avatar.name) || "?"
            color: "#FFFFFF"
            font.pixelSize: Math.max(11, Math.round(avatar.size * 0.38))
            font.weight: Font.ExtraBold
        }
    }

    Image {
        id: image
        anchors.fill: parent
        visible: false
        source: avatar.picture
        sourceSize.width: avatar.size * 2
        sourceSize.height: avatar.size * 2
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
    Rectangle {
        id: mask
        anchors.fill: parent
        radius: width / 2
        visible: false
        layer.enabled: true
    }
    MultiEffect {
        anchors.fill: parent
        visible: avatar.picture !== ""
        source: image
        maskEnabled: true
        maskSource: mask
    }
}
