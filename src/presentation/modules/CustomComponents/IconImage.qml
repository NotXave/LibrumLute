import QtQuick
import QtQuick.Effects

/***
 * Fork addition: Librum's sources use `IconImage` in several places but the
 * component itself is not part of the repository, so the whole alert/popup
 * stack failed to load ("IconImage is not a type"). This is the obvious
 * implementation: an Image that can tint an SVG icon with `color`.
 */
Item {
    id: root

    property url source
    property color color: "transparent"
    property int fillMode: Image.PreserveAspectFit
    property size sourceSize: Qt.size(-1, -1)
    property bool smooth: true
    property bool mipmap: false

    implicitWidth: image.implicitWidth
    implicitHeight: image.implicitHeight

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        fillMode: root.fillMode
        sourceSize: root.sourceSize
        smooth: root.smooth
        mipmap: root.mipmap

        layer.enabled: root.color.a > 0.0
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: root.color
        }
    }
}
