pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import TeamForge

// Position and zoom a new profile photo inside the circle before it is used. Drag to move,
// slide (or scroll) to zoom. The crop itself is made in C++ (Hub.cropAvatar) from the full
// picture, so the result is sharp.
Popup {
    id: dialog

    property string source: ""
    property int sourceWidth: 1
    property int sourceHeight: 1
    signal cropped(string dataUrl)

    readonly property int viewport: Math.min(280, (parent ? parent.width : 400) - 80)
    readonly property real baseScale: viewport / Math.min(sourceWidth, sourceHeight)
    property real zoom: 1
    readonly property real zoomScale: baseScale * zoom
    property real offsetX: 0 // image position relative to the viewport's top-left
    property real offsetY: 0

    function show(src, w, h) {
        source = src
        sourceWidth = Math.max(1, w)
        sourceHeight = Math.max(1, h)
        zoom = 1
        center()
        open()
    }
    function center() {
        offsetX = (viewport - sourceWidth * zoomScale) / 2
        offsetY = (viewport - sourceHeight * zoomScale) / 2
    }
    // The picture always covers the circle.
    function clamp() {
        offsetX = Math.min(0, Math.max(viewport - sourceWidth * zoomScale, offsetX))
        offsetY = Math.min(0, Math.max(viewport - sourceHeight * zoomScale, offsetY))
    }
    function setZoom(value) {
        const before = zoomScale
        const cx = (viewport / 2 - offsetX) / before
        const cy = (viewport / 2 - offsetY) / before
        zoom = Math.max(1, Math.min(4, value))
        offsetX = viewport / 2 - cx * zoomScale
        offsetY = viewport / 2 - cy * zoomScale
        clamp()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(420, parent.width - 24)
    modal: true
    dim: true
    padding: 22
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "#B3050810" }
    background: Surface { radius: Theme.radiusLarge; baseColor: Theme.surfaceRaised; border.color: Theme.borderStrong }

    contentItem: ColumnLayout {
        spacing: 14
        TfText { text: qsTr("Position your photo"); variant: "section"; font.pixelSize: 20; Layout.fillWidth: true }
        TfText {
            text: qsTr("Drag to move it, zoom to fit. Only what is inside the circle is used.")
            variant: "muted"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            Layout.fillWidth: true
        }

        Item {
            id: view
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: dialog.viewport
            implicitHeight: dialog.viewport
            clip: true
            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Photo position")

            Image {
                id: picture
                x: dialog.offsetX
                y: dialog.offsetY
                width: dialog.sourceWidth * dialog.zoomScale
                height: dialog.sourceHeight * dialog.zoomScale
                source: dialog.source
                smooth: true
                mipmap: true
            }
            // Darken outside the circle.
            Rectangle {
                id: hole
                anchors.fill: parent
                radius: width / 2
                visible: false
                layer.enabled: true
            }
            Rectangle {
                id: shade
                anchors.fill: parent
                color: "#B3000000"
                visible: false
                layer.enabled: true
            }
            MultiEffect {
                anchors.fill: parent
                source: shade
                maskEnabled: true
                maskInverted: true
                maskSource: hole
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: "#CCFFFFFF"
            }
            DragHandler {
                target: null
                property real startX
                property real startY
                onActiveChanged: if (active) { startX = dialog.offsetX; startY = dialog.offsetY }
                onTranslationChanged: {
                    dialog.offsetX = startX + translation.x
                    dialog.offsetY = startY + translation.y
                    dialog.clamp()
                }
            }
            WheelHandler {
                onWheel: event => dialog.setZoom(dialog.zoom + event.angleDelta.y / 1200)
            }
            PinchHandler {
                target: null
                property real startZoom: 1
                onActiveChanged: if (active) startZoom = dialog.zoom
                onActiveScaleChanged: dialog.setZoom(startZoom * activeScale)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TfText { text: "−"; variant: "muted"; font.pixelSize: 20 }
            Slider {
                id: zoomSlider
                Layout.fillWidth: true
                from: 1
                to: 4
                value: dialog.zoom
                onMoved: dialog.setZoom(value)
                Accessible.name: qsTr("Zoom")
                background: Rectangle {
                    x: zoomSlider.leftPadding
                    y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                    width: zoomSlider.availableWidth
                    height: 4
                    radius: 2
                    color: Theme.input
                    Rectangle {
                        width: zoomSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                    }
                }
                handle: Rectangle {
                    x: zoomSlider.leftPadding + zoomSlider.visualPosition * (zoomSlider.availableWidth - width)
                    y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                    width: 20
                    height: 20
                    radius: 10
                    color: "#FFFFFF"
                    border.width: zoomSlider.visualFocus ? 3 : 0
                    border.color: Theme.accent
                }
            }
            TfText { text: "+"; variant: "muted"; font.pixelSize: 20 }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            TfButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: dialog.close() }
            TfButton {
                text: qsTr("Use photo")
                variant: "primary"
                onClicked: {
                    const url = Hub.cropAvatar(-dialog.offsetX / dialog.zoomScale, -dialog.offsetY / dialog.zoomScale,
                                               dialog.viewport / dialog.zoomScale)
                    if (url !== "") {
                        dialog.close()
                        dialog.cropped(url)
                    }
                }
            }
        }
    }
}
