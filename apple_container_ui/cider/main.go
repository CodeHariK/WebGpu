package main

import (
	"bufio"
	"bytes"
	"embed"
	"encoding/json"
	"fmt"
	"io/fs"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/creack/pty"
	"github.com/gorilla/websocket"
)

//go:embed all:dist
var staticFiles embed.FS

// --- Types ---

type StatusResponse struct {
	Status    string `json:"status"`
	Version   string `json:"version"`
	Timestamp string `json:"timestamp"`
}

type ImageInfo struct {
	ID         string `json:"ID"`
	Repository string `json:"Repository"`
	Tag        string `json:"Tag"`
	Size       string `json:"Size"`
	CreatedAt  string `json:"CreatedAt"`
}

type CommandLog struct {
	ID        string    `json:"id"`
	Timestamp time.Time `json:"timestamp"`
	Command   string    `json:"command"`
	Output    string    `json:"output"`
	IsError   bool      `json:"isError"`
	IsPartial bool      `json:"isPartial"`
}

// Broker manages SSE clients and broadcasts messages
type Broker struct {
	notifier       chan []byte
	clients        map[chan []byte]bool
	newClients     chan chan []byte
	closingClients chan chan []byte
}

func NewBroker() *Broker {
	return &Broker{
		notifier:       make(chan []byte, 1),
		clients:        make(map[chan []byte]bool),
		newClients:     make(chan chan []byte),
		closingClients: make(chan chan []byte),
	}
}

func (b *Broker) Start() {
	for {
		select {
		case s := <-b.newClients:
			b.clients[s] = true
		case s := <-b.closingClients:
			delete(b.clients, s)
			close(s)
		case event := <-b.notifier:
			for client := range b.clients {
				client <- event
			}
		}
	}
}

func (b *Broker) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "Streaming unsupported!", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	messageChan := make(chan []byte)
	b.newClients <- messageChan
	defer func() {
		b.closingClients <- messageChan
	}()

	notify := r.Context().Done()
	go func() {
		<-notify
		b.closingClients <- messageChan
	}()

	for {
		fmt.Fprintf(w, "data: %s\n\n", <-messageChan)
		flusher.Flush()
	}
}

var (
	actionLogs []CommandLog
	logBroker  = NewBroker()
)

func logAction(command string, output string, isError bool) {
	logActionExt(command, output, isError, false)
}

func logActionExt(command string, output string, isError bool, isPartial bool) {
	// Simple noise filtering similar to original JS implementation
	if strings.Contains(command, "ls --all") || strings.Contains(command, "image list") || strings.Contains(command, "top") || strings.Contains(command, "stats") {
		if !isError {
			return
		}
	}

	logEntry := CommandLog{
		ID:        fmt.Sprintf("%d", time.Now().UnixNano()),
		Timestamp: time.Now(),
		Command:   command,
		Output:    output,
		IsError:   isError,
		IsPartial: isPartial,
	}

	// Only persist full logs to history
	if !isPartial {
		actionLogs = append(actionLogs, logEntry)
		// Keep only last 100 logs
		if len(actionLogs) > 100 {
			actionLogs = actionLogs[1:]
		}
	}

	// Broadcast to SSE clients
	if data, err := json.Marshal(logEntry); err == nil {
		logBroker.notifier <- data
	}
}

// --- Helpers ---

func runCli(args ...string) ([]byte, error) {
	fullCmd := "container " + strings.Join(args, " ")
	cmd := exec.Command("container", args...)

	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()

	if err := cmd.Start(); err != nil {
		logAction(fullCmd, fmt.Sprintf("Error: %v", err), true)
		return nil, err
	}

	var outBuf bytes.Buffer
	var mu sync.Mutex

	// Stream stdout and stderr
	go func() {
		s := bufio.NewScanner(stdout)
		for s.Scan() {
			line := s.Text()
			mu.Lock()
			outBuf.WriteString(line + "\n")
			mu.Unlock()
			logActionExt(fullCmd, line, false, true)
		}
	}()

	go func() {
		s := bufio.NewScanner(stderr)
		for s.Scan() {
			line := s.Text()
			logActionExt(fullCmd, line, true, true)
		}
	}()

	err := cmd.Wait()

	if err != nil {
		logAction(fullCmd, fmt.Sprintf("Command failed: %v", err), true)
	} else {
		logAction(fullCmd, "Command completed successfully", false)
	}

	return outBuf.Bytes(), err
}

func parseCommand(cmdStr string) []string {
	var args []string
	var current strings.Builder
	inQuotes := false
	for _, r := range cmdStr {
		if r == '"' {
			inQuotes = !inQuotes
		} else if r == ' ' && !inQuotes {
			if current.Len() > 0 {
				args = append(args, current.String())
				current.Reset()
			}
		} else {
			current.WriteRune(r)
		}
	}
	if current.Len() > 0 {
		args = append(args, current.String())
	}
	return args
}

func enableCors(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS, DELETE")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}
		next.ServeHTTP(w, r)
	})
}

// --- Handlers ---

func handleStatus(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	cmd := exec.Command("container", "system", "status")
	err := cmd.Run()
	status := "ok"
	if err != nil {
		status = "offline"
	}

	json.NewEncoder(w).Encode(StatusResponse{
		Status:    status,
		Version:   "0.0.1",
		Timestamp: time.Now().Format(time.RFC3339),
	})
}

func handleListContainers(w http.ResponseWriter, r *http.Request) {
	output, err := runCli("ls", "--all", "--format", "json")
	if err != nil {
		outStr := string(output)
		if strings.Contains(outStr, "container system start") ||
			strings.Contains(outStr, "XPC connection error") ||
			strings.Contains(outStr, "Connection invalid") {
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		}
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleInspectContainer(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/inspect/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	output, err := runCli("inspect", id)
	if err != nil {
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleListImages(w http.ResponseWriter, r *http.Request) {
	output, err := runCli("image", "list", "--format", "json")
	if err != nil {
		outStr := string(output)
		if strings.Contains(outStr, "container system start") ||
			strings.Contains(outStr, "XPC connection error") ||
			strings.Contains(outStr, "Connection invalid") {
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		}
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleGetStats(w http.ResponseWriter, r *http.Request) {
	output, err := runCli("stats", "--no-stream", "--format", "json")
	if err != nil {
		// If no containers are running, 'stats' might error or return empty.
		// For now, return empty array.
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte("[]"))
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleContainerAction(w http.ResponseWriter, r *http.Request) {
	// Simple routing for /api/containers/{id}/{action}
	parts := strings.Split(strings.Trim(r.URL.Path, "/"), "/")
	if len(parts) < 4 {
		http.Error(w, "invalid path", http.StatusBadRequest)
		return
	}
	id := parts[2]
	action := parts[3]

	var output []byte
	var err error

	switch action {
	case "stop":
		output, err = runCli("stop", id)
	default:
		http.Error(w, "invalid action", http.StatusBadRequest)
		return
	}

	if err != nil {
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func handleRemoveContainer(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	output, err := runCli("rm", "--force", id)
	if err != nil {
		// If container is already gone, don't return 500
		if strings.Contains(string(output), "failed to delete one or more containers") {
			w.WriteHeader(http.StatusNoContent) // 204 already gone
			return
		}
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.WriteHeader(http.StatusOK)
}

func handleCommand(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" && r.Method != "DELETE" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	var req struct {
		Command string `json:"command"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	args := parseCommand(req.Command)
	if len(args) == 0 || args[0] != "container" {
		http.Error(w, "invalid command", http.StatusBadRequest)
		return
	}

	_, err := runCli(args[1:]...)
	if err != nil {
		// For DELETE images, we still might want graceful 204.
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	w.WriteHeader(http.StatusOK)
}

// handleVolumes removed - replaced by handleCommand

func handleSystemAction(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	action := strings.TrimPrefix(r.URL.Path, "/api/system/")
	var output []byte
	var err error

	switch action {
	case "start":
		output, err = runCli("system", "start")
	case "stop":
		output, err = runCli("system", "stop")
	case "prune":
		output, err = runCli("system", "prune", "--force")
	default:
		http.Error(w, "invalid system action", http.StatusBadRequest)
		return
	}

	if err != nil {
		http.Error(w, string(output), http.StatusInternalServerError)
		return
	}
	w.WriteHeader(http.StatusOK)
}

// handleManageImages removed - replaced by handleCommand
// handleSaveImage removed - replaced by handleCommand
// handleLoadImage removed - replaced by handleCommand
// handleTagImage removed - replaced by handleCommand

// handleRunCommand removed and replaced by handleRunContainer in run.go

// handleCreateContainer removed and moved to create.go

func handleActionLogs(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case "GET":
		w.Header().Set("Content-Type", "application/json")
		if actionLogs == nil {
			json.NewEncoder(w).Encode([]CommandLog{})
			return
		}
		json.NewEncoder(w).Encode(actionLogs)
	case "DELETE":
		actionLogs = []CommandLog{}
		w.WriteHeader(http.StatusOK)
	}
}

// handleBuilder removed - replaced by handleCommand

func handleListRegistries(w http.ResponseWriter, _ *http.Request) {
	output, err := runCli("registry", "list", "--format", "json")
	if err != nil {
		outStr := string(output)
		if strings.Contains(outStr, "container system start") ||
			strings.Contains(outStr, "XPC connection error") ||
			strings.Contains(outStr, "Connection invalid") {
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		}
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleRegistry(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		handleListRegistries(w, r)
	}
}

func handleRegistryLogin(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	var req struct {
		Server   string `json:"server"`
		Username string `json:"username"`
		Password string `json:"password"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	// container registry login --username <user> --password-stdin <server>
	cmd := exec.Command("container", "registry", "login", "--username", req.Username, "--password-stdin", req.Server)
	cmd.Stdin = strings.NewReader(req.Password + "\n")
	output, err := cmd.CombinedOutput()
	if err != nil {
		http.Error(w, fmt.Sprintf("Login Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.WriteHeader(http.StatusOK)
}

// handleRegistryLogout removed - replaced by handleCommand

func handleListSystemProperties(w http.ResponseWriter, r *http.Request) {
	output, err := runCli("system", "property", "list", "--format", "json")
	if err != nil {
		outStr := string(output)
		if strings.Contains(outStr, "container system start") ||
			strings.Contains(outStr, "XPC connection error") ||
			strings.Contains(outStr, "Connection invalid") {
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		}
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write(output)
}

func handleSystemProperties(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		handleListSystemProperties(w, r)
	}
}

// handleSetSystemProperty removed - replaced by handleCommand

// handleClearSystemProperty removed - replaced by handleCommand

func handleLogs(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/logs/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}
	lines := r.URL.Query().Get("n")
	if lines == "" {
		lines = "100"
	}

	output, err := runCli("logs", "-n", lines, id)
	if err != nil {
		http.Error(w, fmt.Sprintf("CLI Error: %v\nOutput: %s", err, string(output)), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "text/plain")
	w.Write(output)
}

func handleLogsSSE(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/logs/stream/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "streaming not supported", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	// Use container logs -f to follow logs
	cmd := exec.Command("container", "logs", "-f", id)
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		http.Error(w, fmt.Sprintf("Error creating stdout pipe: %v", err), http.StatusInternalServerError)
		return
	}
	cmd.Stderr = cmd.Stdout

	if err := cmd.Start(); err != nil {
		http.Error(w, fmt.Sprintf("Error starting logs: %v", err), http.StatusInternalServerError)
		return
	}
	defer cmd.Process.Kill()

	// Handle client disconnect
	notify := r.Context().Done()
	go func() {
		<-notify
		cmd.Process.Kill()
	}()

	scanner := bufio.NewScanner(stdout)
	for scanner.Scan() {
		line := scanner.Text()
		fmt.Fprintf(w, "data: %s\n\n", line)
		flusher.Flush()
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(w, "event: error\ndata: %v\n\n", err)
		flusher.Flush()
	}
}

func handleExec(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/exec/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	var req struct {
		Command string `json:"command"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	args := []string{"exec", id}
	args = append(args, strings.Fields(req.Command)...)

	cmd := exec.Command("container", args...)
	stdout, err := cmd.Output()

	resp := struct {
		Stdout string `json:"stdout"`
		Stderr string `json:"stderr"`
		Error  string `json:"error,omitempty"`
	}{
		Stdout: string(stdout),
	}

	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			resp.Stderr = string(exitErr.Stderr)
		}
		resp.Error = err.Error()
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func handleExecStream(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/exec-stream/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	var req struct {
		Command string `json:"command"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	w.Header().Set("Content-Type", "application/x-ndjson")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "Streaming not supported", http.StatusInternalServerError)
		return
	}

	args := []string{"exec", id}
	args = append(args, strings.Fields(req.Command)...)
	cmd := exec.Command("container", args...)

	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()

	if err := cmd.Start(); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	sendChunk := func(typ, text string) {
		resp := struct {
			Type string `json:"type"`
			Text string `json:"text"`
		}{Type: typ, Text: text}
		json.NewEncoder(w).Encode(resp)
		fmt.Fprint(w, "\n")
		flusher.Flush()
	}

	// Stream stdout in a goroutine
	done := make(chan bool)
	go func() {
		scanner := bufio.NewScanner(stdout)
		for scanner.Scan() {
			sendChunk("out", scanner.Text())
		}
		done <- true
	}()

	// Stream stderr
	scannerErr := bufio.NewScanner(stderr)
	for scannerErr.Scan() {
		sendChunk("err", scannerErr.Text())
	}

	<-done
	if err := cmd.Wait(); err != nil {
		sendChunk("err", fmt.Sprintf("Command exited with error: %v", err))
	} else {
		sendChunk("sys", "Command finished")
	}
}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // In a real app, restrict this
	},
}

func handleTerminalWS(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/containers/ws/")
	if id == "" {
		http.Error(w, "missing container id", http.StatusBadRequest)
		return
	}

	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("Websocket upgrade failed: %v", err)
		return
	}
	defer conn.Close()

	// Use /bin/sh by default. In the future, this could be configurable.
	cmd := exec.Command("container", "exec", "-it", id, "/bin/sh")

	f, err := pty.Start(cmd)
	if err != nil {
		conn.WriteMessage(websocket.TextMessage, []byte(fmt.Sprintf("\r\n[Cider] Error starting PTY: %v\r\n", err)))
		return
	}
	defer f.Close()

	// Bridge PTY to WebSocket
	go func() {
		buf := make([]byte, 1024)
		for {
			n, err := f.Read(buf)
			if err != nil {
				conn.WriteMessage(websocket.TextMessage, []byte("\r\n[Cider] PTY closed\r\n"))
				return
			}
			if err := conn.WriteMessage(websocket.BinaryMessage, buf[:n]); err != nil {
				return
			}
		}
	}()

	// Bridge WebSocket to PTY
	for {
		mt, data, err := conn.ReadMessage()
		if err != nil {
			break
		}

		if mt == websocket.TextMessage {
			// Check for resize command
			var msg struct {
				Type string `json:"type"`
				Rows uint16 `json:"rows"`
				Cols uint16 `json:"cols"`
			}
			if err := json.Unmarshal(data, &msg); err == nil && msg.Type == "resize" {
				pty.Setsize(f, &pty.Winsize{Rows: msg.Rows, Cols: msg.Cols})
				continue
			}
		}

		if _, err := f.Write(data); err != nil {
			break
		}
	}

	cmd.Process.Kill()
	cmd.Wait()
}

// handleVolumes removed - replaced by inline mux handler

// handleNetworks removed - replaced by inline mux handler

// handleDns removed - replaced by inline mux handler

func handleFindDockerfiles(w http.ResponseWriter, r *http.Request) {
	baseDir := r.URL.Query().Get("baseDir")
	if baseDir == "" {
		cwd, _ := os.Getwd()
		baseDir = cwd
	}

	// If it's relative, make it absolute based on CWD
	if !filepath.IsAbs(baseDir) {
		cwd, _ := os.Getwd()
		baseDir = filepath.Join(cwd, baseDir)
	}

	type DockerfileInfo struct {
		Path string `json:"path"`
		Name string `json:"name"`
	}
	results := []DockerfileInfo{}

	filepath.Walk(baseDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return nil // ignore errors
		}
		if info.IsDir() {
			if info.Name() == "node_modules" || strings.HasPrefix(info.Name(), ".") {
				return filepath.SkipDir
			}
			return nil
		}

		name := info.Name()
		if name == "Dockerfile" || name == "Containerfile" || strings.HasSuffix(name, ".Dockerfile") {
			rel, _ := filepath.Rel(baseDir, path)
			results = append(results, DockerfileInfo{
				Path: path,
				Name: rel,
			})
		}
		return nil
	})

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(results)
}

func handleReadFile(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Query().Get("path")
	if path == "" {
		http.Error(w, "missing path", http.StatusBadRequest)
		return
	}

	content, err := ioutil.ReadFile(path)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/plain")
	w.Write(content)
}

func handleComposeUp(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Path        string `json:"path"`
		ProjectName string `json:"projectName"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	cf, err := parseComposeFile(req.Path)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	projectDir := filepath.Dir(req.Path)
	if err := cf.composeUp(projectDir, req.ProjectName); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusOK)
}

func handleComposeDown(w http.ResponseWriter, r *http.Request) {
	var req struct {
		ProjectName string `json:"projectName"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	if err := composeDown(req.ProjectName); err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusOK)
}

func handleComposeStatus(w http.ResponseWriter, r *http.Request) {
	projectName := r.URL.Query().Get("project")
	if projectName == "" {
		http.Error(w, "missing project query param", http.StatusBadRequest)
		return
	}

	status, err := composeStatus(projectName)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(status)
}

func handleComposeParse(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Path string `json:"path"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	cf, err := parseComposeFile(req.Path)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(cf)
}

func handleFindComposeFiles(w http.ResponseWriter, r *http.Request) {
	baseDir := r.URL.Query().Get("baseDir")
	if baseDir == "" {
		cwd, _ := os.Getwd()
		baseDir = cwd
	}

	if !filepath.IsAbs(baseDir) {
		cwd, _ := os.Getwd()
		baseDir = filepath.Join(cwd, baseDir)
	}

	type ComposeFileInfo struct {
		Path string `json:"path"`
		Name string `json:"name"`
	}
	results := []ComposeFileInfo{}

	filepath.Walk(baseDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return nil
		}
		if info.IsDir() {
			if info.Name() == "node_modules" || strings.HasPrefix(info.Name(), ".") {
				return filepath.SkipDir
			}
			return nil
		}

		name := info.Name()
		if name == "docker-compose.yml" || name == "docker-compose.yaml" || name == "compose.yml" || name == "compose.yaml" {
			rel, _ := filepath.Rel(baseDir, path)
			results = append(results, ComposeFileInfo{
				Path: path,
				Name: rel,
			})
		}
		return nil
	})

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(results)
}

func main() {
	mux := http.NewServeMux()

	go logBroker.Start()

	mux.HandleFunc("/api/status", handleStatus)
	mux.HandleFunc("/api/containers/ws/", handleTerminalWS)
	mux.HandleFunc("/api/containers/exec-stream/", handleExecStream)
	mux.HandleFunc("/api/containers/exec/", handleExec)
	mux.HandleFunc("/api/containers/logs/stream/", handleLogsSSE)
	mux.HandleFunc("/api/containers/logs/", handleLogs)
	mux.HandleFunc("/api/command", handleCommand)
	mux.HandleFunc("/api/containers", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			handleListContainers(w, r)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/images", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			handleListImages(w, r)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/system/", handleSystemAction)
	mux.HandleFunc("/api/logs", handleActionLogs)
	mux.HandleFunc("/api/stats", handleGetStats)
	mux.HandleFunc("/api/logs/stream", logBroker.ServeHTTP)
	mux.HandleFunc("/api/registry", handleRegistry)
	mux.HandleFunc("/api/registry/login", handleRegistryLogin)
	mux.HandleFunc("/api/system/properties", handleSystemProperties)
	mux.HandleFunc("/api/volumes", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			output, err := runCli("volume", "ls", "--format", "json")
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			w.Write(output)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/networks", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			output, err := runCli("network", "ls", "--format", "json")
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			w.Write(output)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/dns/", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			output, err := runCli("dns", "list", "--format", "json")
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			w.Write(output)
		} else if r.Method == "DELETE" {
			domain := strings.TrimPrefix(r.URL.Path, "/api/dns/")
			if domain == "" {
				http.Error(w, "missing domain", http.StatusBadRequest)
				return
			}
			output, err := runCli("dns", "delete", domain)
			if err != nil {
				http.Error(w, string(output), http.StatusInternalServerError)
				return
			}
			w.WriteHeader(http.StatusOK)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/dns", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			output, err := runCli("dns", "list", "--format", "json")
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			w.Write(output)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/builder/", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "GET" {
			output, err := runCli("builder", "status", "--format", "json")
			if err != nil {
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			w.Write(output)
		} else {
			handleCommand(w, r)
		}
	})
	mux.HandleFunc("/api/dockerfiles", handleFindDockerfiles)
	mux.HandleFunc("/api/files/read", handleReadFile)
	mux.HandleFunc("/api/compose/up", handleComposeUp)
	mux.HandleFunc("/api/compose/down", handleComposeDown)
	mux.HandleFunc("/api/compose/status", handleComposeStatus)
	mux.HandleFunc("/api/compose/parse", handleComposeParse)
	mux.HandleFunc("/api/compose/discover", handleFindComposeFiles)

	// Actions like start/stop
	mux.HandleFunc("/api/containers/", func(w http.ResponseWriter, r *http.Request) {
		parts := strings.Split(strings.Trim(r.URL.Path, "/"), "/")
		if len(parts) == 4 {
			handleContainerAction(w, r)
		} else if r.Method == "DELETE" {
			handleRemoveContainer(w, r)
		} else {
			http.NotFound(w, r)
		}
	})

	// Static UI and SPA support
	staticFS, _ := fs.Sub(staticFiles, "dist")
	fileServer := http.FileServer(http.FS(staticFS))

	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		// If requesting an API route, don't serve static files here
		// The specific API handlers are already registered on the mux
		if strings.HasPrefix(r.URL.Path, "/api") {
			http.NotFound(w, r)
			return
		}

		// Try to find the file
		path := strings.TrimPrefix(r.URL.Path, "/")
		if path == "" {
			path = "index.html"
		}

		_, err := staticFS.Open(path)
		if err != nil {
			// File not found, serve index.html for SPA routing
			r.URL.Path = "/"
		}

		fileServer.ServeHTTP(w, r)
	})

	fmt.Println("🍎 Cider Backend starting on :7777...")
	log.Fatal(http.ListenAndServe(":7777", enableCors(mux)))
}
