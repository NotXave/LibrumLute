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

                if (mark.parts !== undefined && mark.parts.length > 0) {
                    for (let p = 0; p < mark.parts.length; ++p) {
                        const part = mark.parts[p];
                        drawMark(ctx, part.x * width, part.y * height,
                                 part.w * width, part.h * height, mark, true);
                    }
                    continue;
                }

                drawMark(ctx, mark.x * width, mark.y * height,
                         mark.w * width, mark.h * height, mark,
                         mark.kind === "phrase");
            }
            ctx.globalAlpha = 1.0;
        }

        function drawMark(ctx, x, y, w, h, mark, isPhrase) {
            if (w <= 0 || h <= 0)
                return;

            const known = mark.status === 99 || mark.status === 98;
            ctx.globalAlpha = known ? 0.30 : 0.42;
            ctx.fillStyle = mark.color;
            ctx.fillRect(x, y, w, h);

            ctx.globalAlpha = 1.0;
            ctx.fillStyle = mark.color;
            const lineHeight = isPhrase ? 3 : 2;
            ctx.fillRect(x, y + h - lineHeight, w, lineHeight);

            if (isPhrase) {
                ctx.globalAlpha = 0.85;
                ctx.strokeStyle = mark.color;
                ctx.lineWidth = 1;
                ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1),
                               Math.max(0, h - 1));
            }
        }
    }
}
