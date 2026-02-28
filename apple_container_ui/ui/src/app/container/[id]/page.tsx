"use client";

import { useState, useEffect, useRef, use } from "react";
import { execContainer, inspectContainer, listContainers, ContainerInfo, getContainerLogs } from "@/lib/container";
import {
    Terminal,
    Folder,
    File,
    ArrowLeft,
    RefreshCw,
    Play,
    CornerLeftUp,
    Info,
    AlignLeft
} from "lucide-react";
import Link from "next/link";
import "./ContainerDetails.css";

export default function ContainerDetailsPage({ params }: { params: Promise<{ id: string }> }) {
    const unwrappedParams = use(params);
    const { id } = unwrappedParams;

    const [containerName, setContainerName] = useState<string>("Loading...");
    const [isRunning, setIsRunning] = useState<boolean>(true);

    const [containerInfo, setContainerInfo] = useState<ContainerInfo | null>(null);

    // Tabs: 'info', 'terminal', 'logs' or 'files'
    const [activeTab, setActiveTab] = useState<"info" | "terminal" | "logs" | "files">("info");

    const [logs, setLogs] = useState<string>("");
    const [isLoadingLogs, setIsLoadingLogs] = useState<boolean>(false);

    // Terminal State
    const [termCommand, setTermCommand] = useState<string>("");
    const [termOutput, setTermOutput] = useState<{ id: number, type: "cmd" | "out" | "err" | "sys", text: string }[]>([]);
    const [isExecuting, setIsExecuting] = useState(false);
    const endRef = useRef<HTMLDivElement>(null);

    // Filesystem State
    const [currentPath, setCurrentPath] = useState<string>("/");
    const [files, setFiles] = useState<{ name: string, isDir: boolean, size: string, permissions: string }[]>([]);
    const [isLoadingFiles, setIsLoadingFiles] = useState(false);

    useEffect(() => {
        // Determine context
        const init = async () => {
            try {
                const cList = await listContainers();
                const me = cList.find(c => c.ID === id || c.ID.startsWith(id));
                if (me) {
                    setContainerInfo(me);
                    setContainerName(me.Names || me.ID.substring(0, 8));
                    if (me.State !== "running") setIsRunning(false);
                } else {
                    setContainerName(id.substring(0, 8));
                }
            } catch (e) {
                console.error(e);
            }
        };
        init();
        appendLog("sys", `Connected to container ${id}`);
        appendLog("sys", "Type a command and press Enter.");
    }, [id]);

    useEffect(() => {
        endRef.current?.scrollIntoView({ behavior: "smooth" });
    }, [termOutput]);

    useEffect(() => {
        if (activeTab === "files" && isRunning) {
            loadFiles(currentPath);
        } else if (activeTab === "logs") {
            fetchLogs();
        }
    }, [activeTab, currentPath, isRunning]);

    const fetchLogs = async () => {
        setIsLoadingLogs(true);
        try {
            const out = await getContainerLogs(id, 200);
            setLogs(out || "No logs available.");
        } catch (e: any) {
            setLogs(e.message || "Error fetching logs");
        } finally {
            setIsLoadingLogs(false);
        }
    };

    const appendLog = (type: "cmd" | "out" | "err" | "sys", text: string) => {
        if (!text.trim() && type !== "cmd") return;
        setTermOutput(prev => [...prev, { id: Date.now() + Math.random(), type, text }]);
    };

    const executeCommand = async (e: React.FormEvent) => {
        e.preventDefault();
        if (!termCommand.trim() || isExecuting) return;

        const cmd = termCommand;
        setTermCommand("");
        appendLog("cmd", cmd);

        setIsExecuting(true);
        try {
            const { stdout, stderr, error } = await execContainer(id, cmd);
            if (stdout) appendLog("out", stdout);
            if (stderr) appendLog("err", stderr);
            if (error) appendLog("err", `Error: ${error}`);
        } catch (err: any) {
            appendLog("err", err.message);
        } finally {
            setIsExecuting(false);
        }
    };

    const loadFiles = async (path: string) => {
        setIsLoadingFiles(true);
        try {
            const { stdout, error } = await execContainer(id, `ls -la ${path}`);
            if (error) {
                setFiles([]);
                return;
            }

            const lines = stdout.split('\n').slice(1); // skip total
            const parsedFiles = lines.map(line => {
                const parts = line.split(/\s+/);
                if (parts.length < 9) return null;

                const permissions = parts[0];
                const isDir = permissions.startsWith('d');
                const size = parts[4];
                const name = parts.slice(8).join(' '); // Rejoin in case of spaces

                if (name === '.' || name === '..') return null;

                return { name, isDir, size, permissions };
            }).filter(Boolean) as any[];

            // Sort dirs first, then files
            parsedFiles.sort((a, b) => {
                if (a.isDir === b.isDir) return a.name.localeCompare(b.name);
                return a.isDir ? -1 : 1;
            });

            setFiles(parsedFiles);
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoadingFiles(false);
        }
    };

    const navigateTo = (dirName: string) => {
        if (currentPath.endsWith('/')) {
            setCurrentPath(currentPath + dirName);
        } else {
            setCurrentPath(currentPath + '/' + dirName);
        }
    };

    const navigateUp = () => {
        if (currentPath === '/') return;
        const parts = currentPath.split('/').filter(Boolean);
        parts.pop();
        setCurrentPath('/' + parts.join('/'));
    };

    return (
        <div className="dashboard-container">
            <header className="premium-card flex-between mb-4" style={{ padding: "16px 24px" }}>
                <div className="flex-center" style={{ gap: "16px" }}>
                    <Link href="/" className="btn-icon" style={{ background: "rgba(255,255,255,0.05)" }}>
                        <ArrowLeft size={20} />
                    </Link>
                    <div>
                        <h1 style={{ fontSize: "20px", display: "flex", alignItems: "center", gap: "12px" }}>
                            {containerName}
                            <span className={`status-indicator status-${isRunning ? 'running' : 'stopped'}`} />
                        </h1>
                        <p style={{ color: "var(--text-secondary)", fontSize: "13px", marginTop: "4px" }}>{id}</p>
                    </div>
                </div>
            </header>

            {!isRunning && (
                <div className="premium-card" style={{ borderColor: "var(--danger-color)", background: "rgba(255, 68, 68, 0.05)" }}>
                    <h2 style={{ color: "var(--danger-color)" }}>Container is not running</h2>
                    <p style={{ color: "var(--text-secondary)", marginTop: "8px" }}>
                        The container must be running to inspect its filesystem or execute commands. Please start it from the dashboard.
                    </p>
                    <Link href="/" className="btn btn-primary" style={{ marginTop: "16px", display: "inline-block" }}>Return to Dashboard</Link>
                </div>
            )}

            {isRunning && (
                <div className="premium-card inspect-layout">
                    <div className="tabs flex-center mb-4" style={{ gap: "16px", borderBottom: "1px solid rgba(255,255,255,0.1)", paddingBottom: "16px" }}>
                        <button
                            className={`tab-btn ${activeTab === "info" ? "active" : ""}`}
                            onClick={() => setActiveTab("info")}
                        >
                            <Info size={18} /> Configuration
                        </button>
                        <button
                            className={`tab-btn ${activeTab === "terminal" ? "active" : ""}`}
                            onClick={() => setActiveTab("terminal")}
                        >
                            <Terminal size={18} /> Interactive Terminal
                        </button>
                        <button
                            className={`tab-btn ${activeTab === "logs" ? "active" : ""}`}
                            onClick={() => setActiveTab("logs")}
                        >
                            <AlignLeft size={18} /> Logs
                        </button>
                        <button
                            className={`tab-btn ${activeTab === "files" ? "active" : ""}`}
                            onClick={() => setActiveTab("files")}
                        >
                            <Folder size={18} /> Filesystem
                        </button>
                    </div>

                    {activeTab === "info" && containerInfo && (
                        <div className="info-container animate-fade-in" style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "24px" }}>
                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>General</h3>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Image:</strong> {containerInfo.Image}</p>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Status:</strong> {containerInfo.Status}</p>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>CPUs:</strong> {containerInfo.CPUs || "Unlimited"}</p>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Memory:</strong> {containerInfo.MemoryBytes ? (containerInfo.MemoryBytes / 1024 / 1024) + " MB" : "Unlimited"}</p>
                            </div>

                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>Network Ports</h3>
                                {Array.isArray(containerInfo.Ports) && containerInfo.Ports.length > 0 ? (
                                    <ul style={{ listStyle: "none", padding: 0, margin: 0 }}>
                                        {containerInfo.Ports.map((p, i) => (
                                            <li key={i} style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "4px" }}>
                                                {p.hostAddress || "0.0.0.0"}:{p.hostPort} &rarr; {p.containerPort}/{p.proto || "tcp"}
                                            </li>
                                        ))}
                                    </ul>
                                ) : <p style={{ fontSize: "13px", color: "var(--text-secondary)", fontStyle: "italic" }}>No published ports</p>}
                            </div>

                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>Environment Variables</h3>
                                {Array.isArray(containerInfo.Env) && containerInfo.Env.length > 0 ? (
                                    <div style={{ maxHeight: "150px", overflowY: "auto", background: "rgba(0,0,0,0.3)", padding: "8px", borderRadius: "4px" }}>
                                        {containerInfo.Env.map((e, i) => (
                                            <div key={i} style={{ fontSize: "12px", fontFamily: "monospace", color: "var(--text-secondary)", marginBottom: "4px", wordBreak: "break-all" }}>
                                                {e}
                                            </div>
                                        ))}
                                    </div>
                                ) : <p style={{ fontSize: "13px", color: "var(--text-secondary)", fontStyle: "italic" }}>No environment variables</p>}
                            </div>

                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>Bind Mounts / Volumes</h3>
                                {Array.isArray(containerInfo.Mounts) && containerInfo.Mounts.length > 0 ? (
                                    <ul style={{ listStyle: "none", padding: 0, margin: 0 }}>
                                        {containerInfo.Mounts.map((m, i) => (
                                            <li key={i} style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "6px", wordBreak: "break-all", background: "rgba(0,0,0,0.3)", padding: "6px", borderRadius: "4px" }}>
                                                <strong>Source:</strong> {m.source || m.Name}<br />
                                                <strong>Target:</strong> {m.destination || m.Destination}<br />
                                                <strong>Type:</strong> {m.type || "volume"}
                                            </li>
                                        ))}
                                    </ul>
                                ) : <p style={{ fontSize: "13px", color: "var(--text-secondary)", fontStyle: "italic" }}>No mounts configured</p>}
                            </div>
                        </div>
                    )}

                    {activeTab === "logs" && (
                        <div className="logs-container premium-card" style={{ padding: 0, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                            <div className="flex-between" style={{ padding: "12px 16px", borderBottom: "1px solid rgba(255,255,255,0.1)", background: "rgba(0,0,0,0.2)" }}>
                                <span style={{ fontSize: "14px", color: "var(--text-secondary)" }}>Recent Output (last 200 lines)</span>
                                <button className="btn-icon" onClick={fetchLogs} disabled={isLoadingLogs}>
                                    <RefreshCw size={16} className={isLoadingLogs ? "spin" : ""} />
                                </button>
                            </div>
                            <div style={{ background: "#000", padding: "16px", overflowY: "auto", maxHeight: "400px", fontFamily: "monospace", fontSize: "13px", whiteSpace: "pre-wrap", color: "var(--text-primary)" }}>
                                {logs}
                            </div>
                        </div>
                    )}

                    {activeTab === "terminal" && (
                        <div className="terminal-container">
                            <div className="terminal-output">
                                {termOutput.map((log) => (
                                    <div key={log.id} className={`term-line term-${log.type}`}>
                                        {log.type === "cmd" && <span className="term-prompt">$&nbsp;</span>}
                                        {log.type === "sys" && <span className="term-sys">[sys]&nbsp;</span>}
                                        {log.text}
                                    </div>
                                ))}
                                <div ref={endRef} />
                            </div>
                            <form onSubmit={executeCommand} className="terminal-input-group flex-center mt-4">
                                <span className="term-prompt">$&nbsp;</span>
                                <input
                                    type="text"
                                    value={termCommand}
                                    onChange={e => setTermCommand(e.target.value)}
                                    placeholder="Enter command (e.g., ls -la, apk add nano)"
                                    disabled={isExecuting}
                                    autoFocus
                                />
                                <button type="submit" className="btn btn-primary" disabled={isExecuting || !termCommand.trim()}>
                                    {isExecuting ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                                </button>
                            </form>
                        </div>
                    )}

                    {activeTab === "files" && (
                        <div className="files-container">
                            <div className="files-header flex-center mb-4" style={{ gap: "12px" }}>
                                <button className="btn-icon" onClick={navigateUp} disabled={currentPath === '/' || isLoadingFiles}>
                                    <CornerLeftUp size={18} />
                                </button>
                                <div className="current-path" style={{ flex: 1, padding: "8px 12px", background: "rgba(0,0,0,0.3)", borderRadius: "4px", fontFamily: "monospace", display: "flex", gap: "8px", alignItems: "center" }}>
                                    <Folder size={16} color="var(--accent-primary)" />
                                    {currentPath}
                                </div>
                                <button className="btn-icon" onClick={() => loadFiles(currentPath)} disabled={isLoadingFiles}>
                                    <RefreshCw size={18} className={isLoadingFiles ? "spin" : ""} />
                                </button>
                            </div>

                            <div className="table-responsive">
                                <table className="container-table">
                                    <thead>
                                        <tr>
                                            <th>Name</th>
                                            <th>Size</th>
                                            <th>Permissions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {files.map(f => (
                                            <tr key={f.name} className={`container-row ${f.isDir ? 'dir-row' : ''}`} onClick={() => f.isDir && navigateTo(f.name)} style={{ cursor: f.isDir ? 'pointer' : 'default' }}>
                                                <td className="flex-center" style={{ gap: "12px", justifyContent: "flex-start" }}>
                                                    {f.isDir ? <Folder size={18} color="var(--accent-primary)" /> : <File size={18} color="var(--text-secondary)" />}
                                                    <span style={{ color: f.isDir ? "var(--text-primary)" : "var(--text-secondary)" }}>{f.name}</span>
                                                </td>
                                                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{f.isDir ? "-" : f.size}</td>
                                                <td style={{ color: "var(--text-secondary)", fontSize: "13px", fontFamily: "monospace" }}>{f.permissions}</td>
                                            </tr>
                                        ))}
                                        {files.length === 0 && !isLoadingFiles && (
                                            <tr><td colSpan={3} style={{ textAlign: "center", padding: "24px", color: "var(--text-secondary)" }}>Directory is empty</td></tr>
                                        )}
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    )}
                </div>
            )}
        </div>
    );
}
