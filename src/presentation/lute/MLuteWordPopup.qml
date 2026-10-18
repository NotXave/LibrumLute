import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * The Lute term panel: a compact editor for the clicked word. Doubles as the
 * docked side panel and the floating popup; shows statuses, the sentence,
 * switchable translation sources (Reverso/Google/AI/MyMemory), manual
 * translation, tags and the word family.
 */
Popup {
    id: root

    property alias contentColumn: contentLayout

    readonly property bool docked: LuteController.readerPresentation === "panel"
    readonly property bool dockRight: LuteController.readerPanelSide === "right"
    readonly property bool hasResult: LuteController.translationResult.translation !== undefined
                                     && LuteController.translationResult.translation.length > 0

    padding: 12
    width: docked ? 380 : 470
    height: docked ? (parent ? parent.height : 700)
                   : flick.implicitHeight + 2 * padding
    modal: false
    dim: false
    closePolicy: docked ? Popup.NoAutoClose
                        : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
    background: Rectangle {
        color: Style.colorPopupBackground
        radius: docked ? 0 : 6
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

    // Hidden helper used to place translations on the clipboard.
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: contentLayout.implicitHeight
        implicitHeight: Math.min(contentHeight, root.docked ? height : contentHeight)

        ScrollBar.vertical: ScrollBar {
            policy: flick.contentHeight > flick.height
                    ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

        ColumnLayout {
            id: contentLayout
            width: flick.width
            spacing: 8

            // ---- header (44 px) -------------------------------------------
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
                        color: Style.colorLightText
                        font.pointSize: Fonts.size10
                    }
                }

                MButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    borderWidth: 1
                    borderColor: Style.colorButtonBorder
                    imagePath: Icons.closePopup
                    imageSize: 12
                    opacityOnPressed: 0.7

                    onClicked: LuteController.closeWordPopup()
                }
            }

            // ---- sentence: italic, max 2 lines, no box ---------------------
            Label {
                Layout.fillWidth: true
                text: LuteController.activeWord.sentence
                      ? "“" + LuteController.activeWord.sentence + "”" : ""
                color: Style.colorLightText
                font.pointSize: Fonts.size11
                font.italic: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            // ---- status chips ----------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: LuteController.statusLegend

                    delegate: Rectangle {
                        required property var modelData

                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 26
                        radius: 5
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
                            font.pointSize: Fonts.size9
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: LuteController.setActiveStatus(modelData.status)
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ---- translation sources ---------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Repeater {
                    model: LuteController.translationProviders

                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool active:
                            LuteController.translationProvider === modelData.key

                        Layout.preferredHeight: 26
                        Layout.preferredWidth: providerLabel.implicitWidth + 20
                        radius: 13
                        color: active ? Style.colorBasePurple : "transparent"
                        border.width: 1
                        border.color: active ? Style.colorBasePurple
                                              : Style.colorContainerBorder

                        Label {
                            id: providerLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            color: active ? Style.colorFocusedButtonText
                                          : Style.colorLightText
                            font.pointSize: Fonts.size9
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: LuteController.setTranslationProvider(modelData.key)
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ---- result area -----------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(54, resultColumn.implicitHeight + 16)
                radius: 5
                color: Style.colorControlBackground
                border.width: 1
                border.color: Style.colorContainerBorder

                ColumnLayout {
                    id: resultColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 4

                    // Loading: two grey skeleton bars instead of a spinner.
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: LuteController.translationBusy && !root.hasResult
                        spacing: 6

                        Rectangle {
                            Layout.preferredWidth: 150
                            Layout.preferredHeight: 10
                            radius: 5
                            color: Style.colorContainerBorder
                        }
                        Rectangle {
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 10
                            radius: 5
                            color: Style.colorContainerBorder
                        }
                    }

                    // Primary translation (16 pt DemiBold).
                    Label {
                        Layout.fillWidth: true
                        visible: !LuteController.translationBusy || root.hasResult
                        text: root.hasResult
                              ? LuteController.translationResult.translation : ""
                        color: Style.colorText
                        font.pointSize: Fonts.size16
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        verticalAlignment: Text.AlignTop
                    }

                    // Source attribution (9 pt).
                    Label {
                        Layout.fillWidth: true
                        visible: root.hasResult
                        text: LuteController.translationResult.label !== undefined
                              ? LuteController.translationResult.label : ""
                        color: Style.colorLightText
                        font.pointSize: Fonts.size9
                    }

                    // Reverso context examples (max 3 pairs).
                    Repeater {
                        model: LuteController.translationResult.examples !== undefined
                               ? LuteController.translationResult.examples : []

                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 1

                            Label {
                                Layout.fillWidth: true
                                text: modelData && modelData.source ? modelData.source : ""
                                color: Style.colorLightText
                                font.pointSize: Fonts.size10
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData && modelData.target ? modelData.target : ""
                                color: Style.colorText
                                font.pointSize: Fonts.size11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Error line + browser fallback.
                    RowLayout {
                        Layout.fillWidth: true
                        visible: LuteController.translationError.length > 0
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: LuteController.translationError
                            color: Style.colorErrorText
                            font.pointSize: Fonts.size10
                            wrapMode: Text.Wrap
                        }

                        Label {
                            visible: LuteController.translationProvider !== "ai"
                            text: qsTr("Im Browser öffnen")
                            color: Style.colorBasePurple
                            font.pointSize: Fonts.size10
                            font.underline: true

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: LuteController.openTranslationInBrowser()
                            }
                        }
                    }

                    // AI misconfigured hint with settings link.
                    RowLayout {
                        Layout.fillWidth: true
                        visible: LuteController.translationProvider === "ai"
                                 && LuteController.translationError.length > 0
                        spacing: 6

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("KI (") + LuteController.aiConfig.providerLabel
                                  + qsTr(") prüfen – Provider, Modell und Ziel sprache stehen in den Einstellungen.")
                            color: Style.colorPageSubtitle
                            font.pointSize: Fonts.size10
                            wrapMode: Text.Wrap
                        }

                        Label {
                            text: qsTr("Einstellungen")
                            color: Style.colorBasePurple
                            font.pointSize: Fonts.size10
                            font.underline: true

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: LuteController.openSettings()
                            }
                        }
                    }

                    // Actions: apply, copy, open externally.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        MButton {
                            Layout.preferredHeight: 26
                            horizontalMargins: 9
                            borderWidth: 0
                            backgroundColor: Style.colorBasePurple
                            textColor: Style.colorFocusedButtonText
                            fontSize: Fonts.size9
                            visible: root.hasResult
                            text: qsTr("Übernehmen")

                            onClicked: {
                                translationField.text
                                        = LuteController.translationResult.translation;
                                LuteController.setActiveTranslation(
                                    LuteController.translationResult.translation);
                            }
                        }

                        MButton {
                            Layout.preferredHeight: 26
                            horizontalMargins: 9
                            borderWidth: 1
                            borderColor: Style.colorButtonBorder
                            backgroundColor: "transparent"
                            textColor: Style.colorText
                            fontSize: Fonts.size9
                            visible: root.hasResult
                            text: qsTr("Copy")

                            onClicked: {
                                clipboardHelper.text
                                        = LuteController.translationResult.translation;
                                clipboardHelper.selectAll();
                                clipboardHelper.copy();
                                clipboardHelper.select(0, 0);
                            }
                        }

                        MButton {
                            Layout.preferredHeight: 26
                            horizontalMargins: 9
                            borderWidth: 1
                            borderColor: Style.colorButtonBorder
                            backgroundColor: "transparent"
                            textColor: Style.colorText
                            fontSize: Fonts.size9
                            visible: !LuteController.translationBusy
                                     && !root.hasResult
                                     && LuteController.translationError.length === 0
                            text: qsTr("Translate")

                            onClicked: LuteController.requestTranslationForActiveWord()
                        }

                        Item { Layout.fillWidth: true }

                        MButton {
                            Layout.preferredHeight: 26
                            horizontalMargins: 9
                            borderWidth: 1
                            borderColor: Style.colorButtonBorder
                            backgroundColor: "transparent"
                            textColor: Style.colorText
                            fontSize: Fonts.size9
                            visible: LuteController.translationProvider !== "ai"
                            text: qsTr("Browser")

                            onClicked: LuteController.openTranslationInBrowser()
                        }
                    }
                }
            }

            // ---- manual translation ----------------------------------------
            TextField {
                id: translationField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                leftPadding: 10
                background: Rectangle {
                    color: Style.colorControlBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }
                color: Style.colorBaseInputText
                placeholderText: qsTr("Eigene Übersetzung…")
                placeholderTextColor: Style.colorPlaceholderText
                font.pointSize: Fonts.size11
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                onAccepted: LuteController.setActiveTranslation(text)
                onEditingFinished: {
                    if (text !== LuteController.activeWord.translation)
                        LuteController.setActiveTranslation(text);
                }
            }

            // ---- romanization (only for languages that need it) ------------
            TextField {
                id: romanizationField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                visible: LuteController.activeWord.showRomanization === true
                leftPadding: 10
                background: Rectangle {
                    color: Style.colorControlBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 5
                }
                color: Style.colorBaseInputText
                placeholderText: qsTr("pinyin / kana / reading")
                placeholderTextColor: Style.colorPlaceholderText
                font.pointSize: Fonts.size10
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                onAccepted: LuteController.setActiveRomanization(text)
            }

            // ---- tags --------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Label {
                    text: qsTr("TAGS")
                    color: Style.colorLightText
                    font.pointSize: Fonts.size9
                    font.capitalization: Font.AllUppercase
                }

                Repeater {
                    model: LuteController.activeWord.tags !== undefined
                           ? LuteController.activeWord.tags : []

                    delegate: Rectangle {
                        required property var modelData
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: tagLabel.implicitWidth + 14
                        radius: 10
                        color: Style.colorBasePurple

                        Label {
                            id: tagLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: Style.colorFocusedButtonText
                            font.pointSize: Fonts.size9
                        }
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 24
                    radius: 10
                    color: "transparent"
                    border.width: 1
                    border.color: Style.colorContainerBorder

                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: Style.colorLightText
                        font.pointSize: Fonts.size9
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            tagFilter = "";
                            tagPicker.open();
                            tagSearch.forceActiveFocus();
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ---- word family -------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 5
                visible: LuteController.activeWord.parents !== undefined
                         && LuteController.activeWord.parents.length > 0

                Label {
                    text: qsTr("FAMILIE")
                    color: Style.colorLightText
                    font.pointSize: Fonts.size9
                    font.capitalization: Font.AllUppercase
                }

                Repeater {
                    model: LuteController.activeWord.parents !== undefined
                           ? LuteController.activeWord.parents : []

                    delegate: Rectangle {
                        required property var modelData
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: parentLabel.implicitWidth + 14
                        radius: 10
                        color: Style.colorControlBackground
                        border.width: 1
                        border.color: Style.colorContainerBorder

                        Label {
                            id: parentLabel
                            anchors.centerIn: parent
                            text: (modelData.translation.length > 0
                                   ? modelData.text + " — " + modelData.translation
                                   : modelData.text) + "  ×"
                            color: Style.colorText
                            font.pointSize: Fonts.size9
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: LuteController.unlinkActiveParent(modelData.id)
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // "+ Parent" opens a small popover instead of a permanent field.
            Rectangle {
                Layout.preferredHeight: 20
                Layout.preferredWidth: 62
                radius: 10
                color: "transparent"
                border.width: 1
                border.color: Style.colorContainerBorder

                Label {
                    anchors.centerIn: parent
                    text: qsTr("+ Parent")
                    color: Style.colorLightText
                    font.pointSize: Fonts.size9
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        parentInput.text = "";
                        parentPicker.open();
                        parentInput.forceActiveFocus();
                    }
                }
            }

            // The single flexible region keeps the footer anchored below.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 4
            }

            // ---- footer ------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                MButton {
                    Layout.preferredHeight: 26
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
                    font.pointSize: Fonts.size10
                }
            }
        }
    }

    property string tagFilter: ""
    property var filteredTags: []

    function updateTagFilter() {
        const all = LuteController.tags;
        const needle = tagFilter.toLowerCase();
        let result = [];
        for (let i = 0; i < all.length; ++i) {
            if (needle.length === 0
                || all[i].text.toLowerCase().indexOf(needle) !== -1) {
                result.push(all[i]);
                if (result.length >= 60)
                    break;
            }
        }
        filteredTags = result;
    }

    Popup {
        id: tagPicker
        width: 280
        height: 300
        x: Math.max(8, root.width - width - 16)
        y: 60
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 10
        background: Rectangle {
            color: Style.colorPopupBackground
            radius: 6
            border.width: 1
            border.color: Style.colorContainerBorder
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            TextField {
                id: tagSearch
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                leftPadding: 9
                placeholderText: qsTr("Search tags…")
                placeholderTextColor: Style.colorPlaceholderText
                color: Style.colorBaseInputText
                font.pointSize: Fonts.size10
                background: Rectangle {
                    color: Style.colorControlBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }
                onTextChanged: {
                    root.tagFilter = text;
                    root.updateTagFilter();
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.filteredTags

                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 28
                    color: "transparent"

                    Label {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.text
                        color: Style.colorText
                        font.pointSize: Fonts.size10
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: LuteController.toggleActiveTag(modelData.id)
                    }
                }
            }
        }
    }

    Popup {
        id: parentPicker
        width: 260
        height: 44
        x: Math.max(8, Math.min(root.width - width - 8, 96))
        y: 96
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 6
        background: Rectangle {
            color: Style.colorPopupBackground
            radius: 6
            border.width: 1
            border.color: Style.colorContainerBorder
        }

        RowLayout {
            anchors.fill: parent
            spacing: 6

            TextField {
                id: parentInput
                Layout.fillWidth: true
                Layout.fillHeight: true
                leftPadding: 8
                placeholderText: qsTr("Parent term…")
                placeholderTextColor: Style.colorPlaceholderText
                color: Style.colorBaseInputText
                font.pointSize: Fonts.size10
                selectByMouse: true
                background: Rectangle {
                    color: Style.colorControlBackground
                    border.width: 1
                    border.color: Style.colorContainerBorder
                    radius: 4
                }
                onAccepted: {
                    LuteController.linkActiveParent(text);
                    text = "";
                    parentPicker.close();
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
                text: qsTr("Link")

                onClicked: {
                    LuteController.linkActiveParent(parentInput.text);
                    parentInput.text = "";
                    parentPicker.close();
                }
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
