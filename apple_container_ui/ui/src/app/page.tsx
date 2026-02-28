"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
  checkSystemStatus,
  startSystem,
  stopSystem,
  listContainers,
  startContainer,
  stopContainer,
  removeContainer,
  createContainer,
  pullImage,
  listImages,
  deleteImage,
  getActionLogs,
  clearActionLogs,
  ContainerInfo,
  ImageInfo,
  ContainerConfig,
  CommandLog,
  getContainerStats,
  pruneSystem
} from "@/lib/container";
import CreateContainerModal from "@/components/CreateContainerModal";
import {
  Power,
  Play,
  Square,
  Trash2,
  RefreshCw,
  Server,
  Box,
  Activity,
  Download,
  Layers,
  Terminal,
  ExternalLink,
  Trash,
  Settings,
  Database,
  Network
} from "lucide-react";
import "./Dashboard.css";

export default function Dashboard() {
  const [systemRunning, setSystemRunning] = useState<boolean>(false);
  const [containers, setContainers] = useState<ContainerInfo[]>([]);
  const [stats, setStats] = useState<Record<string, any>>({});
  const [pollingInterval, setPollingInterval] = useState<number>(60000); // 1 min default
  const [isLoading, setIsLoading] = useState<boolean>(true);
  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [newImage, setNewImage] = useState<string>("");
  const [newArgs, setNewArgs] = useState<string>("");
  const [images, setImages] = useState<ImageInfo[]>([]);
  const [logs, setLogs] = useState<CommandLog[]>([]);
  const [showCreateModalForImage, setShowCreateModalForImage] = useState<string | null>(null);

  const refreshData = async () => {
    setIsLoading(true);
    try {
      const isRunning = await checkSystemStatus();
      setSystemRunning(isRunning);
      if (isRunning) {
        const [cList, iList, logList, sList] = await Promise.all([listContainers(), listImages(), getActionLogs(), getContainerStats()]);
        setContainers(cList);
        setImages(iList);
        setLogs(logList);

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

  // Load settings on mount
  useEffect(() => {
    const stored = localStorage.getItem("refreshInterval");
    if (stored !== null) {
      setPollingInterval(parseInt(stored, 10));
    }
  }, []);

  useEffect(() => {
    refreshData();
    if (pollingInterval > 0) {
      const interval = setInterval(refreshData, pollingInterval);
      return () => clearInterval(interval);
    }
  }, [pollingInterval]);

  const handleSystemToggle = async () => {
    setActionLoading("system");
    try {
      if (systemRunning) {
        await stopSystem();
      } else {
        await startSystem();
      }
      await new Promise(r => setTimeout(r, 2000)); // wait for daemon to start/stop
      await refreshData();
    } catch (e) {
      alert("Failed to toggle system: " + e);
    } finally {
      setActionLoading(null);
    }
  };

  const handlePruneSystem = async () => {
    if (!confirm("Are you sure you want to prune all unused containers, networks, and images? This cannot be undone.")) return;
    setActionLoading("prune");
    try {
      await pruneSystem();
      alert("System pruned successfully.");
      await refreshData();
    } catch (e: any) {
      alert("Failed to prune: " + e.message);
    } finally {
      setActionLoading(null);
    }
  };

  const handleContainerAction = async (id: string, action: "start" | "stop" | "remove") => {
    setActionLoading(id + action);
    try {
      if (action === "start") await startContainer(id);
      if (action === "stop") await stopContainer(id);
      if (action === "remove") {
        if (confirm("Are you sure you want to remove this container?")) {
          await removeContainer(id, true);
        }
      }
      await refreshData();
    } catch (e) {
      alert(`Failed to ${action} container: ` + e);
    } finally {
      setActionLoading(null);
    }
  };

  const handleCreate = async () => {
    if (!newImage) return;
    setActionLoading("create");
    try {
      await createContainer({ image: newImage, command: newArgs });
      setNewImage("");
      setNewArgs("");
      await refreshData();
    } catch (e: any) {
      alert(e.message);
    } finally {
      setActionLoading(null);
    }
  };

  const handlePull = async () => {
    if (!newImage) return;
    setActionLoading("create");
    try {
      await pullImage(newImage);
      alert(`Successfully pulled ${newImage}`);
      await refreshData();
    } catch (e: any) {
      alert(e.message);
    } finally {
      setActionLoading(null);
    }
  };

  const handleDeleteImage = async (imageRef: string) => {
    if (!confirm(`Are you sure you want to delete the image ${imageRef}?`)) return;
    setActionLoading("delete-image-" + imageRef);
    try {
      await deleteImage(imageRef, true);
      await refreshData();
    } catch (e: any) {
      alert(`Failed to delete image: ` + e.message);
    } finally {
      setActionLoading(null);
    }
  };

  const handleCreateFromImage = async (config: ContainerConfig) => {
    setActionLoading("create-image-" + config.image);
    try {
      await createContainer(config);
      await refreshData();
      setShowCreateModalForImage(null);
    } catch (e: any) {
      alert(`Failed to create container: ` + e.message);
    } finally {
      setActionLoading(null);
    }
  };

  const totalRunning = containers.filter(c => c.State === "running").length;

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
          <Link href="/networks" className="btn-icon" title="Container Networks">
            <Network size={20} color="var(--text-secondary)" />
          </Link>
          <Link href="/volumes" className="btn-icon" title="Storage Volumes">
            <Database size={20} color="var(--text-secondary)" />
          </Link>
          <Link href="/settings" className="btn-icon" title="Preferences">
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

      <div className="stats-grid">
        <div className="premium-card stat-card">
          <div className="stat-icon flex-center" style={{ background: 'rgba(16, 185, 129, 0.1)', color: 'var(--status-running)' }}>
            <Activity size={24} />
          </div>
          <div className="stat-info">
            <h3>{totalRunning}</h3>
            <p>Active Containers</p>
          </div>
        </div>
        <div className="premium-card stat-card">
          <div className="stat-icon flex-center" style={{ background: 'rgba(255, 255, 255, 0.05)', color: 'var(--text-primary)' }}>
            <Server size={24} />
          </div>
          <div className="stat-info">
            <h3>{containers.length}</h3>
            <p>Total Containers</p>
          </div>
        </div>
        <div className="premium-card stat-card">
          <div className="stat-icon flex-center" style={{ background: 'rgba(0, 102, 255, 0.1)', color: 'var(--accent-primary)' }}>
            <Layers size={24} />
          </div>
          <div className="stat-info">
            <h3>{images.length}</h3>
            <p>Downloaded Images</p>
          </div>
        </div>
      </div>

      {systemRunning && (
        <section className="create-container-section premium-card mb-4" style={{ marginTop: '-4px' }}>
          <h2 style={{ fontSize: '18px', marginBottom: '16px' }}>Launch & Pull</h2>
          <div style={{ display: 'flex', gap: '16px', flexWrap: 'wrap', alignItems: 'flex-end' }}>
            <div className="input-group">
              <label>Image Reference</label>
              <input
                className="premium-input"
                placeholder="e.g., ubuntu:latest"
                value={newImage}
                onChange={e => setNewImage(e.target.value)}
              />
            </div>
            <div className="input-group" style={{ flex: 1, minWidth: '200px' }}>
              <label>Arguments (Optional)</label>
              <input
                className="premium-input"
                placeholder="e.g., --name my-app"
                value={newArgs}
                onChange={e => setNewArgs(e.target.value)}
                style={{ width: '100%' }}
              />
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <button
                className="btn btn-primary"
                onClick={handleCreate}
                disabled={!newImage || actionLoading === "create"}
                style={{ height: '40px', padding: '0 16px' }}
              >
                {actionLoading === "create" ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                Create
              </button>
              <button
                className="btn btn-ghost"
                onClick={handlePull}
                disabled={!newImage || actionLoading === "create"}
                style={{ border: '1px solid var(--border-color)', height: '40px', padding: '0 16px' }}
              >
                {actionLoading === "create" ? <RefreshCw size={16} className="spin" /> : <Download size={16} />}
                Pull
              </button>
            </div>
          </div>
          <div style={{ marginTop: '16px', display: 'flex', justifyContent: 'flex-end' }}>
            <button
              className="btn btn-ghost text-danger"
              onClick={handlePruneSystem}
              disabled={actionLoading === "prune"}
              style={{ border: '1px solid rgba(239, 68, 68, 0.3)' }}
            >
              {actionLoading === "prune" ? <RefreshCw size={16} className="spin" /> : <Trash size={16} />}
              Prune Unused Data
            </button>
          </div>
        </section>
      )
      }

      <main className="containers-list premium-card">
        <div className="list-header flex-between mb-4">
          <h2 style={{ fontSize: '20px' }}>Your Containers</h2>
          <button className="btn btn-ghost" onClick={refreshData}>
            <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
          </button>
        </div>

        {!systemRunning ? (
          <div className="empty-state">
            <Power size={48} color="var(--text-secondary)" opacity={0.5} />
            <p>The container daemon is offline.</p>
            <p className="subtitle">Start the system to see your containers.</p>
          </div>
        ) : containers.length === 0 ? (
          <div className="empty-state">
            <Box size={48} color="var(--text-secondary)" opacity={0.5} />
            <p>No containers found.</p>
            <p className="subtitle">Use the CLI to create some containers!</p>
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
                {containers.map(container => {
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
                        <Link href={`/container/${container.ID}`} style={{ color: isRunning ? "var(--accent-primary)" : "var(--text-primary)", textDecoration: "none", display: "inline-flex", alignItems: "center", gap: "6px" }} title="Inspect Container">
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
                              className="btn-icon text-warning"
                              onClick={() => handleContainerAction(container.ID, "stop")}
                              disabled={actionLoading === container.ID + "stop"}
                              title="Stop Container"
                            >
                              {actionLoading === container.ID + "stop" ? <RefreshCw size={16} className="spin" /> : <Square size={16} />}
                            </button>
                          ) : (
                            <button
                              className="btn-icon text-success"
                              onClick={() => handleContainerAction(container.ID, "start")}
                              disabled={actionLoading === container.ID + "start"}
                              title="Start Container"
                            >
                              {actionLoading === container.ID + "start" ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                            </button>
                          )}
                          <button
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
                  )
                })}
              </tbody>
            </table>
          </div>
        )}
      </main>

      {systemRunning && images.length > 0 && (
        <main className="containers-list premium-card">
          <div className="list-header flex-between mb-4">
            <h2 style={{ fontSize: '20px' }}>Downloaded Images</h2>
          </div>
          <div className="table-responsive">
            <table className="container-table">
              <thead>
                <tr>
                  <th>Repository</th>
                  <th>Tag</th>
                  <th>Image ID</th>
                  <th>Size</th>
                  <th>Created</th>
                  <th style={{ textAlign: "right" }}>Actions</th>
                </tr>
              </thead>
              <tbody>
                {images.map(img => (
                  <tr key={img.Name} className="container-row">
                    <td style={{ fontWeight: 500 }}>{img.Repository || "<none>"}</td>
                    <td style={{ color: "var(--text-secondary)" }}>{img.Tag || "<none>"}</td>
                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{img.ID ? img.ID.substring(0, 12) : "-"}</td>
                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{img.Size || "-"}</td>
                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{img.CreatedAt || "-"}</td>
                    <td style={{ textAlign: "right" }}>
                      <div className="action-buttons" style={{ justifyContent: "flex-end" }}>
                        <button
                          className="btn-icon text-success"
                          onClick={() => setShowCreateModalForImage(img.Name)}
                          disabled={actionLoading === "create-image-" + img.Name}
                          title="Create Container"
                        >
                          {actionLoading === "create-image-" + img.Name ? <RefreshCw size={16} className="spin" /> : <Play size={16} />}
                        </button>
                        <button
                          className="btn-icon text-danger"
                          onClick={() => handleDeleteImage(img.Name)}
                          disabled={actionLoading === "delete-image-" + img.Name}
                          title="Delete Image"
                        >
                          {actionLoading === "delete-image-" + img.Name ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </main>
      )}

      {systemRunning && (
        <main className="containers-list premium-card" style={{ marginTop: '24px' }}>
          <div className="list-header flex-between mb-4">
            <h2 style={{ fontSize: '20px', display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Terminal size={20} /> Command Logs
            </h2>
            <button className="btn btn-secondary" onClick={async () => { await clearActionLogs(); await refreshData(); }}>Clear Logs</button>
          </div>
          <div className="terminal-logs" style={{ background: '#0a0a0a', padding: '16px', borderRadius: '8px', maxHeight: '300px', overflowY: 'auto', fontFamily: 'monospace', fontSize: '13px' }}>
            {logs.length === 0 ? (
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
      )}
      {showCreateModalForImage && (
        <CreateContainerModal
          imageName={showCreateModalForImage}
          onClose={() => setShowCreateModalForImage(null)}
          onCreate={handleCreateFromImage}
          isCreating={actionLoading === "create-image-" + showCreateModalForImage}
        />
      )}
    </div>
  );
}
