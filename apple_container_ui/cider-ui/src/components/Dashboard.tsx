"use client";

import React, { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import type {
    CommandLog
} from "../lib/container";
import {
    checkSystemStatus,
    startSystem,
    stopSystem,
    listContainers,
    startContainer,
    stopContainer,
    removeContainer,
    deleteImage,
    getContainerStats,
    getActionLogs,
    clearActionLogs,
    runContainer,
    createContainer,
    type ContainerInfo
} from "../lib/container";
import { AlertDialog } from "@base-ui/react/alert-dialog";
import { Toast } from "@base-ui/react/toast";
import CreateContainerModal from "../components/CreateContainerModal";
import {
    Activity,
    Box,
    Layers,
    Play,
    Power,
    RefreshCw,
    Terminal,
    Square,
    Trash2,
    ExternalLink,
    Network,
    Database,
    KeySquare,
    FileCode,
    Settings,
    AlertCircle
} from 'lucide-react';
import "../Dashboard.css";

const toastManager = Toast.createToastManager();

export default function Dashboard() {
    return (
        <Toast.Provider toastManager={toastManager}>
            <DashboardContent />
        </Toast.Provider>
    );
}

function DashboardContent() {
    const { toasts } = Toast.useToastManager();
    const [systemRunning, setSystemRunning] = useState<boolean>(false);
    const [containers, setContainers] = useState<ContainerInfo[]>([]);
    const [stats, setStats] = useState<Record<string, any>>({});
    const [pollingInterval, setPollingInterval] = useState<number>(60000); // 1 min default
    const [isLoading, setIsLoading] = useState<boolean>(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [logs, setLogs] = useState<CommandLog[]>([]);
    const [showCreateModalForImage, setShowCreateModalForImage] = useState<string | null>(null);
    const [groupByProject, setGroupByProject] = useState<boolean>(false);
    const [isPollingPaused, setIsPollingPaused] = useState<boolean>(false);
    const [confirmAction, setConfirmAction] = useState<{ id: string, name: string } | null>(null);
    const [confirmImageDelete, setConfirmImageDelete] = useState<string | null>(null);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);

            const logList = await getActionLogs();
            setLogs(logList);

            if (isRunning) {
                const [cList, sList] = await Promise.all([
                    listContainers(),
                    getContainerStats()
                ]);
                setContainers(cList);

                const sMap: Record<string, any> = {};
                if (sList && Array.isArray(sList)) {
                    sList.forEach(s => sMap[s.id] = s);
                }
                setStats(sMap);
            } else {
                setContainers([]);
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
        refreshData();
    }, []);

    useEffect(() => {
        const eventSource = new EventSource("/api/logs/stream");

        eventSource.onmessage = (event) => {
            try {
                const logEntry: CommandLog = JSON.parse(event.data);
                setLogs(prev => [logEntry, ...prev.slice(0, 99)]);

                if (!logEntry.isPartial) {
                    toastManager.add({
                        title: logEntry.isError ? "Command Failed" : "Command Successful",
                        description: logEntry.command,
                        type: logEntry.isError ? "error" : "success"
                    });
                    refreshData();
                }
            } catch (e) {
                console.error("Failed to parse SSE message", e);
            }
        };

        eventSource.onerror = (err) => {
            console.error("EventSource failed:", err);
            eventSource.close();
        };

        return () => {
            eventSource.close();
        };
    }, []);

    useEffect(() => {
        if (isPollingPaused) return;

        const interval = setInterval(() => {
            refreshData();
        }, pollingInterval);
        return () => clearInterval(interval);
    }, [pollingInterval, isPollingPaused]);

    const handleSystemToggle = async () => {
        setActionLoading("system");
        try {
            if (systemRunning) {
                await stopSystem();
            } else {
                await startSystem();
            }
            await new Promise(r => setTimeout(r, 2000));
            await refreshData();
        } catch (e) {
            alert("Failed to toggle system: " + e);
        } finally {
            setActionLoading(null);
        }
    };

    const handleContainerAction = async (id: string, action: "start" | "stop" | "remove") => {
        if (action === "remove") {
            const container = (containers as any).find((c: any) => c.ID === id);
            setConfirmAction({ id, name: container?.Names[0] || id });
            setIsPollingPaused(true);
            return;
        }

        setActionLoading(id + action);
        try {
            if (action === "start") await startContainer(`container start ${id}`);
            if (action === "stop") await stopContainer(id);
        } catch (e) {
            console.error(e);
            toastManager.add({
                title: "Action Failed",
                description: String(e),
                type: "error"
            });
        } finally {
            setActionLoading(null);
        }
    };

    const confirmDelete = async () => {
        if (!confirmAction) return;
        const id = confirmAction.id;

        try {
            await removeContainer(id);
            setConfirmAction(null);
            setIsPollingPaused(false);
        } catch (e) {
            console.error(e);
            toastManager.add({
                title: "Removal Failed",
                description: String(e),
                type: "error"
            });
        } finally {
            setActionLoading(null);
        }
    };

    const confirmImageDeletion = async () => {
        if (!confirmImageDelete) return;
        const ref = confirmImageDelete;
        setActionLoading("delete-image-" + ref);
        try {
            await deleteImage(ref);
            setConfirmImageDelete(null);
            setIsPollingPaused(false);
            await refreshData();
            toastManager.add({
                title: "Image Deleted",
                description: `Successfully removed ${ref}`,
                type: "success"
            });
        } catch (e: any) {
            console.error(e);
            toastManager.add({
                title: "Deletion Failed",
                description: String(e.message || e),
                type: "error"
            });
        } finally {
            setActionLoading(null);
        }
    };

    const handleCreateFromImage = async (config: any) => {
        setActionLoading("create-image-" + config.image);
        try {
            if (config.runImmediately) {
                await runContainer(config.command);
            } else {
                await createContainer(config.command);
            }
            await refreshData();
            setShowCreateModalForImage(null);
        } catch (e: any) {
            alert(`Failed to create container: ` + e.message);
        } finally {
            setActionLoading(null);
        }
    };

    const totalRunning = containers ? containers.filter(c => c.State === "running").length : 0;

    const renderContainerRow = (container: any) => {
        const isRunning = container.State === "running";
        return (
            <tr key={container.ID} className="container-row">
                <td>
                    <div className="flex-center" style={{ justifyContent: "flex-start", gap: "8px" }}>
                        <span className={`status-indicator status-${container.State}`} />
                        <span style={{ fontSize: "12px", textTransform: "capitalize" }}>{container.State}</span>
                    </div>
                </td>
                <td style={{ fontWeight: 500 }}>
                    <Link to={`/container/${container.ID}`} style={{ color: isRunning ? "var(--accent-primary)" : "var(--text-primary)", textDecoration: "none", display: "inline-flex", alignItems: "center", gap: "6px" }} title="Inspect Container">
                        {container.Names || (container.ID ? container.ID.substring(0, 8) : "-")}
                        <ExternalLink size={14} style={{ opacity: 0.7 }} />
                    </Link>
                </td>
                <td style={{ color: "var(--text-secondary)" }}>{container.Image}</td>
                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                    {isRunning && stats[container.ID] ? (
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                            <span style={{ fontWeight: 500 }}>{Math.round(stats[container.ID].cpuUsageUsec / 1000)} ms CPU</span>
                            <span>{(stats[container.ID].memoryUsageBytes / 1024 / 1024).toFixed(1)} MB / {stats[container.ID].memoryLimitBytes ? (stats[container.ID].memoryLimitBytes / 1024 / 1024 / 1024).toFixed(2) + ' GB' : '-'}</span>
                            <span style={{ fontSize: '11px', opacity: 0.7 }}>
                                {(stats[container.ID].networkRxBytes / 1024).toFixed(1)} KB ↓ / {(stats[container.ID].networkTxBytes / 1024).toFixed(1)} KB ↑
                            </span>
                        </div>
                    ) : (
                        "-"
                    )}
                </td>
                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                    {Array.isArray(container.Ports) && container.Ports.length > 0
                        ? container.Ports.map((p: any) => `${p.hostPort || '*'}:${p.containerPort}`).join(", ")
                        : (typeof container.Ports === 'string' && container.Ports ? container.Ports : "-")}
                </td>
                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{container.CreatedAt || "-"}</td>
                <td style={{ textAlign: "right" }}>
                    <div className="action-buttons">
                        {isRunning ? (
                            <button
                                type="button"
                                className="btn-icon text-warning"
                                onClick={() => handleContainerAction(container.ID, "stop")}
                                disabled={actionLoading === container.ID + "stop"}
                                title="Stop Container"
                            >
                                {actionLoading === container.ID + "stop" ? <RefreshCw size={16} className="spin" /> : <Square size={16} />}
                            </button>
                        ) : (
                            <button
                                type="button"
                                className="btn-icon text-success"
                                onClick={() => handleContainerAction(container.ID, "start")}
                                disabled={actionLoading === container.ID + "start"}
                                title="Start Container"
                            >
                                {actionLoading === container.ID + "start" ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                            </button>
                        )}
                        <button
                            type="button"
                            className="btn-icon text-danger"
                            onClick={() => handleContainerAction(container.ID, "remove")}
                            disabled={actionLoading === container.ID + "remove"}
                            title="Remove Container"
                        >
                            {actionLoading === container.ID + "remove" ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
                        </button>
                    </div>
                </td>
            </tr>
        );
    };

    if (isLoading && !systemRunning && containers.length === 0) {
        return (
            <div className="flex-center" style={{ height: "100vh", flexDirection: "column", gap: "16px" }}>
                <RefreshCw size={48} className="spin" color="var(--accent-primary)" />
                <p style={{ color: "var(--text-secondary)" }}>Initializing dashboard...</p>
            </div>
        );
    }

    const content = systemRunning ? (
        <>
            <div className="stats-grid">
                <div className="stat-card premium-card">
                    <div className="stat-icon" style={{ background: 'rgba(52, 199, 89, 0.1)', color: '#34c759' }}>
                        <Box size={20} />
                    </div>
                    <div className="stat-info">
                        <div className="stat-value">{containers.length}</div>
                        <div className="stat-label">Total Containers</div>
                    </div>
                </div>
                <div className="stat-card premium-card">
                    <div className="stat-icon" style={{ background: 'rgba(0, 122, 255, 0.1)', color: '#007aff' }}>
                        <Activity size={20} />
                    </div>
                    <div className="stat-info">
                        <div className="stat-value">{totalRunning}</div>
                        <div className="stat-label">Running Now</div>
                    </div>
                </div>
                <div className="stat-card premium-card">
                    <div className="stat-icon" style={{ background: 'rgba(255, 59, 48, 0.1)', color: '#ff3b30' }}>
                        <AlertCircle size={20} />
                    </div>
                    <div className="stat-info">
                        <div className="stat-value">{containers.filter(c => c.State === "exited").length}</div>
                        <div className="stat-label">Stopped</div>
                    </div>
                </div>
                <div className="stat-card premium-card">
                    <div className="stat-icon" style={{ background: 'rgba(175, 82, 222, 0.1)', color: '#af52de' }}>
                        <Terminal size={20} />
                    </div>
                    <div className="stat-info">
                        <div className="stat-value">{systemRunning ? "Active" : "Offline"}</div>
                        <div className="stat-label">System Status</div>
                    </div>
                </div>
            </div>

            <main className="containers-list premium-card" style={{ marginTop: '24px' }}>
                <div className="list-header flex-between mb-4">
                    <h2 style={{ fontSize: '20px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <Box size={20} /> Containers
                    </h2>
                    <div className="flex-center" style={{ gap: '16px' }}>
                        <div className="flex-center" style={{ gap: '8px' }}>
                            <label className="switch" style={{ fontSize: '13px', display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
                                <input
                                    type="checkbox"
                                    checked={groupByProject}
                                    onChange={e => setGroupByProject(e.target.checked)}
                                    style={{ width: 'auto' }}
                                />
                                Group by Project
                            </label>
                        </div>
                        <button className="btn btn-ghost" onClick={refreshData}>
                            <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                        </button>
                    </div>
                </div>

                {!containers || containers.length === 0 ? (
                    <div className="empty-state">
                        <Box size={48} color="var(--text-secondary)" opacity={0.5} />
                        <p>No containers found.</p>
                        <p className="subtitle">Launch a new container above to get started.</p>
                    </div>
                ) : (
                    <div className="table-responsive">
                        <table className="container-table">
                            <thead>
                                <tr>
                                    <th>Status</th>
                                    <th>Name</th>
                                    <th>Image</th>
                                    <th>CPU / Mem / Net</th>
                                    <th>Ports</th>
                                    <th>Created</th>
                                    <th style={{ textAlign: "right" }}>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {groupByProject ? (
                                    Object.entries(
                                        containers.reduce((acc, c) => {
                                            const project = (c.Labels && c.Labels["cider.compose.project"]) || "Other";
                                            if (!acc[project]) acc[project] = [];
                                            acc[project].push(c);
                                            return acc;
                                        }, {} as Record<string, ContainerInfo[]>)
                                    ).map(([project, projectContainers]) => (
                                        <React.Fragment key={project}>
                                            <tr className="project-group-header">
                                                <td colSpan={7} style={{ background: 'rgba(255,255,255,0.03)', padding: '12px 16px', fontWeight: 600 }}>
                                                    <div className="flex-between">
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <Layers size={14} color="var(--accent-primary)" />
                                                            Project: {project}
                                                        </div>
                                                        <div className="flex-center" style={{ gap: '8px' }}>
                                                            {project !== "Other" && (
                                                                <>
                                                                    <button className="btn-icon btn-small" title="Start All" onClick={() => Promise.all(projectContainers.map(c => startContainer(`container start ${c.ID}`))).then(refreshData)}>
                                                                        <Play size={14} />
                                                                    </button>
                                                                    <button className="btn-icon btn-small" title="Stop All" onClick={() => Promise.all(projectContainers.map(c => stopContainer(c.ID))).then(refreshData)}>
                                                                        <Square size={14} />
                                                                    </button>
                                                                </>
                                                            )}
                                                        </div>
                                                    </div>
                                                </td>
                                            </tr>
                                            {projectContainers.map(container => renderContainerRow(container))}
                                        </React.Fragment>
                                    ))
                                ) : (
                                    containers.map(container => renderContainerRow(container))
                                )}
                            </tbody>
                        </table>
                    </div>
                )}
            </main>

            <main className="containers-list premium-card" style={{ marginTop: '24px' }}>
                <div className="list-header flex-between mb-4">
                    <h2 style={{ fontSize: '20px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <Terminal size={20} /> Command Logs
                    </h2>
                    <button className="btn btn-secondary" onClick={async () => { await clearActionLogs(); await refreshData(); }}>Clear Logs</button>
                </div>
                <div className="terminal-logs" style={{ background: '#0a0a0a', padding: '16px', borderRadius: '8px', maxHeight: '300px', overflowY: 'auto', fontFamily: 'monospace', fontSize: '13px' }}>
                    {(logs || []).length === 0 ? (
                        <span style={{ color: 'var(--text-secondary)' }}>No recent commands executed.</span>
                    ) : (
                        logs.map((log) => (
                            <div key={log.id} style={{ marginBottom: '12px', paddingBottom: '12px', borderBottom: '1px solid rgba(255,255,255,0.05)' }}>
                                <div style={{ color: 'var(--accent-primary)', marginBottom: '4px' }}>
                                    <span style={{ color: 'var(--text-secondary)' }}>[{new Date(log.timestamp).toLocaleTimeString()}]</span> $ {log.command}
                                </div>
                                <div style={{ color: log.isError ? 'var(--danger-color)' : 'var(--text-primary)', whiteSpace: 'pre-wrap', paddingLeft: '8px', borderLeft: log.isError ? '2px solid var(--danger-color)' : '2px solid rgba(255,255,255,0.1)' }}>
                                    {log.output || "No output"}
                                </div>
                            </div>
                        ))
                    )}
                </div>
            </main>
        </>
    ) : (
        <div className="empty-state premium-card" style={{ marginTop: '24px', padding: '64px' }}>
            <Power size={64} style={{ opacity: 0.2, marginBottom: '24px' }} />
            <h2 style={{ fontSize: '24px', marginBottom: '8px' }}>System Offline</h2>
            <p style={{ color: 'var(--text-secondary)', maxWidth: '400px', margin: '0 auto 24px' }}>
                The container management system is currently offline. Start the system to manage containers and images.
            </p>
            <button className="btn btn-primary btn-large" onClick={handleSystemToggle} disabled={actionLoading === "system"}>
                {actionLoading === "system" ? <RefreshCw size={20} className="spin" /> : <Power size={20} />}
                Start System
            </button>
        </div>
    );

    return (
        <div className="dashboard-container animate-fade-in">
            <header className="dashboard-header premium-card flex-between">
                <div className="header-brand">
                    <div className="brand-logo flex-center">
                        <Box size={24} color="var(--accent-primary)" />
                    </div>
                    <div>
                        <h1 style={{ fontSize: '24px', margin: 0 }}>apple container UI</h1>
                        <p style={{ color: "var(--text-secondary)", fontSize: "14px", marginTop: "4px" }}>
                            Next-gen container management
                        </p>
                    </div>
                </div>

                <div className="system-status-toggle" style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/images" className="btn-icon" title="Images & Builders">
                        <Layers size={20} color="var(--text-secondary)" />
                    </Link>
                    <Link to="/networks" className="btn-icon" title="Container Networks">
                        <Network size={20} color="var(--text-secondary)" />
                    </Link>
                    <Link to="/volumes" className="btn-icon" title="Storage Volumes">
                        <Database size={20} color="var(--text-secondary)" />
                    </Link>
                    <Link to="/registries" className="btn-icon" title="Registries">
                        <KeySquare size={20} color="var(--text-secondary)" />
                    </Link>
                    <Link to="/settings" className="btn-icon" title="Preferences">
                        <Settings size={20} color="var(--text-secondary)" />
                    </Link>
                    <div className="status-badge flex-center" style={{ gap: '8px' }}>
                        <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                        <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                    </div>
                    <button
                        className={`btn ${systemRunning ? 'btn-danger' : 'btn-primary'}`}
                        onClick={handleSystemToggle}
                        disabled={actionLoading === "system"}
                        style={{ opacity: actionLoading === "system" ? 0.7 : 1 }}
                    >
                        {actionLoading === "system" ? (
                            <RefreshCw size={16} className="spin" />
                        ) : (
                            <Power size={16} />
                        )}
                        {systemRunning ? "Stop System" : "Start System"}
                    </button>
                </div>
            </header>

            {content}

            {showCreateModalForImage && (
                <CreateContainerModal
                    imageName={showCreateModalForImage as string}
                    onClose={() => setShowCreateModalForImage(null)}
                    onCreate={handleCreateFromImage}
                    isCreating={actionLoading === "create-image-" + showCreateModalForImage}
                />
            )}

            <AlertDialog.Root open={!!confirmAction} onOpenChange={(open) => {
                if (!open) {
                    setConfirmAction(null);
                    setIsPollingPaused(false);
                }
            }}>
                <AlertDialog.Portal>
                    <AlertDialog.Backdrop className="dialog-backdrop" />
                    <AlertDialog.Popup className="dialog-popup">
                        <AlertDialog.Title className="dialog-title">Remove Container</AlertDialog.Title>
                        <AlertDialog.Description className="dialog-description">
                            Are you sure you want to remove <strong>{confirmAction?.name}</strong>? This action cannot be undone.
                        </AlertDialog.Description>
                        <div className="dialog-actions">
                            <AlertDialog.Close className="btn btn-secondary" disabled={actionLoading === (confirmAction?.id + "remove")}>Cancel</AlertDialog.Close>
                            <button
                                className="btn btn-danger"
                                onClick={confirmDelete}
                                disabled={actionLoading === (confirmAction?.id + "remove")}
                            >
                                {actionLoading === (confirmAction?.id + "remove") ? <RefreshCw size={16} className="spin" /> : "Remove"}
                            </button>
                        </div>
                    </AlertDialog.Popup>
                </AlertDialog.Portal>
            </AlertDialog.Root>

            <AlertDialog.Root open={!!confirmImageDelete} onOpenChange={(open) => {
                if (!open) {
                    setConfirmImageDelete(null);
                    setIsPollingPaused(false);
                }
            }}>
                <AlertDialog.Portal>
                    <AlertDialog.Backdrop className="dialog-backdrop" />
                    <AlertDialog.Popup className="dialog-popup">
                        <AlertDialog.Title className="dialog-title">Delete Image</AlertDialog.Title>
                        <AlertDialog.Description className="dialog-description">
                            Are you sure you want to delete <strong>{confirmImageDelete}</strong>? This action cannot be undone.
                        </AlertDialog.Description>
                        <div className="dialog-actions">
                            <AlertDialog.Close className="btn btn-secondary" disabled={actionLoading === ("delete-image-" + confirmImageDelete)}>Cancel</AlertDialog.Close>
                            <button
                                className="btn btn-danger"
                                onClick={confirmImageDeletion}
                                disabled={actionLoading === ("delete-image-" + confirmImageDelete)}
                            >
                                {actionLoading === ("delete-image-" + confirmImageDelete) ? <RefreshCw size={16} className="spin" /> : "Delete"}
                            </button>
                        </div>
                    </AlertDialog.Popup>
                </AlertDialog.Portal>
            </AlertDialog.Root>

            <Toast.Portal>
                <Toast.Viewport className="toast-viewport">
                    {toasts.map((toast: any) => (
                        <Toast.Root key={toast.id} toast={toast} className={`toast-root toast-${toast.type || 'info'}`}>
                            <div className="toast-content">
                                <Toast.Title className="toast-title">{toast.title}</Toast.Title>
                                <Toast.Description className="toast-description">{toast.description}</Toast.Description>
                            </div>
                            <Toast.Close className="toast-close">×</Toast.Close>
                        </Toast.Root>
                    ))}
                </Toast.Viewport>
            </Toast.Portal>
        </div>
    );
}
