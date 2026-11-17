import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DicomViewer

// Main window content: toolbar above the metadata panel, viewport and errors
// panel, status bar below. Instantiated by Main.qml once the startup steps have
// run. Files arrive from the Open dialog, a drop on the window or the command line.
Item {
    Component.onCompleted: {
        if (AppInfo.startupFile.toString() !== "")
            DicomController.open(AppInfo.startupFile)
    }

    Connections {
        target: DicomController
        function onErrorOccurred() {
            topBar.errorsChecked = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            Layout.fillWidth: true
            errorCount: DicomController.errors.count
            onToggleTheme: AppSettings.theme = Theme.dark ? "light" : "dark"
            onNewRequested: DicomController.reset()
            onOpenRequested: fileDialog.open()
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            handle: Rectangle {
                implicitWidth: 4
                color: Theme.divider
                Rectangle {
                    anchors.fill: parent
                    color: Theme.accent
                    opacity: SplitHandle.pressed || SplitHandle.hovered ? 1 : 0
                    Behavior on opacity {
                        OpacityAnimator { duration: Theme.durationFast; easing.type: Theme.easing }
                    }
                }
            }

            Panel {
                open: topBar.metadataChecked
                restoreWidth: 400
                MetadataPanel {
                    anchors.fill: parent
                }
            }
            View2D {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 240
            }
            Panel {
                open: topBar.errorsChecked
                restoreWidth: 340
                ErrorsPanel {
                    anchors.fill: parent
                }
            }
        }

        StatusBar {
            Layout.fillWidth: true
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open DICOM file")
        currentFolder: AppSettings.lastFolder
        nameFilters: [qsTr("DICOM files (*.dcm *.dicom)"), qsTr("All files (*)")]
        onAccepted: {
            AppSettings.lastFolder = currentFolder
            DicomController.open(selectedFile)
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent
        onDropped: drop => {
            if (drop.hasUrls)
                DicomController.open(drop.urls[0])
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.checkedFill
            border.width: 2
            border.color: Theme.accent
            opacity: dropArea.containsDrag ? 1 : 0
            Behavior on opacity {
                OpacityAnimator { duration: Theme.durationFast; easing.type: Theme.easing }
            }
        }
    }
}
