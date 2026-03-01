package main

import (
	"fmt"
	"os"
	"runtime"

	"net/http"
)

func main() {
	startServer()
}

func startServer() {
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		hostname, _ := os.Hostname()
		fmt.Fprintf(w, "System Info:\n")
		fmt.Fprintf(w, "- OS: %s\n", runtime.GOOS)
		fmt.Fprintf(w, "- Arch: %s\n", runtime.GOARCH)
		fmt.Fprintf(w, "- Hostname: %s\n", hostname)
		fmt.Fprintf(w, "- CPUs: %d\n", runtime.NumCPU())
	})
	fmt.Println("Server running on :8080")
	http.ListenAndServe(":8080", nil)
}
