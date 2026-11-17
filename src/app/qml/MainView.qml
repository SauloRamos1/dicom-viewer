// SPDX-License-Identifier: GPL-3.0-only
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
        optionsPane.fx = AppSettings.optionsX
        optionsPane.fy = AppSettings.optionsY
        topBar.optionsChecked = AppSettings.optionsOpen
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
            errorCount: DicomController.errors.errorCount
            onToggleTheme: AppSettings.theme = Theme.dark ? "light" : "dark"
            onNewRequested: DicomController.reset()
            onOpenRequested: fileDialog.open()
            onOpenFolderRequested: folderDialog.open()
            onOptionsCheckedChanged: {
                AppSettings.optionsOpen = optionsChecked
                // Keep the arrow keys stepping frames after the pane goes away.
                if (!optionsChecked && optionsPane.containsFocus())
                    viewport.focusView()
            }
        }

        // The TopBar's 3D button decides which view the controller prepares data for.
        Binding {
            target: DicomController
            property: "view3D"
            value: topBar.view3DChecked
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
            Item {
                SplitView.fillWidth: true
                // Room for the Options pane (300) and its margins.
                SplitView.minimumWidth: 332
                clip: true

                ViewerPane {
                    id: viewport
                    anchors.fill: parent
                    smoothing: AppSettings.smoothing
                }
                OptionsPane {
                    id: optionsPane
                    viewer: viewport
                    open: topBar.optionsChecked
                    onCloseRequested: topBar.optionsChecked = false
                    onPositionCommitted: {
                        AppSettings.optionsX = fx
                        AppSettings.optionsY = fy
                    }
                }
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

    FolderDialog {
        id: folderDialog
        title: qsTr("Open DICOM series folder")
        currentFolder: AppSettings.lastFolder
        onAccepted: {
            // The dialog next time starts beside the folder, not inside it.
            const folder = selectedFolder.toString().replace(/\/+$/, "")
            AppSettings.lastFolder = folder.substring(0, folder.lastIndexOf("/"))
            DicomController.openSeries(selectedFolder)
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
