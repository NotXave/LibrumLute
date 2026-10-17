import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: the Lute book library. Shows the books of the selected
 * language that already live in lute.db (the user's own Lute library) and
 * opens them in the Lute text reader.
 */
Page {
    id: root
    horizontalPadding: 44
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    property string filterText: ""
    property var filteredBooks: []

    function rebuildFilter() {
        const all = LuteController.books;
        if (filterText.length === 0) {
            filteredBooks = all;
            return;
        }
        const needle = filterText.toLowerCase();
        let result = [];
        for (let i = 0; i < all.length; ++i) {
            if (all[i].title.toLowerCase().indexOf(needle) !== -1)
                result.push(all[i]);
        }
        filteredBooks = result;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            MTitle {
                Layout.topMargin: 40
                titleText: qsTr("Lute library")
                descriptionText: qsTr("%1 books in %2").arg(LuteController.books.length).arg(LuteController.languageName)
                titleSize: Fonts.size25
                descriptionSize: Fonts.size13dot25
            }

            Item { Layout.fillWidth: true }

            MButton {
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignBottom
                horizontalMargins: 14
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size11
                text: qsTr("Import .txt")

                onClicked: importTextDialog.open()
            }

            MButton {
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignBottom
                horizontalMargins: 14
                borderWidth: 0
                backgroundColor: Style.colorBasePurple
                textColor: Style.colorFocusedButtonText
                fontSize: Fonts.size11
                fontWeight: Font.Bold
                text: qsTr("New text")

                onClicked: newTextPopup.open()
            }

            MButton {
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignBottom
                horizontalMargins: 14
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size11
                text: qsTr("Add a book")

                onClicked: addPdfDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 22
            spacing: 12

            Pane {
                Layout.preferredWidth: 380
                Layout.preferredHeight: 34
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }

                TextField {
                    anchors.fill: parent
                    leftPadding: 12
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    color: Style.colorBaseInputText
                    placeholderText: qsTr("Search books…")
                    placeholderTextColor: Style.colorPlaceholderText
                    font.pointSize: Fonts.size11
                    selectByMouse: true
                    onTextChanged: {
                        root.filterText = text
                        root.rebuildFilter()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            MButton {
                Layout.preferredHeight: 34
                horizontalMargins: 12
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size10
                text: qsTr("Refresh")

                onClicked: LuteController.refreshLibrary()
            }
        }

        ListView {
            id: bookList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 20
            clip: true
            spacing: 10
            model: root.filteredBooks

            delegate: Pane {
                id: bookRow
                required property var modelData

                width: bookList.width
                height: 76
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 16

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 44
                        radius: 3
                        color: Style.colorControlBackground

                        Label {
                            anchors.centerIn: parent
                            text: "Aa"
                            color: Style.colorLightText
                            font.pointSize: Fonts.size13
                            font.weight: Font.DemiBold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: bookRow.modelData.title
                            color: Style.colorText
                            font.pointSize: Fonts.size13
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 pages · page %2%3")
                                  .arg(bookRow.modelData.pages)
                                  .arg(bookRow.modelData.currentPage)
                                  .arg(bookRow.modelData.archived ? qsTr(" · archived") : "")
                            color: Style.colorPageSubtitle
                            font.pointSize: Fonts.size10
                            elide: Text.ElideRight
                        }
                    }

                    MButton {
                        Layout.preferredHeight: 32
                        horizontalMargins: 14
                        borderWidth: 1
                        borderColor: Style.colorButtonBorder
                        backgroundColor: "transparent"
                        textColor: Style.colorText
                        fontSize: Fonts.size11
                        text: qsTr("Read")

                        onClicked: root.openBook(bookRow.modelData.id)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openBook(bookRow.modelData.id)
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            visible: LuteController.books.length === 0
            text: qsTr("No books in this Lute database yet. Add a PDF to get started.")
            color: Style.colorPageSubtitle
            font.pointSize: Fonts.size11
        }
    }

    function openBook(bookId) {
        if (LuteController.openBook(bookId))
            LuteController.showReader();
    }

    Popup {
        id: newTextPopup
        width: 580
        height: 470
        x: Math.round((root.width - width) / 2)
        y: 70
        modal: true
        dim: true
        padding: 22
        background: Rectangle {
            color: Style.colorPopupBackground
            radius: 6
            border.width: 1
            border.color: Style.colorContainerBorder
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: qsTr("New Lute text")
                color: Style.colorText
                font.pointSize: Fonts.size17
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The text is split into Lute pages of about 250 words; a line with --- starts a new page.")
                color: Style.colorPageSubtitle
                font.pointSize: Fonts.size10
                wrapMode: Text.WordWrap
            }

            MLabeledInputBox {
                id: titleInput
                Layout.fillWidth: true
                Layout.topMargin: 4
                headerText: qsTr("Title")
                placeholderContent: qsTr("My text")
            }

            Pane {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                padding: 0
                background: Rectangle {
                    color: Style.colorControlBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true

                    TextArea {
                        id: textInput
                        placeholderText: qsTr("Paste the text here…")
                        placeholderTextColor: Style.colorPlaceholderText
                        color: Style.colorBaseInputText
                        font.pointSize: Fonts.size11
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        background: Item {}
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MCheckBox {
                    id: splitSentences
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }

                Label {
                    text: qsTr("Split at sentence boundaries")
                    color: Style.colorText
                    font.pointSize: Fonts.size10
                }

                Item { Layout.fillWidth: true }

                MButton {
                    Layout.preferredHeight: 32
                    horizontalMargins: 14
                    borderWidth: 1
                    borderColor: Style.colorButtonBorder
                    backgroundColor: "transparent"
                    textColor: Style.colorText
                    fontSize: Fonts.size11
                    text: qsTr("Cancel")

                    onClicked: newTextPopup.close()
                }

                MButton {
                    Layout.preferredHeight: 32
                    horizontalMargins: 14
                    borderWidth: 0
                    backgroundColor: Style.colorBasePurple
                    textColor: Style.colorFocusedButtonText
                    fontSize: Fonts.size11
                    text: qsTr("Create")

                    onClicked: {
                        if (LuteController.createBook(titleInput.text,
                                                      textInput.text,
                                                      splitSentences.checked)) {
                            newTextPopup.close()
                            titleInput.text = ""
                            textInput.text = ""
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: importTextDialog
        title: qsTr("Import a text file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]

        onAccepted: LuteController.importTextFile(
                        root.toLocalPath(selectedFile))
    }

    FileDialog {
        id: addPdfDialog
        title: qsTr("Add a PDF to the Lute library")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Books (*.pdf *.epub *.mobi *.fb2 *.cbz)"), qsTr("All files (*)")]
        onAccepted: LuteController.addPdfToLuteLibrary(
                        root.toLocalPath(selectedFile))
    }

    function toLocalPath(url) {
        let path = decodeURIComponent(url.toString());
        if (path.startsWith("file:///"))
            path = path.substring(8);
        else if (path.startsWith("file://"))
            path = path.substring(7);
        return path;
    }

    Component.onCompleted: {
        LuteController.refreshLibrary()
        root.rebuildFilter()
    }

    Connections {
        target: LuteController

        function onBooksChanged() {
            root.rebuildFilter()
        }

        function onLanguageChanged() {
            root.rebuildFilter()
        }
    }
}
