import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.1

ApplicationWindow {
    property string swfFileName: ""
    property int swfFrameIdx: 0
    visible: true
    width: 640
    height: 480
    title: qsTr("Hello World")

    FileDialog {
        id: fileDialog
        title: "Please choose a file"
        nameFilters: [ "Shockwave Flash (*.swf)" ]
        onAccepted: {
            console.log("You chose: " + fileDialog.fileUrls)
            loadSwf(fileUrl)
        }
        onRejected: {
            console.log("Canceled")
        }
    }
    MessageDialog {
        id: messageDialog
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            MenuItem {
                text: qsTr("&Open")
                shortcut: StandardKey.Open
                onTriggered: fileDialog.open()
            }
            MenuItem {
                text: qsTr("E&xit")
                shortcut: StandardKey.Close
                onTriggered: Qt.quit();
            }
        }
    }

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        cache: false
        source: "qrc:/swf-open-file-format.png"
        onStatusChanged: {
            console.log("Image status ", status)
            if (status == Image.Error) {
                messageDialog.text = "Failed to load " + image.source
                messageDialog.open()
                timer.stop()
                source = "qrc:/swf-open-file-format.png"
            }
        }
    }

    Timer {
        id: timer
        repeat: true
        interval: 1000 / SwfFps
        onTriggered: {
            image.source = "image://swf/%1/%2".arg(swfFileName).arg(++swfFrameIdx)
        }
    }

    function loadSwf(fileName) {
        swfFileName = fileName
        swfFrameIdx = 0
        image.source = "image://swf/%1/%2".arg(swfFileName).arg(swfFrameIdx)
        timer.start()
    }
}
