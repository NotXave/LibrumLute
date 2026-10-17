import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: Lute language management. Lists the languages of the open
 * lute.db and edits all parser fields Lute stores for them.
 */
Page {
    id: root
    horizontalPadding: 44
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    property int editingId: 0

    ListModel {
        id: parserModel
        ListElement { label: "Space delimited"; value: "spacedel" }
        ListElement { label: "Space or mark"; value: "spaceormark" }
        ListElement { label: "Generic"; value: "generic" }
    }

    function openNew() {
        root.editingId = 0
        nameInput.text = ""
        substitutionsInput.text = ""
        splitInput.text = ".!?"
        exceptionsInput.text = ""
        wordCharsInput.text = "a-zA-Z"
        rtlCheck.checked = false
        romanizationCheck.checked = false
        parserCombo.defaultIndex = 0
        parserCombo.text = "Space delimited"
        languagePopup.open()
    }

    function openEdit(language) {
        root.editingId = language.id
        nameInput.text = language.name
        substitutionsInput.text = language.characterSubstitutions
        splitInput.text = language.regexpSplitSentences
        exceptionsInput.text = language.exceptionsSplitSentences
        wordCharsInput.text = language.regexpWordCharacters
        rtlCheck.checked = language.rightToLeft
        romanizationCheck.checked = language.showRomanization
        let index = 0
        for (let i = 0; i < parserModel.count; ++i) {
            if (parserModel.get(i).value === language.parserType)
                index = i
        }
        parserCombo.defaultIndex = index
        parserCombo.text = parserModel.get(index).label
        languagePopup.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            MTitle {
                Layout.topMargin: 40
                titleText: qsTr("Languages")
                descriptionText: qsTr("%1 languages · parser rules stored in lute.db")
                                 .arg(LuteController.languages.length)
                titleSize: Fonts.size25
                descriptionSize: Fonts.size13dot25
            }

            Item { Layout.fillWidth: true }

            MButton {
                Layout.preferredHeight: 34
                Layout.alignment: Qt.AlignBottom
                horizontalMargins: 14
                borderWidth: 0
                backgroundColor: Style.colorBasePurple
                textColor: Style.colorFocusedButtonText
                fontSize: Fonts.size11
                text: qsTr("New language")

                onClicked: root.openNew()
            }
        }

        ListView {
            id: languageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 20
            clip: true
            spacing: 6
            model: LuteController.languages

            delegate: Pane {
                id: languageRow
                required property var modelData

                width: languageList.width
                height: 54
                padding: 0
                background: Rectangle {
                    color: Style.colorContainerBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: languageRow.modelData.name
                        color: Style.colorText
                        font.pointSize: Fonts.size13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Label {
                        text: languageRow.modelData.parserType
                        color: Style.colorPageSubtitle
                        font.pointSize: Fonts.size10
                    }

                    Label {
                        visible: languageRow.modelData.rightToLeft
                        text: "RTL"
                        color: Style.colorBasePurple
                        font.pointSize: Fonts.size9
                    }

                    Label {
                        visible: languageRow.modelData.showRomanization
                        text: qsTr("Romanization")
                        color: Style.colorBasePurple
                        font.pointSize: Fonts.size9
                    }

                    Label {
                        visible: languageRow.modelData.id === LuteController.languageId
                        text: qsTr("current")
                        color: Style.colorGreenText
                        font.pointSize: Fonts.size9
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openEdit(languageRow.modelData)
                }
            }
        }
    }

    Popup {
        id: languagePopup
        width: 620
        height: 600
        x: Math.round((root.width - width) / 2)
        y: 40
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
            spacing: 8

            Label {
                text: root.editingId > 0 ? qsTr("Edit language") : qsTr("New language")
                color: Style.colorText
                font.pointSize: Fonts.size17
                font.weight: Font.DemiBold
            }

            MLabeledInputBox {
                id: nameInput
                Layout.fillWidth: true
                headerText: qsTr("Name")
                placeholderContent: qsTr("Polish")
            }

            MComboBox {
                id: parserCombo
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                itemHeight: 30
                fontSize: Fonts.size11
                selectedItemFontSize: Fonts.size11
                contentPropertyName: "label"
                model: parserModel
                defaultIndex: 0
                Component.onCompleted: parserCombo.text = "Space delimited"
            }

            MLabeledInputBox {
                id: substitutionsInput
                Layout.fillWidth: true
                headerText: qsTr("Character substitutions")
                placeholderContent: "’='|—=–"
            }

            MLabeledInputBox {
                id: splitInput
                Layout.fillWidth: true
                headerText: qsTr("Split sentences at")
                placeholderContent: ".!?"
            }

            MLabeledInputBox {
                id: exceptionsInput
                Layout.fillWidth: true
                headerText: qsTr("Split sentence exceptions")
                placeholderContent: "Mr.|Dr."
            }

            MLabeledInputBox {
                id: wordCharsInput
                Layout.fillWidth: true
                headerText: qsTr("Word characters (regexp class)")
                placeholderContent: "a-zA-Z"
            }

            RowLayout {
                spacing: 24

                RowLayout {
                    spacing: 6
                    MCheckBox {
                        id: rtlCheck
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                    }
                    Label {
                        text: qsTr("Right to left")
                        color: Style.colorText
                        font.pointSize: Fonts.size10
                    }
                }

                RowLayout {
                    spacing: 6
                    MCheckBox {
                        id: romanizationCheck
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                    }
                    Label {
                        text: qsTr("Show romanization")
                        color: Style.colorText
                        font.pointSize: Fonts.size10
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: LuteController.lastMessage.length > 0
                text: LuteController.lastMessage
                color: LuteController.lastMessageIsError
                       ? Style.colorErrorText : Style.colorGreenText
                font.pointSize: Fonts.size10
                wrapMode: Text.WordWrap
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MButton {
                    Layout.preferredHeight: 32
                    horizontalMargins: 12
                    borderWidth: 1
                    borderColor: Style.colorErrorBorder
                    backgroundColor: "transparent"
                    textColor: Style.colorErrorText
                    fontSize: Fonts.size10
                    visible: root.editingId > 0
                    text: qsTr("Delete")

                    onClicked: {
                        if (LuteController.deleteLanguage(root.editingId))
                            languagePopup.close()
                    }
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

                    onClicked: languagePopup.close()
                }

                MButton {
                    Layout.preferredHeight: 32
                    horizontalMargins: 14
                    borderWidth: 0
                    backgroundColor: Style.colorBasePurple
                    textColor: Style.colorFocusedButtonText
                    fontSize: Fonts.size11
                    text: qsTr("Save")

                    onClicked: {
                        const parser = parserModel.get(parserCombo.defaultIndex)
                        const payload = {
                            "id": root.editingId,
                            "name": nameInput.text,
                            "characterSubstitutions": substitutionsInput.text,
                            "regexpSplitSentences": splitInput.text,
                            "exceptionsSplitSentences": exceptionsInput.text,
                            "regexpWordCharacters": wordCharsInput.text,
                            "rightToLeft": rtlCheck.checked,
                            "showRomanization": romanizationCheck.checked,
                            "parserType": parser ? parser.value : "spacedel"
                        }
                        if (LuteController.saveLanguage(payload))
                            languagePopup.close()
                    }
                }
            }
        }
    }
}
