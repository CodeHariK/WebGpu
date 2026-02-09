package main

import (
	"log"
	"net/http"
)

func main() {
	// Define the directory to serve files from (current folder in this case)
	dir := "."

	// Create a file server to serve static files
	fileServer := http.FileServer(http.Dir(dir))

	// Serve files on the root URL
	http.Handle("/", fileServer)

	// Start the server
	port := ":80"
	log.Printf("Serving %s on HTTP port %s\n", dir, port)
	if err := http.ListenAndServe(port, nil); err != nil {
		log.Fatal(err)
	}
}
