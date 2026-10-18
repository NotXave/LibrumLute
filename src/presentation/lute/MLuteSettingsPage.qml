import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: the Lute settings page. Language, lute.db, AI provider and
 * the Lute library integration live here, in Librum's settings layout.
 */
Page {
    id: root
    horizontalPadding: 48
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    ListModel {
        id: languageModel
    }

    ListModel {
        id: providerModel
        ListElement { providerKey: "ollama"; label: "Ollama (local)" }
        ListElement { providerKey: "openai"; label: "OpenAI" }
        ListElement { providerKey: "gemini"; label: "Google Gemini" }
    }

    ListModel {
        id: presentationModel
        ListElement { key: "popup"; label: "Word popup" }
        ListElement { key: "panel"; label: "Side panel (full Lute)" }
    }

    ListModel {
        id: panelSideModel
        ListElement { key: "left"; label: "Left" }
        ListElement { key: "right"; label: "Right" }
    }

    ListModel {
        id: translationProviderModel
        ListElement { key: "reverso"; label: "Reverso Context" }
        ListElement { key: "google"; label: "Google Translate" }
        ListElement { key: "ai"; label: "AI translation" }
        ListElement { key: "mymemory"; label: "MyMemory" }
    }

    function rebuildLanguageModel() {
        languageModel.clear();
        const languages = LuteController.languages;
        for (let i = 0; i < languages.length; ++i)
            languageModel.append({
                                     "languageId": languages[i].id,
                                     "name": languages[i].name
                                 });
        languageComboBox.defaultIndex = root.languageIndex();
    }

    Connections {
        target: LuteController

        function onLanguageChanged() {
            root.rebuildLanguageModel();
        }

        function onDatabaseChanged() {
            root.rebuildLanguageModel();
        }

        function onAiConfigChanged() {
            internal.loadAiSettings();
        }

        function onReaderPresentationChanged() {
            presentationComboBox.defaultIndex =
                LuteController.readerPresentation === "panel" ? 1 : 0;
            presentationComboBox.text =
                presentationModel.get(presentationComboBox.defaultIndex).label;
        }

        function onReaderPanelSideChanged() {
            panelSideComboBox.defaultIndex =
                LuteController.readerPanelSide === "left" ? 0 : 1;
            panelSideComboBox.text =
                panelSideModel.get(panelSideComboBox.defaultIndex).label;
        }
    }

    function toLocalPath(url) {
        let path = decodeURIComponent(url.toString());
        if (path.startsWith("file:///"))
            path = path.substring(8);
        else if (path.startsWith("file://"))
            path = path.substring(7);
        return path;
    }

    function languageIndex() {
        const languages = LuteController.languages;
        for (let i = 0; i < languages.length; ++i) {
            if (languages[i].id === LuteController.languageId)
                return i;
        }
        return 0;
    }

    ColumnLayout {
        id: layout
        width: parent.width
        spacing: 0

        Pane {
            id: messageBanner
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.preferredHeight: messageLabel.implicitHeight + 24
            visible: LuteController.lastMessage.length > 0
            padding: 0
            background: Rectangle {
                color: Style.colorContainerBackground
                border.width: 1
                border.color: LuteController.lastMessageIsError
                              ? Style.colorErrorBorder : Style.colorContainerBorder
                radius: 4
            }

            Label {
                id: messageLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                text: LuteController.lastMessage
                color: LuteController.lastMessageIsError
                       ? Style.colorErrorText : Style.colorGreenText
                font.pointSize: Fonts.size11
                wrapMode: Text.WordWrap
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: LuteController.messageShown()
            }
        }

        Timer {
            interval: 7000
            running: LuteController.lastMessage.length > 0
            onTriggered: LuteController.messageShown()
        }

        MButton {
            Layout.topMargin: 12
            Layout.preferredHeight: 30
            horizontalMargins: 12
            borderWidth: 1
            borderColor: Style.colorButtonBorder
            backgroundColor: "transparent"
            textColor: Style.colorText
            fontSize: Fonts.size11
            visible: LuteController.documentOpen
            text: qsTr("← Back to reading")

            onClicked: LuteController.openReader()
        }

        MTitle {
            Layout.topMargin: 40
            titleText: qsTr("Lute")
            descriptionText: qsTr("Language learning with your own lute.db")
            titleSize: Fonts.size25
            descriptionSize: Fonts.size13dot25
        }

        MFlickWrapper {
            id: flickWrapper
            Layout.fillWidth: true
            Layout.preferredHeight: root.height - 64 - 90 - root.bottomPadding
            contentHeight: contentLayout.implicitHeight
            Layout.topMargin: 32
            flickDeceleration: 20000
            maximumFlickVelocity: 2300
            clip: true

            ColumnLayout {
                id: contentLayout
                width: flickWrapper.width - internal.scrollbarOffset
                spacing: 22

                // ---- language ------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("Learning language")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: qsTr("Statuses, translations and phrases are stored per language in lute.db.")
                            font.pointSize: Fonts.size11
                            color: Style.colorPageSubtitle
                            wrapMode: Text.WordWrap
                        }

                        MComboBox {
                            id: languageComboBox
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 380
                            Layout.topMargin: 16
                            itemHeight: 32
                            fontSize: Fonts.size12
                            selectedItemFontSize: Fonts.size12
                            selectedItemPadding: 4
                            maxHeight: 260
                            contentPropertyName: "name"
                            model: languageModel

                            onItemChanged: index => {
                                               if (index >= 0 && index < languageModel.count)
                                               LuteController.selectLanguage(languageModel.get(index).languageId)
                                               closePopup()
                                           }
                        }

                        Label {
                            Layout.topMargin: 8
                            text: qsTr("%1 languages in this database").arg(LuteController.languages.length)
                            font.pointSize: Fonts.size10
                            color: Style.colorLightText
                        }
                    }
                }

                // ---- database ------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("Database")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: LuteController.databasePath
                            font.pointSize: Fonts.size11
                            color: Style.colorPageSubtitle
                            elide: Text.ElideMiddle
                        }

                        RowLayout {
                            Layout.topMargin: 16
                            spacing: 10

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 12
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                text: qsTr("Open lute.db")

                                onClicked: openDatabaseDialog.open()
                            }

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 12
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                text: qsTr("New lute.db")

                                onClicked: newDatabaseDialog.open()
                            }

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 12
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                text: qsTr("Back up…")

                                onClicked: backupDialog.open()
                            }

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 12
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                text: qsTr("Restore…")

                                onClicked: restoreDialog.open()
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // ---- reader ---------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("Reader")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: qsTr("Choose between the quick word popup and the full Lute side panel with the translation field and all translation methods.")
                            font.pointSize: Fonts.size11
                            color: Style.colorPageSubtitle
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 16
                            spacing: 24

                            ColumnLayout {
                                spacing: 6

                                Label {
                                    text: qsTr("Word display")
                                    color: Style.colorLightText
                                    font.pointSize: Fonts.size10
                                }

                                MComboBox {
                                    id: presentationComboBox
                                    Layout.preferredHeight: 36
                                    Layout.preferredWidth: 260
                                    itemHeight: 32
                                    fontSize: Fonts.size12
                                    selectedItemFontSize: Fonts.size12
                                    selectedItemPadding: 4
                                    contentPropertyName: "label"
                                    model: presentationModel

                                    Component.onCompleted: {
                                        defaultIndex =
                                            LuteController.readerPresentation === "panel"
                                            ? 1 : 0;
                                        text = presentationModel.get(defaultIndex).label;
                                    }

                                    onItemChanged: index => {
                                                       if (index >= 0 && index < presentationModel.count)
                                                       LuteController.setReaderPresentation(
                                                           presentationModel.get(index).key);
                                                       closePopup();
                                                   }
                                }
                            }

                            ColumnLayout {
                                spacing: 6

                                Label {
                                    text: qsTr("Panel side")
                                    color: Style.colorLightText
                                    font.pointSize: Fonts.size10
                                }

                                MComboBox {
                                    id: panelSideComboBox
                                    Layout.preferredHeight: 36
                                    Layout.preferredWidth: 160
                                    itemHeight: 32
                                    fontSize: Fonts.size12
                                    selectedItemFontSize: Fonts.size12
                                    selectedItemPadding: 4
                                    contentPropertyName: "label"
                                    model: panelSideModel
                                    enabled: LuteController.readerPresentation === "panel"
                                    opacity: enabled ? 1 : 0.5

                                    Component.onCompleted: {
                                        defaultIndex =
                                            LuteController.readerPanelSide === "left"
                                            ? 0 : 1;
                                        text = panelSideModel.get(defaultIndex).label;
                                    }

                                    onItemChanged: index => {
                                                       if (index >= 0 && index < panelSideModel.count)
                                                       LuteController.setReaderPanelSide(
                                                           panelSideModel.get(index).key);
                                                       closePopup();
                                                   }
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // ---- Translation -------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("Translation")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 40

                            ColumnLayout {
                                spacing: 6

                                Label {
                                    text: qsTr("Default source")
                                    color: Style.colorLightText
                                    font.pointSize: Fonts.size10
                                }

                                MComboBox {
                                    id: translationProviderComboBox
                                    Layout.preferredHeight: 36
                                    Layout.preferredWidth: 200
                                    itemHeight: 32
                                    fontSize: Fonts.size12
                                    selectedItemFontSize: Fonts.size12
                                    selectedItemPadding: 4
                                    contentPropertyName: "label"
                                    model: translationProviderModel

                                    Component.onCompleted: syncSelection()

                                    onItemChanged: index => {
                                                       if (index >= 0
                                                           && index < translationProviderModel.count)
                                                       LuteController.setTranslationProvider(
                                                           translationProviderModel.get(index).key);
                                                       closePopup();
                                                   }

                                    function syncSelection() {
                                        let index = 0;
                                        for (let i = 0; i < translationProviderModel.count; ++i) {
                                            if (translationProviderModel.get(i).key
                                                === LuteController.translationProvider)
                                                index = i;
                                        }
                                        defaultIndex = index;
                                        text = translationProviderModel.get(index).label;
                                    }

                                    Connections {
                                        target: LuteController

                                        function onTranslationChanged() {
                                            translationProviderComboBox.syncSelection();
                                        }
                                    }
                                }

                                Label {
                                    Layout.topMargin: 10
                                    text: qsTr("Disabled sources disappear from the term panel tabs.")
                                    color: Style.colorPageSubtitle
                                    font.pointSize: Fonts.size10
                                }
                            }

                            ColumnLayout {
                                spacing: 10

                                Label {
                                    text: qsTr("Sources")
                                    color: Style.colorLightText
                                    font.pointSize: Fonts.size10
                                }

                                RowLayout {
                                    spacing: 18

                                    Repeater {
                                        model: [{
                                                "key": "reverso",
                                                "label": "Reverso"
                                            }, {
                                                "key": "google",
                                                "label": "Google"
                                            }, {
                                                "key": "mymemory",
                                                "label": "MyMemory"
                                            }]

                                        delegate: RowLayout {
                                            required property var modelData
                                            spacing: 6

                                            MCheckBox {
                                                Layout.preferredWidth: 20
                                                Layout.preferredHeight: 20
                                                checked: internal.providerEnabled(modelData.key)

                                                onClicked:
                                                    LuteController.setTranslationProviderEnabled(
                                                        modelData.key, checked)
                                            }

                                            Label {
                                                text: modelData.label
                                                color: Style.colorText
                                                font.pointSize: Fonts.size11
                                            }
                                        }
                                    }
                                }

                                Label {
                                    text: qsTr("AI is always available. Target language for all sources: ")
                                          + LuteController.aiConfig.targetLanguage
                                          + qsTr(" (see AI translation).")
                                    color: Style.colorPageSubtitle
                                    font.pointSize: Fonts.size10
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // ---- AI -------------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("AI translation")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: qsTr("A contextual gloss for the clicked word and its sentence.")
                            font.pointSize: Fonts.size11
                            color: Style.colorPageSubtitle
                            wrapMode: Text.WordWrap
                        }

                        MComboBox {
                            id: providerComboBox
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 380
                            Layout.topMargin: 16
                            itemHeight: 32
                            fontSize: Fonts.size12
                            selectedItemFontSize: Fonts.size12
                            selectedItemPadding: 4
                            contentPropertyName: "label"
                            model: providerModel

                            onItemChanged: index => {
                                               if (index >= 0 && index < providerModel.count)
                                               internal.updateProvider(providerModel.get(index).providerKey)
                                               closePopup()
                                           }
                        }

                        MLabeledInputBox {
                            id: modelInput
                            Layout.preferredWidth: 380
                            Layout.topMargin: 14
                            headerText: qsTr("Model")
                            placeholderContent: internal.defaultModel(internal.currentProvider)
                        }

                        MLabeledInputBox {
                            id: baseUrlInput
                            Layout.preferredWidth: 380
                            Layout.topMargin: 14
                            headerText: qsTr("Base URL")
                            placeholderContent: internal.defaultBaseUrl(internal.currentProvider)
                        }

                        MLabeledInputBox {
                            id: apiKeyInput
                            Layout.preferredWidth: 380
                            Layout.topMargin: 14
                            headerText: qsTr("API key")
                            placeholderContent: qsTr("only needed for OpenAI / Gemini")
                        }

                        MLabeledInputBox {
                            id: targetLanguageInput
                            Layout.preferredWidth: 380
                            Layout.topMargin: 14
                            headerText: qsTr("Translate into")
                            placeholderContent: "German"
                        }

                        MButton {
                            Layout.preferredHeight: 32
                            Layout.topMargin: 18
                            horizontalMargins: 14
                            borderWidth: 0
                            backgroundColor: Style.colorBasePurple
                            textColor: Style.colorFocusedButtonText
                            fontSize: Fonts.size11
                            text: qsTr("Save AI settings")

                            onClicked: {
                                LuteController.saveAiSettings(internal.currentProvider,
                                                              modelInput.text,
                                                              baseUrlInput.text,
                                                              apiKeyInput.text,
                                                              targetLanguageInput.text)
                                internal.loadAiSettings()
                            }
                        }
                    }
                }

                // ---- library -------------------------------------------
                Pane {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 30
                    verticalPadding: 26
                    horizontalPadding: 28
                    background: Rectangle {
                        color: Style.colorContainerBackground
                        border.color: Style.colorContainerBorder
                        radius: 4
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Label {
                            text: qsTr("Lute library")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: LuteController.documentRegistered
                                  ? qsTr("“%1” is already in the Lute library.").arg(LuteController.documentTitle)
                                  : LuteController.documentOpen
                                    ? qsTr("“%1” is open. Add it to the Lute library, or pick another PDF.").arg(LuteController.documentTitle)
                                    : qsTr("Add any PDF to your Lute library. Its text becomes readable in Lute.")
                            font.pointSize: Fonts.size11
                            color: Style.colorPageSubtitle
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.topMargin: 16
                            spacing: 10

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 14
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                visible: LuteController.documentOpen
                                         && !LuteController.documentRegistered
                                text: qsTr("Add current document")

                                onClicked: LuteController.addDocumentToLuteLibrary()
                            }

                            MButton {
                                Layout.preferredHeight: 32
                                horizontalMargins: 14
                                borderWidth: 1
                                borderColor: Style.colorButtonBorder
                                backgroundColor: "transparent"
                                textColor: Style.colorText
                                fontSize: Fonts.size11
                                text: qsTr("Add a book…")

                                onClicked: addPdfDialog.open()
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: restoreDialog
        title: qsTr("Restore lute.db from a backup")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Lute databases (*.db)"), qsTr("All files (*)")]

        onAccepted: LuteController.restoreDatabase(
                        root.toLocalPath(selectedFile))
    }

    FileDialog {
        id: backupDialog
        title: qsTr("Back up lute.db")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "db"
        nameFilters: [qsTr("Lute databases (*.db)"), qsTr("All files (*)")]

        onAccepted: LuteController.backupDatabase(
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

    FileDialog {
        id: openDatabaseDialog
        title: qsTr("Open lute.db")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Lute databases (lute.db)"), qsTr("All files (*)")]

        onAccepted: LuteController.openDatabase(root.toLocalPath(selectedFile))
    }

    FileDialog {
        id: newDatabaseDialog
        title: qsTr("Create a new lute.db")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "db"
        nameFilters: [qsTr("Lute databases (*.db)"), qsTr("All files (*)")]
        currentFile: "file:///" + LuteController.databasePath.replace("\\", "/")

        onAccepted: LuteController.createDatabase(root.toLocalPath(selectedFile))
    }

    Component.onCompleted: {
        root.rebuildLanguageModel();
        internal.loadAiSettings();
    }

    QtObject {
        id: internal
        property int scrollbarOffset: 10
        property string currentProvider: "ollama"
        property var enabledProviders: LuteController.translationProviders

        function providerEnabled(key) {
            const list = internal.enabledProviders;
            for (let i = 0; i < list.length; ++i) {
                if (list[i].key === key)
                    return true;
            }
            return false;
        }

        function providerIndex() {
            const providers = ["ollama", "openai", "gemini"];
            const current = LuteController.aiConfig.provider;
            const index = providers.indexOf(current);
            return index < 0 ? 0 : index;
        }

        function defaultModel(provider) {
            if (provider === "openai")
                return "gpt-4o-mini";
            if (provider === "gemini")
                return "gemini-2.0-flash";
            // Keep in sync with defaultModel() in lute_ai_access.cpp.
            return "qwen2.5:7b";
        }

        function defaultBaseUrl(provider) {
            if (provider === "openai")
                return "https://api.openai.com/v1";
            if (provider === "gemini")
                return "https://generativelanguage.googleapis.com/v1beta";
            return "http://localhost:11434";
        }

        function updateProvider(provider) {
            internal.currentProvider = provider;
            modelInput.text = defaultModel(provider);
            baseUrlInput.text = defaultBaseUrl(provider);
            modelInput.placeholderContent = defaultModel(provider);
            baseUrlInput.placeholderContent = defaultBaseUrl(provider);
        }

        function loadAiSettings() {
            const config = LuteController.aiConfig;
            internal.currentProvider = config.provider;
            modelInput.text = config.model;
            baseUrlInput.text = config.baseUrl;
            apiKeyInput.text = config.apiKey;
            targetLanguageInput.text = config.targetLanguage;
            providerComboBox.defaultIndex = providerIndex();
            providerComboBox.text = config.providerLabel;
        }
    }
}
