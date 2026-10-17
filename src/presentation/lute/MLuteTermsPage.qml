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
 * Fork addition: the Lute term list. Search and status filter over the real
 * `words` table; clicking a term opens the Lute popup for editing.
 */
Page {
    id: root
    horizontalPadding: 44
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    property string searchText: ""
    property int statusFilter: 0
    property point popupPoint: Qt.point(700, 260)

    function reload() {
        LuteController.refreshTerms(searchText, statusFilter);
    }

    ListModel {
        id: statusModel
        ListElement { label: "All statuses"; value: 0 }
        ListElement { label: "New (1)"; value: 1 }
        ListElement { label: "New (2)"; value: 2 }
        ListElement { label: "Learning (3)"; value: 3 }
        ListElement { label: "Learning (4)"; value: 4 }
        ListElement { label: "Learned (5)"; value: 5 }
        ListElement { label: "Well known"; value: 99 }
        ListElement { label: "Ignored"; value: 98 }
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
                titleText: qsTr("Terms")
                descriptionText: qsTr("%1 terms in %2 (showing %3)")
                                 .arg(LuteController.stats.totalTerms)
                                 .arg(LuteController.languageName)
                                 .arg(LuteController.terms.length)
                titleSize: Fonts.size25
                descriptionSize: Fonts.size13dot25
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            spacing: 10

            Pane {
                Layout.preferredWidth: 340
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
                    placeholderText: qsTr("Search terms…")
                    placeholderTextColor: Style.colorPlaceholderText
                    font.pointSize: Fonts.size11
                    selectByMouse: true
                    onTextChanged: {
                        root.searchText = text
                        root.reload()
                    }
                }
            }

            MComboBox {
                id: statusCombo
                Layout.preferredWidth: 190
                Layout.preferredHeight: 34
                itemHeight: 30
                fontSize: Fonts.size11
                selectedItemFontSize: Fonts.size11
                contentPropertyName: "label"
                model: statusModel
                defaultIndex: 0
                Component.onCompleted: statusCombo.text = qsTr("All statuses")

                onItemChanged: index => {
                                   if (index >= 0 && index < statusModel.count) {
                                       root.statusFilter = statusModel.get(index).value
                                       root.reload()
                                   }
                                   closePopup()
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
                text: qsTr("Import TSV")

                onClicked: importDialog.open()
            }

            MButton {
                Layout.preferredHeight: 34
                horizontalMargins: 12
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size10
                text: qsTr("Export TSV")

                onClicked: exportDialog.open()
            }

            MButton {
                Layout.preferredHeight: 34
                horizontalMargins: 12
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size10
                text: qsTr("Export Anki")

                onClicked: ankiDialog.open()
            }

            MButton {
                Layout.preferredHeight: 34
                horizontalMargins: 12
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                fontSize: Fonts.size10
                text: qsTr("Refresh")

                onClicked: root.reload()
            }
        }

        ListView {
            id: termList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 16
            clip: true
            spacing: 6
            model: LuteController.terms

            delegate: Pane {
                id: termRow
                required property var modelData

                width: termList.width
                height: 52
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 26
                        radius: 4
                        color: termRow.modelData.color

                        Label {
                            anchors.centerIn: parent
                            text: termRow.modelData.statusAbbrev
                            color: "#20242D"
                            font.pointSize: Fonts.size10
                            font.weight: Font.Bold
                        }
                    }

                    Label {
                        Layout.preferredWidth: 220
                        text: termRow.modelData.word
                        color: Style.colorText
                        font.pointSize: Fonts.size12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: termRow.modelData.translation.length > 0
                              ? termRow.modelData.translation
                              : qsTr("No translation")
                        color: termRow.modelData.translation.length > 0
                               ? Style.colorPageSubtitle : Style.colorLightText
                        font.pointSize: Fonts.size11
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: termRow.modelData.isPhrase
                        text: qsTr("phrase")
                        color: Style.colorBasePurple
                        font.pointSize: Fonts.size9
                    }

                    MButton {
                        Layout.preferredHeight: 28
                        horizontalMargins: 10
                        borderWidth: 1
                        borderColor: Style.colorErrorBorder
                        backgroundColor: "transparent"
                        textColor: Style.colorErrorText
                        fontSize: Fonts.size9
                        text: qsTr("Delete")

                        onClicked: LuteController.deleteTermById(
                                       termRow.modelData.id)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.popupPoint = mapToItem(root, mouse.x, mouse.y)
                        LuteController.selectTerm(termRow.modelData.id)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: LuteController.terms.length === 0
                text: qsTr("No terms for this filter yet.")
                color: Style.colorPageSubtitle
                font.pointSize: Fonts.size11
            }
        }
    }

    FileDialog {
        id: ankiDialog
        title: qsTr("Export terms for Anki")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: [qsTr("TSV files (*.txt *.tsv)"), qsTr("All files (*)")]

        onAccepted: LuteController.exportAnki(
                        root.toLocalPath(selectedFile))
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import terms from TSV")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("TSV files (*.tsv *.txt)"), qsTr("All files (*)")]

        onAccepted: LuteController.importTerms(
                        root.toLocalPath(selectedFile))
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export terms as TSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "tsv"
        nameFilters: [qsTr("TSV files (*.tsv)"), qsTr("All files (*)")]

        onAccepted: LuteController.exportTerms(
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

    MLuteWordPopup {
        id: termPopup
    }

    Connections {
        target: LuteController

        function onShowTermsRequested() {
            root.reload()
        }

        function onWordPopupChanged() {
            if (!LuteController.wordPopupVisible) {
                termPopup.close()
                return
            }
            termPopup.x = Math.max(20, Math.min(
                                       root.width - termPopup.width - 20,
                                       root.popupPoint.x - termPopup.width / 2))
            termPopup.y = Math.max(60, Math.min(root.height - 420,
                                                root.popupPoint.y + 14))
            termPopup.open()
        }
    }

    Component.onCompleted: reload()
}
