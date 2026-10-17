import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomComponents
import Librum.style
import Librum.fonts
import Librum.lute 1.0

/***
 * Fork addition: Librum's statistics page shows the Lute vocabulary of the
 * selected language instead of the "currently in development" placeholder.
 */
Page {
    id: root
    horizontalPadding: 48
    bottomPadding: 22
    background: Rectangle {
        anchors.fill: parent
        color: Style.colorPageBackground
    }

    Component.onCompleted: LuteController.refreshStats()

    ColumnLayout {
        id: layout
        width: parent.width
        spacing: 0

        RowLayout {
            id: titleRow
            Layout.fillWidth: true
            spacing: 0

            MTitle {
                Layout.topMargin: 64
                titleText: qsTr("Statistics")
                descriptionText: qsTr("Your vocabulary in %1").arg(LuteController.languageName)
                titleSize: Fonts.size25
                descriptionSize: Fonts.size13dot25
            }

            Item { Layout.fillWidth: true }

            MButton {
                Layout.preferredHeight: 38
                Layout.alignment: Qt.AlignBottom
                borderWidth: 1
                borderColor: Style.colorButtonBorder
                backgroundColor: "transparent"
                textColor: Style.colorText
                horizontalMargins: 14
                fontSize: Fonts.size12
                text: qsTr("Refresh")

                onClicked: LuteController.refreshStats()
            }
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
                width: flickWrapper.width - 10
                spacing: 18

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    StatCard {
                        Layout.fillWidth: true
                        label: qsTr("Terms")
                        value: LuteController.stats.totalTerms
                    }

                    StatCard {
                        Layout.fillWidth: true
                        label: qsTr("Learning")
                        value: LuteController.stats.learningTerms
                        accent: Style.colorBasePurple
                    }

                    StatCard {
                        Layout.fillWidth: true
                        label: qsTr("Known")
                        value: LuteController.stats.knownTerms
                        accent: "#A8DDB5"
                    }

                    StatCard {
                        Layout.fillWidth: true
                        label: qsTr("Words read")
                        value: LuteController.stats.wordsReadTotal
                    }
                }

                // ---- coverage ------------------------------------------------
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

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: qsTr("Document coverage")
                                font.pointSize: Fonts.size17
                                font.weight: Font.DemiBold
                                color: Style.colorText
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: Math.round(LuteController.stats.coverage) + "%"
                                font.pointSize: Fonts.size22
                                font.weight: Font.DemiBold
                                color: Style.colorBasePurple
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            text: LuteController.stats.hasDocument
                                  ? qsTr("%1 of %2 words of “%3” are already in your vocabulary%4")
                                    .arg(LuteController.stats.documentKnownTokens)
                                    .arg(LuteController.stats.documentTokenCount)
                                    .arg(LuteController.documentTitle)
                                    .arg(LuteController.stats.coverageEstimated
                                         ? qsTr(" (estimated from a sample of pages)") : "")
                                  : qsTr("Open a PDF to measure how much of it you already know.")
                            color: Style.colorPageSubtitle
                            font.pointSize: Fonts.size11
                            wrapMode: Text.WordWrap
                        }

                        Pane {
                            Layout.fillWidth: true
                            Layout.topMargin: 16
                            Layout.preferredHeight: 8
                            padding: 0
                            background: Rectangle {
                                color: Style.colorContainerBorder
                                radius: 4
                            }

                            Rectangle {
                                width: parent.width
                                       * Math.min(1.0, LuteController.stats.coverage / 100)
                                height: parent.height
                                radius: 4
                                color: Style.colorBasePurple
                            }
                        }
                    }
                }

                // ---- status distribution --------------------------------------
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
                            text: qsTr("Status distribution")
                            font.pointSize: Fonts.size17
                            font.weight: Font.DemiBold
                            color: Style.colorText
                        }

                        Repeater {
                            model: LuteController.stats.byStatus

                            delegate: RowLayout {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.topMargin: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 12
                                    Layout.preferredHeight: 12
                                    radius: 2
                                    color: modelData.color
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: Style.colorText
                                    font.pointSize: Fonts.size12
                                }

                                Label {
                                    text: modelData.count
                                    color: Style.colorPageSubtitle
                                    font.pointSize: Fonts.size12
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component StatCard: Pane {
        id: card
        property string label: ""
        property int value: 0
        property color accent: Style.colorText

        Layout.preferredHeight: 118
        verticalPadding: 22
        horizontalPadding: 24
        background: Rectangle {
            color: Style.colorContainerBackground
            border.color: Style.colorContainerBorder
            radius: 4
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Label {
                text: card.value
                font.pointSize: Fonts.size28
                font.weight: Font.DemiBold
                color: card.accent
            }

            Label {
                Layout.topMargin: 6
                text: card.label
                font.pointSize: Fonts.size11
                color: Style.colorPageSubtitle
            }
        }
    }
}
