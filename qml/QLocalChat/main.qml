// This code is released as public domain.
// -- Markus Goetz
import QtQuick 1.0

// Pixel values hardcoded for Nokia N950
Rectangle {
    width: 854
    height: 480
    Rectangle {

        ListView {
            id: nodeView
            model: nodeList
            anchors.fill: parent
            anchors.margins: 10
            highlightFollowsCurrentItem: true
            onCurrentItemChanged : {
                chatField.text = nodeView.currentItem.log
            }
            focus: true

            delegate: Text {
                font.pointSize: 20
                text: addr
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                property string log: model.log
                property string addr: model.addr
                height: 50
                width: parent.width
                MouseArea {
                    anchors.fill: parent
                    onClicked: nodeView.currentIndex = index
                }
            }

            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        }
        color: "white"
        border.color: "grey"
        radius:10

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 250
    }


    Rectangle {
        color: "white"
        border.color: "grey"
        radius:10
        x: 250
        y: 0
        width:854-250
        height: 480-60

        Text {
            font.pointSize: 16
            id: chatField
            anchors.fill: parent
            anchors.margins: 10

            text: "Select a host on the left.<br>Then use the input below to send a message<br>"
        }
    }

    Rectangle {
        TextInput {
            id: input
            font.pointSize: 16
            anchors.fill: parent
            anchors.margins: 10
            Keys.onPressed: {
                if (event.key == Qt.Key_Return) {
                    if (!nodeView.currentItem)
                        return;

                    var chatText = input.text;
                    input.text = "";
                    var addr = nodeView.currentItem.addr;

                    var request = new XMLHttpRequest();
                    request.onreadystatechange = function() {
                        if (request.readyState == 4) {
                            nodeList.appendLog(addr, "You", chatText);
                            chatField.text = nodeView.currentItem.log;
                        }
                    }

                    var url = "http://" + addr + ":31337/chat";
                    request.open("POST", url);
                    request.send("<chat><message>" + chatText + "</message></chat>");
                }
            }
        }
        color: "white"
        border.color: "grey"
        radius:10
        x: 250
        width: 854-250
        y: 480-60
        height: 60
        focus: true
    }


}
