import { useState, useEffect, useRef } from "react";
import { useParams, Link } from "react-router-dom";
import {
    Terminal,
    Folder,
    File as FileIcon,
    ArrowLeft,
    RefreshCw,
    Play,
    CornerLeftUp,
    Info,
    AlignLeft
} from "lucide-react";
import {
    execContainer,
    listContainers,
    getContainerLogs,
    checkSystemStatus
} from "../lib/container";
import type {
    ContainerInfo
} from "../lib/container";
import "../components/ContainerDetails.css";
import "../Dashboard.css";

export default function ContainerDetailsPage() {
    const { id } = useParams<{ id: string }>();

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
        if (!id) return;

        const init = async () => {
            try {
                const isSystemRunning = await checkSystemStatus();
                if (!isSystemRunning) {
                    setContainerName("System Offline");
                    setIsRunning(false);
                    return;
                }

                const cList = await listContainers();
                const me = cList.find(c => c.ID === id || c.ID.startsWith(id));
                if (me) {
                    setContainerInfo(me);
                    setContainerName(me.Names || me.ID.substring(0, 8));
                    setIsRunning(me.State === "running");
                } else {
                    setContainerName(id.substring(0, 8));
                    setIsRunning(false);
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
        if (!id) return;
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
        if (!id || !termCommand.trim() || isExecuting) return;

        const cmd = termCommand;
        setTermCommand("");
        appendLog("cmd", cmd);

        setIsExecuting(true);
        try {
            const res = await execContainer(id, cmd);
            if (res.stdout) appendLog("out", res.stdout);
            if (res.stderr) appendLog("err", res.stderr);
            if (res.error) appendLog("err", `Error: ${res.error}`);
        } catch (err: any) {
            appendLog("err", err.message);
        } finally {
            setIsExecuting(false);
        }
    };

    const loadFiles = async (path: string) => {
        if (!id) return;
        setIsLoadingFiles(true);
        try {
            const res = await execContainer(id, `ls -la ${path}`);
            if (res.error) {
                setFiles([]);
                return;
            }

            const stdout = res.stdout || "";
            const lines = stdout.split('\n').slice(1); // skip total
            const parsedFiles = lines.map((line: string) => {
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
        <div className="dashboard-container" style={{ padding: '24px' }}>
            <header className="premium-card flex-between mb-4" style={{ padding: "16px 24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: "16px" }}>
                    <Link to="/" className="btn-icon" style={{ background: "rgba(255,255,255,0.05)", display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                        <ArrowLeft size={20} />
                    </Link>
                    <div>
                        <h1 style={{ fontSize: "20px", display: "flex", alignItems: "center", gap: "12px", margin: 0 }}>
                            {containerName}
                            <span className={`status-indicator status-${isRunning ? 'running' : 'stopped'}`} />
                        </h1>
                        <p style={{ color: "var(--text-secondary)", fontSize: "13px", marginTop: "4px", margin: 0 }}>{id}</p>
                    </div>
                </div>
            </header>

            {!isRunning && (
                <div className="premium-card" style={{ borderColor: "var(--danger-color)", background: "rgba(255, 68, 68, 0.05)", padding: '24px', textAlign: 'center' }}>
                    <h2 style={{ color: "var(--danger-color)", margin: 0 }}>Container is not running</h2>
                    <p style={{ color: "var(--text-secondary)", marginTop: "8px" }}>
                        The container must be running to inspect its filesystem or execute commands. Please start it from the dashboard.
                    </p>
                    <Link to="/" className="btn btn-primary" style={{ marginTop: "16px", display: "inline-block" }}>Return to Dashboard</Link>
                </div>
            )}

            {isRunning && (
                <div className="premium-card inspect-layout" style={{ padding: '24px' }}>
                    <div className="tabs flex-center mb-4" style={{ gap: "16px", borderBottom: "1px solid rgba(255,255,255,0.1)", paddingBottom: "16px", display: 'flex', alignItems: 'center', marginBottom: '24px' }}>
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
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px", marginTop: 0 }}>General</h3>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Image:</strong> {containerInfo.Image}</p>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Status:</strong> {containerInfo.Status}</p>
                                <p style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "8px" }}><strong style={{ color: "var(--text-primary)" }}>Created At:</strong> {containerInfo.CreatedAt || "Unknown"}</p>
                            </div>

                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px", marginTop: 0 }}>Network Ports</h3>
                                {Array.isArray(containerInfo.Ports) && containerInfo.Ports.length > 0 ? (
                                    <ul style={{ listStyle: "none", padding: 0, margin: 0 }}>
                                        {containerInfo.Ports.map((p: any, i: number) => (
                                            <li key={i} style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "4px" }}>
                                                {p.hostAddress || "0.0.0.0"}:{p.hostPort} &rarr; {p.containerPort}/{p.proto || "tcp"}
                                            </li>
                                        ))}
                                    </ul>
                                ) : <p style={{ fontSize: "13px", color: "var(--text-secondary)", fontStyle: "italic" }}>No published ports</p>}
                            </div>

                            <div className="premium-card" style={{ padding: "16px", background: "rgba(255,255,255,0.02)" }}>
                                <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px", marginTop: 0 }}>Network Interfaces</h3>
                                {Array.isArray(containerInfo.Networks) && containerInfo.Networks.length > 0 ? (
                                    <ul style={{ listStyle: "none", padding: 0, margin: 0 }}>
                                        {containerInfo.Networks.map((n: any, i: number) => (
                                            <li key={i} style={{ fontSize: "13px", color: "var(--text-secondary)", marginBottom: "12px", background: "rgba(0,0,0,0.3)", padding: "10px", borderRadius: "6px" }}>
                                                <div style={{ fontWeight: 600, color: "var(--text-primary)", marginBottom: "4px" }}>{n.network || "Interface " + (i + 1)}</div>
                                                <div style={{ display: 'grid', gridTemplateColumns: '80px 1fr', gap: '4px' }}>
                                                    <strong>IP:</strong> <span style={{ fontFamily: 'monospace' }}>{n.ipv4Address || n.ipv6Address || "-"}</span>
                                                    <strong>Gateway:</strong> <span style={{ fontFamily: 'monospace' }}>{n.ipv4Gateway || "-"}</span>
                                                    <strong>MAC:</strong> <span style={{ fontFamily: 'monospace' }}>{n.macAddress || "-"}</span>
                                                    <strong>Host:</strong> <span style={{ fontFamily: 'monospace' }}>{n.hostname || "-"}</span>
                                                </div>
                                            </li>
                                        ))}
                                    </ul>
                                ) : <p style={{ fontSize: "13px", color: "var(--text-secondary)", fontStyle: "italic" }}>No network details</p>}
                            </div>
                        </div>
                    )}

                    {activeTab === "logs" && (
                        <div className="logs-container premium-card" style={{ padding: 0, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: "12px 16px", borderBottom: "1px solid rgba(255,255,255,0.1)", background: "rgba(0,0,0,0.2)" }}>
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
                            <div className="terminal-output" style={{ minHeight: '300px' }}>
                                {termOutput.map((log) => (
                                    <div key={log.id} className={`term-line term-${log.type}`}>
                                        {log.type === "cmd" && <span className="term-prompt">$&nbsp;</span>}
                                        {log.type === "sys" && <span className="term-sys">[sys]&nbsp;</span>}
                                        {log.text}
                                    </div>
                                ))}
                                <div ref={endRef} />
                            </div>
                            <form onSubmit={executeCommand} className="terminal-input-group" style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                <span className="term-prompt">$&nbsp;</span>
                                <input
                                    type="text"
                                    value={termCommand}
                                    onChange={e => setTermCommand(e.target.value)}
                                    placeholder="Enter command (e.g., ls -la)"
                                    disabled={isExecuting}
                                    autoFocus
                                    style={{ flex: 1, background: 'transparent', border: 'none', color: 'inherit', outline: 'none', fontFamily: 'monospace' }}
                                />
                                <button type="submit" className="btn btn-primary" disabled={isExecuting || !termCommand.trim()}>
                                    {isExecuting ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                                </button>
                            </form>
                        </div>
                    )}

                    {activeTab === "files" && (
                        <div className="files-container">
                            <div className="files-header mb-4" style={{ display: 'flex', alignItems: 'center', gap: "12px", marginBottom: '16px' }}>
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
                                <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                            <th style={{ padding: '12px' }}>Name</th>
                                            <th style={{ padding: '12px' }}>Size</th>
                                            <th style={{ padding: '12px' }}>Permissions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {files.map(f => (
                                            <tr key={f.name} className={`container-row ${f.isDir ? 'dir-row' : ''}`} onClick={() => f.isDir && navigateTo(f.name)} style={{ cursor: f.isDir ? 'pointer' : 'default', borderBottom: '1px solid var(--border-color)' }}>
                                                <td style={{ padding: '12px', display: 'flex', alignItems: 'center', gap: "12px" }}>
                                                    {f.isDir ? <Folder size={18} color="var(--accent-primary)" /> : <FileIcon size={18} color="var(--text-secondary)" />}
                                                    <span style={{ color: f.isDir ? "var(--text-primary)" : "var(--text-secondary)" }}>{f.name}</span>
                                                </td>
                                                <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>{f.isDir ? "-" : f.size}</td>
                                                <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px", fontFamily: "monospace" }}>{f.permissions}</td>
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
