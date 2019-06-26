package main

import (
	"log"
	"time"
	"github.com/gorilla/websocket"
)


func run() {
	var err error
	wsConn, _, err := websocket.DefaultDialer.Dial("ws://127.0.0.1:8080/test1", nil)

	if err != nil {
		log.Fatalf("Failed to connect to eventhub: %v", err)
	}

	defer wsConn.Close()

	wsConn.WriteMessage(websocket.TextMessage, []byte("SUBSCRIBE test2\r\n"));
	time.Sleep(500 * time.Millisecond);
	wsConn.WriteMessage(websocket.TextMessage, []byte("UNSUBSCRIBE test2\r\n"));
	time.Sleep(500 * time.Millisecond);
	wsConn.WriteMessage(websocket.TextMessage, []byte("PUBLISH Here comes a very very big message yo!\r\n"));
	time.Sleep(500 * time.Millisecond);
	wsConn.WriteMessage(websocket.TextMessage, []byte("LIST\r\n"));

	time.Sleep(500 * time.Millisecond);
}

func main() {
	run();
}
