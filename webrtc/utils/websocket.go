package utils

import (
	"flag"
	"fmt"
	"log"
	"net/http"
	"strconv"
	"sync"

	"github.com/gorilla/websocket"
)

// Define the ConnectObj struct
type ConnectObj struct {
	Conn *websocket.Conn
	Sdp  string
}

// Declare a slice of connectObj
var ConnectObjMap map[*websocket.Conn]*ConnectObj

var (
	SdpChan = make(chan ConnectObj)
	mu      sync.Mutex
)

var upgrader = websocket.Upgrader{
	// Allow all connections (in production, use proper origin checks)
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

// Handle WebSocket connection
func handleWebSocket(w http.ResponseWriter, r *http.Request) {
	// Upgrade HTTP to WebSocket
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		fmt.Println("Upgrade error:", err)
		return
	}
	defer conn.Close()

	fmt.Println("Client connected!")

	// Store the connection in the map
	mu.Lock()
	ConnectObjMap[conn] = &ConnectObj{
		Conn: conn,
		Sdp:  "",
	}
	mu.Unlock()

	var wg sync.WaitGroup

	// Increment the WaitGroup counter for this goroutine
	wg.Add(1)

	// Start a goroutine for handling the WebSocket communication
	go func() {
		defer wg.Done() // Mark this goroutine as done when finished

		// Handle the WebSocket connection
		for {
			_, message, err := conn.ReadMessage()
			if err != nil {
				// Check for WebSocket closure or error
				if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
					fmt.Println("Connection closed gracefully")
				} else {
					fmt.Println("Read error:", err)
				}
				break
			}

			fmt.Printf("Received: %d bytes\n", len(message))

			// Update the SDP in the map
			mu.Lock() // Lock the map while updating
			ConnectObjMap[conn].Sdp = string(message)
			mu.Unlock()

			// Send the updated SDP to the channel
			SdpChan <- *ConnectObjMap[conn]
		}

		// Clean up after finishing
		mu.Lock()
		delete(ConnectObjMap, conn) // Remove the connection from the map
		mu.Unlock()
	}()

	wg.Wait()
}

func BuildServer() {
	port := flag.Int("port", 80, "http server port")
	flag.Parse()

	ConnectObjMap = make(map[*websocket.Conn]*ConnectObj)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "./demo.html")
	})

	http.HandleFunc("/ws", handleWebSocket)

	go func() {
		fmt.Println("Server running on port", *port)
		panic(http.ListenAndServe(":"+strconv.Itoa(*port), nil))
	}()
}

func SendMessage(encodedObj string, sdpobj ConnectObj) {
	err := sdpobj.Conn.WriteMessage(websocket.TextMessage, []byte(encodedObj))
	if err != nil {
		log.Println("Write error:", err)
	}
}
