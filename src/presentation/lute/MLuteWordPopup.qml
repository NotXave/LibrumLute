import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * The Lute word popup: opens where Librum's selection popup would open and
 * shows the clicked word with Lute's seven statuses, the sentence context,
 * the translation and the contextual AI gloss.
 */
Popup {
    id: root

    property alias contentColumn: contentLayout

    padding: 16
    width: 470
    height: contentLayout.implicitHeight + 2 * padding
    background: Rectangle {
        color: Style.colorPopupBackground
        radius: 6
        border.width: 1
        border.color: Style.colorContainerBorder
    }

    function refreshFields() {
        translationField.text = LuteController.activeWord.translation
                ? LuteController.activeWord.translation : "";
        romanizationField.text = LuteController.activeWord.romanization
                ? LuteController.activeWord.romanization : "";
    }

    onOpened: refreshFields()

    Connections {
        target: LuteController

        function onWordPopupChanged() {
            if (root.opened)
                root.refreshFields();
        }
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        spacing: 0

        // ---- header -----------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    text: LuteController.activeWord.word
                          ? LuteController.activeWord.word.toUpperCase() : ""
                    color: Style.colorText
                    font.pointSize: Fonts.size20
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: LuteController.languageName
                          + (LuteController.activeWord.isPhrase
                             ? " · phrase (" + LuteController.activeWord.tokenCount
                               + " words)" : " · word")
                    color: Style.colorText
                    font.pointSize: Fonts.size11
                }
            }

            MButton {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                imagePath: Icons.closePopup
                imageSize: 12
                opacityOnPressed: 0.7

                onClicked: LuteController.closeWordPopup()
            }
        }

        // ---- tags -------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 6
            visible: LuteController.tags.length > 0

            Repeater {
                model: LuteController.tags

                delegate: Rectangle {
                    required property var modelData
                    readonly property bool selected:
                        LuteController.activeWord.tags !== undefined
                        && LuteController.activeWord.tags.indexOf(modelData.text) !== -1

                    Layout.preferredHeight: 22
                    Layout.preferredWidth: tagLabel.implicitWidth + 16
                    radius: 11
                    color: selected ? Style.colorBasePurple
                                    : Style.colorControlBackground
                    border.width: 1
                    border.color: selected ? Style.colorBasePurple
                                           : Style.colorContainerBorder
                    opacity: selected ? 1 : 0.75

                    Label {
                        id: tagLabel
                        anchors.centerIn: parent
                        text: modelData.text
                        color: selected ? Style.colorFocusedButtonText
                                        : Style.colorLightText
                        font.pointSize: Fonts.size9
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: LuteController.toggleActiveTag(modelData.id)
                    }
                }
            }
        }

        // ---- sentence context -------------------------------------------
        Pane {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.preferredHeight: sentenceLabel.implicitHeight + 22
            padding: 0
            background: Rectangle {
                color: Style.colorControlBackground
                border.width: 1
                border.color: Style.colorContainerBorder
                radius: 5
            }

            Label {
                id: sentenceLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 11
                text: LuteController.activeWord.sentence
                      ? "“" + LuteController.activeWord.sentence + "”" : ""
                color: Style.colorLightText
                font.pointSize: Fonts.size11
                font.italic: true
                wrapMode: Text.WordWrap
            }
        }

        // ---- status ladder ----------------------------------------------
        Label {
            Layout.topMargin: 14
            text: qsTr("STATUS")
            color: Style.colorLightText
            font.pointSize: Fonts.size10
            font.weight: Font.Bold
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 7
            spacing: 6

            Repeater {
                model: LuteController.statusLegend

                delegate: Rectangle {
                    required property var modelData

                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 32
                    radius: 4
                    color: modelData.color
                    opacity: LuteController.activeWord.status
                             === modelData.status ? 1.0 : 0.55
                    border.width: LuteController.activeWord.status
                                  === modelData.status ? 2 : 0
                    border.color: Style.colorBasePurple

                    Label {
                        anchors.centerIn: parent
                        text: modelData.abbrev
                        color: "#20242D"
                        font.pointSize: Fonts.size11
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: LuteController.setActiveStatus(modelData.status)
                    }
                }
            }
        }

        // ---- translation --------------------------------------------------
        Label {
            Layout.topMargin: 14
            text: qsTr("TRANSLATION")
            color: Style.colorLightText
            font.pointSize: Fonts.size10
            font.weight: Font.Bold
        }

        Pane {
            Layout.fillWidth: true
            Layout.topMargin: 7
            Layout.preferredHeight: 34
            padding: 0
            background: Rectangle {
                color: Style.colorControlBackground
                border.width: 1
                border.color: Style.colorContainerBorder
                radius: 5
            }

            TextField {
                id: translationField
                anchors.fill: parent
                leftPadding: 12
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                color: Style.colorBaseInputText
                placeholderText: qsTr("Add a translation…")
                placeholderTextColor: Style.colorPlaceholderText
                font.pointSize: Fonts.size11
                selectByMouse: true

                onAccepted: LuteController.setActiveTranslation(text)
                onEditingFinished: {
                    if (text !== LuteController.activeWord.translation)
                        LuteController.setActiveTranslation(text);
                }
            }
        }

        // ---- romanization --------------------------------------------------
        Label {
            Layout.topMargin: 14
            visible: LuteController.activeWord.showRomanization === true
            text: qsTr("ROMANIZATION")
            color: Style.colorLightText
            font.pointSize: Fonts.size10
            font.weight: Font.Bold
        }

        Pane {
            Layout.fillWidth: true
            Layout.topMargin: 7
            Layout.preferredHeight: 34
            visible: LuteController.activeWord.showRomanization === true
            padding: 0
            background: Rectangle {
                color: Style.colorControlBackground
                border.width: 1
                border.color: Style.colorContainerBorder
                radius: 5
            }

            TextField {
                id: romanizationField
                anchors.fill: parent
                leftPadding: 12
                verticalAlignment: TextInput.AlignVCenter
                background: Item {}
                color: Style.colorBaseInputText
                placeholderText: qsTr("pinyin / kana / reading")
                placeholderTextColor: Style.colorPlaceholderText
                font.pointSize: Fonts.size11
                selectByMouse: true

                onAccepted: LuteController.setActiveRomanization(text)
            }
        }

        // ---- AI --------------------------------------------------------------
        Pane {
            Layout.fillWidth: true
            Layout.topMargin: 14
            Layout.preferredHeight: aiLayout.implicitHeight + 22
            padding: 0
            background: Rectangle {
                color: Style.colorControlBackground
                border.width: 1
                border.color: Style.colorContainerBorder
                radius: 5
            }

            ColumnLayout {
                id: aiLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 11
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: qsTr("AI translation")
                        color: Style.colorText
                        font.pointSize: Fonts.size12
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: LuteController.aiBusy
                              ? qsTr("thinking…")
                              : LuteController.aiConfig.providerLabel + " · "
                                + LuteController.aiConfig.targetLanguage
                        color: Style.colorLightText
                        font.pointSize: Fonts.size10
                        elide: Text.ElideRight
                    }

                    MButton {
                        Layout.preferredHeight: 28
                        horizontalMargins: 10
                        borderWidth: 0
                        backgroundColor: Style.colorBasePurple
                        textColor: Style.colorFocusedButtonText
                        fontSize: Fonts.size10
                        text: LuteController.aiSuggestion.length === 0
                              ? qsTr("Ask AI") : qsTr("Ask again")
                        loading: LuteController.aiBusy

                        onClicked: LuteController.requestAiSuggestion()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: LuteController.aiSuggestion.length > 0
                    text: LuteController.aiSuggestion
                    color: Style.colorGreenText
                    font.pointSize: Fonts.size14
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: LuteController.aiError.length > 0
                    text: LuteController.aiError
                    color: Style.colorErrorText
                    font.pointSize: Fonts.size10
                    wrapMode: Text.Wrap
                }

                MButton {
                    Layout.preferredHeight: 28
                    horizontalMargins: 10
                    borderWidth: 0
                    backgroundColor: Style.colorBasePurple
                    textColor: Style.colorFocusedButtonText
                    fontSize: Fonts.size10
                    visible: LuteController.aiSuggestion.length > 0
                    text: qsTr("Use as definition")

                    onClicked: LuteController.applyAiSuggestion()
                }
            }
        }

        // ---- footer -----------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            spacing: 8

            MButton {
                Layout.preferredHeight: 28
                horizontalMargins: 10
                borderWidth: 1
                borderColor: Style.colorErrorBorder
                backgroundColor: "transparent"
                textColor: Style.colorErrorText
                fontSize: Fonts.size10
                visible: LuteController.activeWord.hasTerm === true
                text: qsTr("Delete term")

                onClicked: LuteController.deleteActiveTerm()
            }

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("1–5 · W · I")
                color: Style.colorLightText
                font.pointSize: Fonts.size9
            }
        }
    }

    Shortcut { enabled: root.opened; sequence: "1"; onActivated: LuteController.setActiveStatus(1) }
    Shortcut { enabled: root.opened; sequence: "2"; onActivated: LuteController.setActiveStatus(2) }
    Shortcut { enabled: root.opened; sequence: "3"; onActivated: LuteController.setActiveStatus(3) }
    Shortcut { enabled: root.opened; sequence: "4"; onActivated: LuteController.setActiveStatus(4) }
    Shortcut { enabled: root.opened; sequence: "5"; onActivated: LuteController.setActiveStatus(5) }
    Shortcut { enabled: root.opened; sequence: "W"; onActivated: LuteController.setActiveStatus(99) }
    Shortcut { enabled: root.opened; sequence: "I"; onActivated: LuteController.setActiveStatus(98) }
    Shortcut { enabled: root.opened; sequence: "Escape"; onActivated: LuteController.closeWordPopup() }
}
