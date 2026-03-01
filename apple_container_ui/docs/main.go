package main

import (
	"fmt"
	"os"
	"time"

	"net/http"
)

func main() {
	startServer()
}

func startServer() {
	hostname, _ := os.Hostname()
	targetURL := os.Getenv("TARGET_URL")

	http.HandleFunc("/ping", func(w http.ResponseWriter, r *http.Request) {
		fmt.Printf("[%s] Received Ping from %s\n", hostname, r.RemoteAddr)
		fmt.Fprintf(w, "pong from %s", hostname)
	})

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "Cider Ping-Pong Server\nHostname: %s\nTarget: %s\n", hostname, targetURL)
	})

	// Background pinger
	if targetURL != "" {
		go func() {
			fmt.Printf("[%s] Starting persistent pinger to %s\n", hostname, targetURL)
			ticker := time.NewTicker(2 * time.Second)
			for range ticker.C {
				resp, err := http.Get(targetURL + "/ping")
				if err != nil {
					fmt.Printf("[%s] Ping Error: %v\n", hostname, err)
					continue
				}
				fmt.Printf("[%s] Ping Success -> %s | Status: %s\n", hostname, targetURL, resp.Status)
				resp.Body.Close()
			}
		}()
	}

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}
	fmt.Printf("[%s] Server running on :%s\n", hostname, port)
	http.ListenAndServe(":"+port, nil)
}
