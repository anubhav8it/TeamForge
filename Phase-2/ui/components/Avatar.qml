import QtQuick
import QtQuick.Effects
import TeamForge

// The one avatar for a person, used everywhere in TeamForge so the same person always looks the
// same: their uploaded photo, or their initials on their own colour.
//
// Who it shows, in this order:
//   person  a Community profile ({id, name, avatar, color}), e.g. a chat author or an applicant
//   seed    a participant profile id: when an account uses that profile (Hub.identityForStudent),
//           its photo and colour are shown; otherwise (sample records, the offline desktop app)
//           initials on a gradient picked deterministically from the id
//   photo   an explicit picture (a data: URL), e.g. the preview while choosing a new photo
// `name` is the fallback for the initials. Initials always come from the real name ("Priya Nair"
// -> "PN"). `highlighted` (e.g. in the team) adds a red ring; `selected` a white one.
Item {
    id: avatar

    property string name
    property string seed: name
    property int size: 32
    property bool highlighted: false
    property bool selected: false
    property var person: null
    property string photo: ""

    readonly property var identity: {
        void Hub.identityRevision // re-read when identities change
        if (person)
            return person
        const found = seed !== "" ? Hub.identityForStudent(seed) : null
        return found && found.name !== undefined ? found : null
    }
    readonly property string shownName: identity && identity.name ? identity.name : name
    readonly property string pictureRef: photo !== "" ? photo : identity && identity.avatar ? identity.avatar : ""
    readonly property string picture: {
        void Hub.photoRevision // re-read when a photo arrives
        return pictureRef !== "" ? Hub.photo(pictureRef) : ""
    }
    // Accounts have their own colour (chosen in their profile); others a gradient from the seed.
    readonly property var style: identity && identity.color
                                 ? { from: Qt.lighter(identity.color, 1.12), to: Qt.darker(identity.color, 1.5), angle: -45 }
                                 : Theme.avatarStyle(seed !== "" ? seed : shownName)
    readonly property bool ringed: highlighted || selected

    implicitWidth: size
    implicitHeight: size
    Accessible.role: Accessible.Graphic
    Accessible.name: shownName

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
            visible: avatar.size >= 30 && avatar.picture === ""
        }

        Text {
            anchors.centerIn: parent
            visible: avatar.picture === ""
            text: Theme.initials(avatar.shownName) || "?"
            textFormat: Text.PlainText
            font.family: Theme.fontFamily
            font.pixelSize: Math.max(10, Math.round(disc.width * 0.38))
            font.weight: Font.ExtraBold
            font.letterSpacing: 0.3
            color: "#FFFFFF"
            style: Text.Raised
            styleColor: "#33000000"
        }

        // The photo, cut to a circle.
        Image {
            id: image
            anchors.fill: parent
            visible: false
            source: avatar.picture
            sourceSize.width: Math.max(64, avatar.size * 2)
            sourceSize.height: Math.max(64, avatar.size * 2)
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
            visible: avatar.picture !== "" && image.status === Image.Ready
            source: image
            maskEnabled: true
            maskSource: mask
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
}
