import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.icons
import Librum.fonts
import Librum.lute 1.0
import "../settings/settingsSidebar"

/***
 * Fork addition: the Lute hub inside Librum. A secondary sidebar like the
 * settings one groups the language-learning areas; the library is the first
 * working section, the others follow in later phases.
 */
Page {
    id: root
    property int currentSection: 0

    Connections {
        target: LuteController

        function onShowTermsRequested() {
            root.currentSection = 1
        }

        function onShowLanguagesRequested() {
            root.currentSection = 3
        }

        function onShowStatisticsPageRequested() {
            root.currentSection = 2
        }

        function onShowTagsRequested() {
            root.currentSection = 4
        }
    }

    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 238
            Layout.fillHeight: true
            color: Style.colorSettingsSidebarBackground

            Rectangle {
                width: 1
                height: parent.height
                anchors.right: parent.right
                color: Style.colorContainerBorder
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    Layout.topMargin: 28
                    Layout.leftMargin: 25
                    text: qsTr("Lute")
                    font.pointSize: Fonts.size19
                    font.bold: true
                    color: Style.colorTitle
                }

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 2
                    Layout.topMargin: 15
                    Layout.leftMargin: 26
                    color: Style.colorDarkSeparator
                }

                Label {
                    Layout.topMargin: 30
                    Layout.leftMargin: 25
                    text: qsTr("LIBRARY")
                    font.pointSize: Fonts.size10
                    font.weight: Font.Bold
                    color: Style.colorLightText
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 10
                    Layout.leftMargin: 1
                    selected: root.currentSection === 0
                    imageLeftMargin: 24
                    imageWidth: 18
                    labelLeftMargin: 10
                    textContent: qsTr("Texts")
                    defaultIcon: Icons.settingsSidebarLute
                    selectedIcon: Icons.settingsSidebarLuteSelected

                    onClicked: root.currentSection = 0
                }

                Label {
                    Layout.topMargin: 25
                    Layout.leftMargin: 25
                    text: qsTr("LEARNING")
                    font.pointSize: Fonts.size10
                    font.weight: Font.Bold
                    color: Style.colorLightText
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 10
                    Layout.leftMargin: 1
                    selected: root.currentSection === 1
                    imageLeftMargin: 24
                    imageWidth: 16
                    labelLeftMargin: 10
                    textContent: qsTr("Terms")
                    defaultIcon: Icons.settingsSidebarShortcuts
                    selectedIcon: Icons.settingsSidebarShortcutsSelected

                    onClicked: root.currentSection = 1
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 4
                    Layout.leftMargin: 1
                    selected: root.currentSection === 2
                    imageLeftMargin: 26
                    imageWidth: 16
                    labelLeftMargin: 10
                    textContent: qsTr("Statistics")
                    defaultIcon: Icons.sidebarPieChart
                    selectedIcon: Icons.sidebarPieChart

                    onClicked: root.currentSection = 2
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 4
                    Layout.leftMargin: 1
                    selected: root.currentSection === 3
                    imageLeftMargin: 26
                    imageWidth: 16
                    labelLeftMargin: 10
                    textContent: qsTr("Languages")
                    defaultIcon: Icons.settingsSidebarSettings
                    selectedIcon: Icons.settingsSidebarSettingsSelected

                    onClicked: root.currentSection = 3
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 4
                    Layout.leftMargin: 1
                    selected: root.currentSection === 4
                    imageLeftMargin: 26
                    imageWidth: 16
                    labelLeftMargin: 10
                    textContent: qsTr("Tags")
                    defaultIcon: Icons.settingsSidebarAbout
                    selectedIcon: Icons.settingsSidebarAboutSelected

                    onClicked: root.currentSection = 4
                }

                Label {
                    Layout.topMargin: 25
                    Layout.leftMargin: 25
                    text: qsTr("DATA")
                    font.pointSize: Fonts.size10
                    font.weight: Font.Bold
                    color: Style.colorLightText
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 10
                    Layout.leftMargin: 1
                    opacity: 0.4
                    imageLeftMargin: 26
                    imageWidth: 16
                    labelLeftMargin: 10
                    textContent: qsTr("Import / Export")
                    defaultIcon: Icons.settingsSidebarStorage
                    selectedIcon: Icons.settingsSidebarStorageSelected
                }

                MSettingsSidebarItem {
                    Layout.preferredHeight: 32
                    Layout.preferredWidth: 236
                    Layout.topMargin: 4
                    Layout.leftMargin: 1
                    imageLeftMargin: 25
                    imageWidth: 18
                    labelLeftMargin: 8
                    textContent: qsTr("Settings")
                    defaultIcon: Icons.settingsSidebarSettings
                    selectedIcon: Icons.settingsSidebarSettingsSelected

                    onClicked: LuteController.openSettings()
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.leftMargin: 25
                    Layout.bottomMargin: 16
                    text: qsTr("Language learning powered by Lute v3")
                    font.pointSize: Fonts.size9
                    color: Style.colorLightText
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentSection

            MLuteTextsPage {}
            MLuteTermsPage {}
            MLuteStatisticsPage {}
            MLuteLanguagesPage {}
            MLuteTagsPage {}
        }
    }
}
