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
                borderWidth: 0
                backgroundColor: Style.colorBasePurple
                textColor: Style.colorFocusedButtonText
                fontSize: Fonts.size11
                fontWeight: Font.Bold
                text: qsTr("Add a PDF")

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

    FileDialog {
        id: addPdfDialog
        title: qsTr("Add a PDF to the Lute library")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("PDF documents (*.pdf)"), qsTr("All files (*)")]
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
