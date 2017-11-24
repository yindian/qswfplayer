import QtQuick 2.2
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.1

ApplicationWindow {
    property string swfFileName: ""
    property int swfFrameIdx: 0
    property int preroll: 0
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
            close()
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
        visible: true
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        cache: false
        source: "qrc:/swf-open-file-format.png"
        onStatusChanged: {
            if (SwfDebug)
            console.log("Image status ", status, source, preroll)
            if (status == Image.Error) {
                if (preroll)
                {
                    messageDialog.text = "Failed to load " + image.source
                    messageDialog.open()
                }
                timer.stop()
                if (SwfDebug)
                console.log("timer should be stopped")
            }
            else if (status == Image.Ready)
            {
                if (preroll && source.toString().substring(0, 12) === "image://swf/")
                {
                    ++preroll;
                    image2.source = "image://swf/%1/%2".arg(swfFileName).arg(++swfFrameIdx)
                    timer.start()
                }
            }
        }
    }

    Image {
        id: image2
        visible: false
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        cache: false
        source: "qrc:/swf-open-file-format.png"
        onStatusChanged: {
            if (SwfDebug)
            console.log("Image2 status ", status, source, preroll)
            if (status == Image.Error) {
                timer.stop()
                if (SwfDebug)
                console.log("timer should be stopped")
            }
            else if (status == Image.Ready)
            {
                if (preroll == 2)
                {
                    preroll = 0;
                }
            }
        }
    }

    Timer {
        id: timer
        repeat: true
        interval: 1000 / SwfFps
        onTriggered: {
            if (preroll)
            {
                console.log("prerolling")
                return;
            }
            if (image.visible)
            {
                if (image2.status == Image.Ready)
                {
                    image2.visible = true
                    image.visible = false
                    image.source = "image://swf/%1/%2".arg(swfFileName).arg(++swfFrameIdx)
                }
                else
                {
                    console.log("image2 not ready")
                }
            }
            else if (image2.visible)
            {
                if (image.status == Image.Ready)
                {
                    image.visible = true
                    image2.visible = false
                    image2.source = "image://swf/%1/%2".arg(swfFileName).arg(++swfFrameIdx)
                }
                else
                {
                    console.log("image not ready")
                }
            }
            else
            {
                console.log("???")
            }
        }
        onRunningChanged: {
            if (SwfDebug)
            console.log("timer running ", running)
        }
    }

    function loadSwf(fileName) {
        timer.stop()
        swfFileName = fileName
        swfFrameIdx = 0
        preroll = 1
        image.source = "qrc:/swf-open-file-format.png"
        image.visible = true
        image.source = "image://swf/%1/%2".arg(swfFileName).arg(swfFrameIdx)
    }
}
