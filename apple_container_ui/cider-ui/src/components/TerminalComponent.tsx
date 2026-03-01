import { useEffect, useRef } from "react";
import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import "@xterm/xterm/css/xterm.css";

interface TerminalComponentProps {
    containerId: string;
    isActive?: boolean;
}

export default function TerminalComponent({ containerId, isActive }: TerminalComponentProps) {
    const terminalRef = useRef<HTMLDivElement>(null);
    const xtermRef = useRef<Terminal | null>(null);
    const wsRef = useRef<WebSocket | null>(null);
    const fitAddonRef = useRef<FitAddon | null>(null);

    useEffect(() => {
        if (!terminalRef.current) return;

        // Initialize xterm.js
        const term = new Terminal({
            cursorBlink: true,
            fontSize: 13,
            fontFamily: 'Menlo, Monaco, "Courier New", monospace',
            theme: {
                background: "transparent",
                foreground: "#E0E0E0",
                cursor: "#FFFFFF",
                selectionBackground: "rgba(255, 255, 255, 0.3)",
                black: "#000000",
                red: "#FF5252",
                green: "#4CAF50",
                yellow: "#FFD740",
                blue: "#448AFF",
                magenta: "#FF4081",
                cyan: "#18FFFF",
                white: "#FFFFFF",
                brightBlack: "#757575",
                brightRed: "#FF8A80",
                brightGreen: "#B9F6CA",
                brightYellow: "#FFE57F",
                brightBlue: "#82B1FF",
                brightMagenta: "#FF80AB",
                brightCyan: "#84FFFF",
                brightWhite: "#FFFFFF",
            },
        });

        const fitAddon = new FitAddon();
        term.loadAddon(fitAddon);
        term.open(terminalRef.current);
        fitAddon.fit();

        xtermRef.current = term;
        fitAddonRef.current = fitAddon;

        // WebSocket connection
        const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
        const host = window.location.host; // This handles both localhost:7777 and cider dashboard
        const wsUrl = `${protocol}//${host}/api/containers/ws/${containerId}`;

        const socket = new WebSocket(wsUrl);
        socket.binaryType = "arraybuffer";
        wsRef.current = socket;

        socket.onopen = () => {
            term.reset();
            // Send initial resize
            const dims = fitAddon.proposeDimensions();
            if (dims) {
                socket.send(JSON.stringify({
                    type: "resize",
                    cols: dims.cols,
                    rows: dims.rows
                }));
            }
        };

        socket.onmessage = (event) => {
            if (event.data instanceof ArrayBuffer) {
                term.write(new Uint8Array(event.data));
            } else {
                term.write(event.data);
            }
        };

        socket.onclose = () => {
            term.write("\r\n[Cider] Connection closed\r\n");
        };

        socket.onerror = () => {
            term.write("\r\n[Cider] WebSocket error\r\n");
        };

        // Terminal -> WebSocket
        term.onData((data) => {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send(data);
            }
        });

        // Resize listener
        const handleResize = () => {
            fitAddon.fit();
            const dims = fitAddon.proposeDimensions();
            if (dims && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify({
                    type: "resize",
                    cols: dims.cols,
                    rows: dims.rows
                }));
            }
        };

        window.addEventListener("resize", handleResize);

        return () => {
            window.removeEventListener("resize", handleResize);
            socket.close();
            term.dispose();
        };
    }, [containerId]);

    // Handle focus and fit when tab becomes active
    useEffect(() => {
        if (isActive && xtermRef.current && fitAddonRef.current) {
            // Delay slightly to ensure visibility
            setTimeout(() => {
                fitAddonRef.current?.fit();
                xtermRef.current?.focus();
            }, 100);
        }
    }, [isActive]);

    return (
        <div
            ref={terminalRef}
            className="xterm-container"
            style={{
                height: "400px",
                width: "100%",
                background: "rgba(0,0,0,0.3)",
                borderRadius: "8px",
                padding: "8px",
                overflow: "hidden"
            }}
        />
    );
}
