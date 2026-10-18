import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: the Lute text reader. Renders a Lute page as rich text with
 * one link per word; the word colour is its Lute status. Clicking a word opens
 * the Lute popup, exactly like clicking a word in a PDF.
 */
Page {
    id: root
    property point lastClick: Qt.point(700, 300)

    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Pane {
            Layout.fillWidth: true
            implicitHeight: 50
            padding: 0
            background: Rectangle {
                color: Style.colorReadingToolbarBackground
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                MButton {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 32
                    backgroundColor: Style.colorReadingViewButtonBackground
                    borderWidth: 0
                    imagePath: Icons.readingViewBack
                    imageSize: 11
                    opacityOnPressed: 0.7

                    onClicked: {
                        LuteController.closeBook()
                        loadPage(luteHubPage, sidebar.luteItem, false)
                    }
                }

                Label {
                    Layout.leftMargin: 6
                    text: LuteController.readerTitle
                    color: Style.colorTitle
                    font.pointSize: Fonts.size13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.maximumWidth: 520
                }

                Slider {
                    id: pageSlider
                    Layout.preferredWidth: 200
                    Layout.leftMargin: 12
                    from: 1
                    to: Math.max(1, LuteController.readerPageCount)
                    value: LuteController.readerPage
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    enabled: LuteController.readerPageCount > 1
                    onMoved: {
                        const target = Math.round(value)
                        if (target !== LuteController.readerPage)
                            LuteController.openPage(target)
                    }

                    background: Rectangle {
                        x: pageSlider.leftPadding
                        y: pageSlider.topPadding
                           + pageSlider.availableHeight / 2 - height / 2
                        implicitHeight: 4
                        width: pageSlider.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: Style.colorContainerBorder

                        Rectangle {
                            width: pageSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Style.colorBasePurple
                        }
                    }

                    handle: Rectangle {
                        x: pageSlider.leftPadding
                           + pageSlider.visualPosition
                             * (pageSlider.availableWidth - width)
                        y: pageSlider.topPadding
                           + pageSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: pageSlider.enabled ? Style.colorBasePurple
                                                  : Style.colorContainerBorder
                    }
                }

                TextField {
                    id: pageInput
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 28
                    leftPadding: 8
                    horizontalAlignment: TextInput.AlignHCenter
                    background: Rectangle {
                        color: Style.colorReadingViewButtonBackground
                        radius: 4
                    }
                    color: Style.colorText
                    font.pointSize: Fonts.size10
                    placeholderText: qsTr("#")
                    placeholderTextColor: Style.colorLightText
                    validator: IntValidator {
                        bottom: 1
                        top: Math.max(1, LuteController.readerPageCount)
                    }
                    onAccepted: {
                        if (text.length > 0)
                            LuteController.openPage(parseInt(text))
                        text = ""
                    }
                }

                MButton {
                    Layout.preferredHeight: 28
                    horizontalMargins: 10
                    borderWidth: 1
                    borderColor: Style.colorButtonBorder
                    backgroundColor: "transparent"
                    textColor: Style.colorText
                    fontSize: Fonts.size9
                    text: qsTr("Go")

                    onClicked: {
                        if (pageInput.text.length > 0)
                            LuteController.openPage(parseInt(pageInput.text))
                        pageInput.text = ""
                    }
                }

                MButton {
                    Layout.preferredHeight: 28
                    horizontalMargins: 10
                    borderWidth: 1
                    borderColor: Style.colorButtonBorder
                    backgroundColor: "transparent"
                    readonly property bool marked:
                        LuteController.bookmarks.some(
                            bookmark => bookmark.page === LuteController.readerPage)
                    textColor: marked ? Style.colorBasePurple : Style.colorText
                    fontSize: Fonts.size10
                    text: marked ? qsTr("★ Bookmarked") : qsTr("☆ Bookmark")

                    onClicked: LuteController.toggleBookmark(
                                   LuteController.readerPage)
                }

                ComboBox {
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 28
                    visible: LuteController.bookmarks.length > 0
                    model: LuteController.bookmarks
                    textRole: "label"
                    font.pointSize: Fonts.size9

                    onActivated: index => {
                                     LuteController.openPage(
                                         LuteController.bookmarks[index].page)
                                     currentIndex = -1
                                 }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: qsTr("Page %1 of %2")
                          .arg(LuteController.readerPage)
                          .arg(LuteController.readerPageCount)
                    color: Style.colorLightText
                    font.pointSize: Fonts.size10
                }

                MButton {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 32
                    backgroundColor: Style.colorReadingViewButtonBackground
                    borderWidth: 0
                    text: qsTr("‹")
                    textColor: Style.colorText
                    fontSize: Fonts.size16
                    enabled: LuteController.readerPage > 1
                    opacityOnPressed: 0.7

                    onClicked: LuteController.previousPage()
                }

                MButton {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 32
                    backgroundColor: Style.colorReadingViewButtonBackground
                    borderWidth: 0
                    text: qsTr("›")
                    textColor: Style.colorText
                    fontSize: Fonts.size16
                    enabled: LuteController.readerPage < LuteController.readerPageCount
                    opacityOnPressed: 0.7

                    onClicked: LuteController.nextPage()
                }

                MButton {
                    Layout.preferredHeight: 32
                    horizontalMargins: 12
                    borderWidth: 1
                    borderColor: Style.colorButtonBorder
                    backgroundColor: "transparent"
                    textColor: LuteController.readerPageRead
                                ? Style.colorGreenText : Style.colorText
                    fontSize: Fonts.size10
                    text: LuteController.readerPageRead
                          ? qsTr("Page read") : qsTr("Mark page read")

                    onClicked: LuteController.markPageRead()
                }
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: paper.height + 48
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Pane {
                id: paper
                width: Math.min(880,
                                parent.width - 96
                                - (wordPopup.docked && wordPopup.opened
                                   ? wordPopup.width : 0))
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: (wordPopup.docked && wordPopup.opened)
                    ? (LuteController.readerPanelSide === "right"
                       ? -wordPopup.width / 2 : wordPopup.width / 2)
                    : 0
                y: 24
                padding: 44
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                Text {
                    id: pageText
                    width: paper.width - 88
                    text: LuteController.readerHtml
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    color: Style.colorText
                    font.pointSize: Fonts.size13
                    font.family: "Times New Roman"
                    lineHeight: 1.55

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const link = pageText.linkAt(mouse.x, mouse.y)
                            if (!link || link.indexOf("w:") !== 0) {
                                wordPopup.close()
                                return
                            }
                            root.lastClick = mapToItem(root, mouse.x, mouse.y)
                            LuteController.selectTextWord(parseInt(link.substring(2)))
                        }
                    }
                }
            }
        }
    }

    MLuteWordPopup {
        id: wordPopup
    }

    Connections {
        target: LuteController

        function onReaderChanged() {
            if (!LuteController.wordPopupVisible)
                wordPopup.close()
        }

        function onWordPopupChanged() {
            if (!LuteController.wordPopupVisible) {
                wordPopup.close()
                return
            }
            if (wordPopup.docked) {
                wordPopup.x = LuteController.readerPanelSide === "right"
                        ? root.width - wordPopup.width : 0
                wordPopup.y = 0
                wordPopup.open()
                return
            }

            const x = Math.max(20, Math.min(
                                   root.width - wordPopup.width - 20,
                                   root.lastClick.x - wordPopup.width / 2))
            const y = Math.max(60, Math.min(root.height - 420,
                                            root.lastClick.y + 18))
            wordPopup.x = x
            wordPopup.y = y
            wordPopup.open()
        }
    }

    Shortcut {
        sequence: "Left"
        onActivated: LuteController.previousPage()
    }

    Shortcut {
        sequence: "Right"
        onActivated: LuteController.nextPage()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: wordPopup.close()
    }

    Shortcut {
        sequence: "b"
        onActivated: LuteController.toggleBookmark(LuteController.readerPage)
    }

    Shortcut {
        sequence: "m"
        onActivated: LuteController.markPageRead()
    }
}
