import QtQuick
import Librum.lute 1.0

/***
 * Draws Lute's coloured underlines on top of one rendered page.
 *
 * The marks come from LuteController.highlightsForPage(pageNumber), which
 * returns normalised rectangles plus the status colour, so the overlay follows
 * the page's zoom automatically.
 */
Item {
    id: root

    property int pageNumber: 0
    property var marks: []
    // Only pages near the viewport extract words; otherwise an EPUB with
    // hundreds of pages would run hundreds of text extractions at once.
    property bool active: true

    visible: LuteController.documentOpen && active
    enabled: false

    function reload() {
        if(!root.active)
            return;
        marks = LuteController.highlightsForPage(root.pageNumber);
    }

    Component.onCompleted: if (root.active) reload()
    onActiveChanged: if (root.active) reload()
    onVisibleChanged: if (visible) reload()
    onMarksChanged: canvas.requestPaint()

    Connections {
        target: LuteController

        function onHighlightsChanged() {
            root.reload();
        }

        function onDocumentChanged() {
            root.reload();
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            for (let i = 0; i < root.marks.length; ++i) {
                const mark = root.marks[i];
                const isPhrase = mark.kind === "phrase";
                const parts = (mark.parts !== undefined && mark.parts.length > 0)
                              ? mark.parts : [mark];

                for (let p = 0; p < parts.length; ++p) {
                    const part = parts[p];
                    if (part === undefined || part.w === undefined)
                        continue;

                    const x = part.x * width + 0.5;
                    const w = part.w * width - 1.0;
                    if (w <= 0.5)
                        continue;

                    const bottom = (part.y + part.h) * height;
                    const thickness = isPhrase ? 2.5 : 2.0;
                    const known = mark.status === 99 || mark.status === 98;

                    ctx.globalAlpha = known ? 0.65 : 0.9;
                    ctx.fillStyle = mark.color;
                    ctx.fillRect(x, bottom - thickness, w, thickness);
                }
            }
            ctx.globalAlpha = 1.0;
        }
    }
}
