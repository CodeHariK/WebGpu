import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import {
    checkSystemStatus,
    listContainers,
    startContainer,
    stopContainer,
    removeContainer,
    listImages,
    getActionLogs,
    getContainerStats,
} from "../lib/container";
import type { ContainerInfo, ImageInfo } from "../lib/container";
import CreateContainerModal from "./CreateContainerModal";
import {
    Play,
    Square,
    Trash2,
    RefreshCw,
    Server,
    Box,
    Layers,
    FileCode,
    Database,
    Globe,
    Key,
    Settings
} from "lucide-react";
import "../Dashboard.css";

export default function Dashboard() {
    const [systemRunning, setSystemRunning] = useState<boolean>(false);
    const [containers, setContainers] = useState<ContainerInfo[]>([]);
    const [stats, setStats] = useState<Record<string, any>>({});
    const [pollingInterval, setPollingInterval] = useState<number>(60000); // 1 min default
    const [isLoading, setIsLoading] = useState<boolean>(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [images, setImages] = useState<ImageInfo[]>([]);
    const [showCreateModalForImage, setShowCreateModalForImage] = useState<string | null>(null);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const [cList, iList, , sList] = await Promise.all([
                    listContainers(),
                    listImages(),
                    getActionLogs(),
                    getContainerStats()
                ]);
                setContainers(cList);
                setImages(iList);

                const sMap: Record<string, any> = {};
                if (sList && Array.isArray(sList)) {
                    sList.forEach(s => sMap[s.id] = s);
                }
                setStats(sMap);
            } else {
                setContainers([]);
                setImages([]);
                setStats({});
            }
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        const stored = localStorage.getItem("refreshInterval");
        if (stored !== null) {
            setPollingInterval(parseInt(stored, 10));
        }
    }, []);

    useEffect(() => {
        refreshData();
        if (pollingInterval > 0) {
            const id = setInterval(refreshData, pollingInterval);
            return () => clearInterval(id);
        }
    }, [pollingInterval]);

    const handleContainerAction = async (id: string, action: "start" | "stop" | "remove") => {
        setActionLoading(id + action);
        try {
            if (action === "start") await startContainer(id);
            if (action === "stop") await stopContainer(id);
            if (action === "remove") {
                if (confirm("Are you sure you want to remove this container?")) {
                    await removeContainer(id);
                } else {
                    return;
                }
            }
            await refreshData();
        } catch (e) {
            alert("Action failed: " + e);
        } finally {
            setActionLoading(null);
        }
    };

    return (
        <div className="dashboard-container animate-fade-in">
            {/* Header */}
            <header className="dashboard-header flex-between mb-6">
                <div className="flex-center" style={{ gap: "12px" }}>
                    <div className="logo-icon flex-center">
                        <Server size={24} color="white" />
                    </div>
                    <div>
                        <h1 style={{ fontSize: "24px", fontWeight: 700, letterSpacing: "-0.5px" }}>Cider</h1>
                        <p style={{ fontSize: "12px", color: "var(--text-secondary)", marginTop: "-2px" }}>Apple Container Management</p>
                    </div>
                </div>

                <nav className="flex-center" style={{ gap: "12px" }}>
                    <Link to="/images" className="btn btn-ghost p-2" title="Images">
                        <Layers size={20} />
                    </Link>
                    <Link to="/volumes" className="btn btn-ghost p-2" title="Volumes">
                        <Database size={20} />
                    </Link>
                    <Link to="/networks" className="btn btn-ghost p-2" title="Networks">
                        <Globe size={20} />
                    </Link>
                    <Link to="/dockerfiles" className="btn btn-ghost p-2" title="Dockerfiles">
                        <FileCode size={20} />
                    </Link>
                    <Link to="/registries" className="btn btn-ghost p-2" title="Registries">
                        <Key size={20} />
                    </Link>
                    <Link to="/settings" className="btn btn-ghost p-2" title="Settings">
                        <Settings size={20} />
                    </Link>

                    <div className="h-6 w-px bg-white/10 mx-2" />

                    <div className={`system-status-pill flex-center ${systemRunning ? "running" : "stopped"}`}>
                        <span className="pulse-dot" />
                        {systemRunning ? "Online" : "Offline"}
                    </div>
                    <button className="btn btn-primary p-2" onClick={refreshData} disabled={isLoading}>
                        <RefreshCw size={18} className={isLoading ? "spin" : ""} />
                    </button>
                </nav>
            </header>

            {/* Main Grid */}
            <div className="dashboard-grid">
                {/* Containers List */}
                <section className="premium-card span-8">
                    <div className="section-header flex-between mb-4">
                        <div className="flex-center" style={{ gap: "8px" }}>
                            <Box size={20} color="var(--accent-primary)" />
                            <h2 style={{ fontSize: "18px" }}>Containers</h2>
                            <span className="badge">{containers.length}</span>
                        </div>
                        {!systemRunning && (
                            <p style={{ color: "var(--text-warning)", fontSize: "13px" }}>Start system to manage containers</p>
                        )}
                    </div>

                    <div className="table-wrapper">
                        <table className="premium-table">
                            <thead>
                                <tr>
                                    <th>Status</th>
                                    <th>Name</th>
                                    <th>Image</th>
                                    <th>CPU / Mem</th>
                                    <th>Ports</th>
                                    <th style={{ textAlign: "right" }}>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {containers.length === 0 ? (
                                    <tr>
                                        <td colSpan={6} style={{ textAlign: "center", padding: "40px", color: "var(--text-secondary)" }}>
                                            {isLoading ? "Loading containers..." : "No containers found."}
                                        </td>
                                    </tr>
                                ) : (
                                    containers.map(container => (
                                        <tr key={container.ID} className="container-row">
                                            <td>
                                                <div className="flex-center" style={{ justifyContent: "flex-start", gap: "8px" }}>
                                                    <span className={`status-indicator status-${container.State}`} />
                                                    <span style={{ fontSize: "12px", textTransform: "capitalize" }}>{container.State}</span>
                                                </div>
                                            </td>
                                            <td style={{ fontWeight: 500 }}>
                                                <Link to={`/container/${container.ID}`} style={{ color: "var(--text-primary)", textDecoration: "none" }}>
                                                    {container.Names || container.ID?.substring(0, 8) || "Unknown"}
                                                </Link>
                                            </td>
                                            <td style={{ color: "var(--text-secondary)" }}>{container.Image}</td>
                                            <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                                                {stats[container.ID] ? (
                                                    `${Math.round(stats[container.ID].cpuUsageUsec / 1000)}ms / ${(stats[container.ID].memoryUsageBytes / 1024 / 1024).toFixed(1)}MB`
                                                ) : "-"}
                                            </td>
                                            <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                                                {Array.isArray(container.Ports) ? container.Ports.map((p: any) => p.hostPort).join(", ") : "-"}
                                            </td>
                                            <td style={{ textAlign: "right" }}>
                                                <div className="action-buttons">
                                                    {container.State === "running" ? (
                                                        <button className="btn-icon text-warning" onClick={() => handleContainerAction(container.ID, "stop")} disabled={actionLoading === container.ID + "stop"}>
                                                            {actionLoading === container.ID + "stop" ? <RefreshCw size={16} className="spin" /> : <Square size={16} />}
                                                        </button>
                                                    ) : (
                                                        <button className="btn-icon text-success" onClick={() => handleContainerAction(container.ID, "start")} disabled={actionLoading === container.ID + "start"}>
                                                            {actionLoading === container.ID + "start" ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                                                        </button>
                                                    )}
                                                    <button className="btn-icon text-danger" onClick={() => handleContainerAction(container.ID, "remove")} disabled={actionLoading === container.ID + "remove"}>
                                                        <Trash2 size={16} />
                                                    </button>
                                                </div>
                                            </td>
                                        </tr>
                                    ))
                                )}
                            </tbody>
                        </table>
                    </div>
                </section>

                {/* Images List */}
                <section className="premium-card span-4">
                    <div className="section-header flex-between mb-4">
                        <div className="flex-center" style={{ gap: "8px" }}>
                            <Layers size={20} color="var(--accent-secondary)" />
                            <h2 style={{ fontSize: "18px" }}>Images</h2>
                        </div>
                    </div>

                    <div className="image-list" style={{ maxHeight: "400px", overflowY: "auto" }}>
                        {images.map((image, idx) => (
                            <div key={image.ID || `img-${idx}`} className="image-item flex-between p-3 mb-2" style={{ background: "rgba(255,255,255,0.03)", borderRadius: "8px" }}>
                                <div>
                                    <div style={{ fontWeight: 600, fontSize: "14px" }}>{image.Repository}</div>
                                    <div style={{ fontSize: "12px", color: "var(--text-secondary)" }}>{image.Tag} • {image.Size}</div>
                                </div>
                                <button className="btn btn-secondary" style={{ padding: "6px" }} onClick={() => setShowCreateModalForImage(image.Repository + ":" + image.Tag)}>
                                    <Play size={14} />
                                </button>
                            </div>
                        ))}
                    </div>
                </section>
            </div>

            {showCreateModalForImage && (
                <CreateContainerModal
                    imageName={showCreateModalForImage}
                    onClose={() => setShowCreateModalForImage(null)}
                    onCreate={async (_config) => {
                        // Handle create
                        setShowCreateModalForImage(null);
                        await refreshData();
                    }}
                    isCreating={false}
                />
            )}
        </div>
    );
}
