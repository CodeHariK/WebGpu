"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
    CornerLeftUp,
    Database,
    Trash2,
    RefreshCw,
    Plus
} from "lucide-react";
import {
    listVolumes,
    createVolume,
    removeVolume,
    VolumeInfo,
    checkSystemStatus
} from "@/lib/container";
import "../Dashboard.css";

export default function VolumesPage() {
    const [volumes, setVolumes] = useState<VolumeInfo[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [newVolumeName, setNewVolumeName] = useState("");
    const [systemRunning, setSystemRunning] = useState(false);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const vols = await listVolumes();
                setVolumes(vols);
            } else {
                setVolumes([]);
            }
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        refreshData();
        const stored = localStorage.getItem("refreshInterval");
        const intervalMs = stored ? parseInt(stored, 10) : 60000;
        if (intervalMs > 0) {
            const interval = setInterval(refreshData, intervalMs);
            return () => clearInterval(interval);
        }
    }, []);

    const handleCreate = async () => {
        if (!newVolumeName.trim()) return;
        setActionLoading("create");
        try {
            await createVolume(newVolumeName.trim());
            setNewVolumeName("");
            await refreshData();
        } catch (e: any) {
            alert(e.message);
        } finally {
            setActionLoading(null);
        }
    };

    const handleDelete = async (name: string) => {
        if (!confirm(`Are you sure you want to delete volume '${name}'?`)) return;
        setActionLoading("delete-" + name);
        try {
            await removeVolume(name, true);
            await refreshData();
        } catch (e: any) {
            alert(e.message);
        } finally {
            setActionLoading(null);
        }
    };

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1000px', margin: '0 auto' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px" }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link href="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Storage Volumes</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card">
                    <Database size={48} color="var(--text-secondary)" opacity={0.5} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage volumes.</p>
                </div>
            ) : (
                <>
                    <section className="create-container-section premium-card mb-4" style={{ marginTop: '-4px' }}>
                        <h2 style={{ fontSize: '18px', marginBottom: '16px' }}>Create Named Volume</h2>
                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>Volume Name</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., redis-data"
                                    value={newVolumeName}
                                    onChange={e => setNewVolumeName(e.target.value)}
                                    // Submit on Enter key
                                    onKeyDown={e => e.key === 'Enter' && handleCreate()}
                                />
                            </div>
                            <div>
                                <button
                                    className="btn btn-primary"
                                    onClick={handleCreate}
                                    disabled={!newVolumeName.trim() || actionLoading === "create"}
                                    style={{ height: '40px', padding: '0 24px' }}
                                >
                                    {actionLoading === "create" ? <RefreshCw size={16} className="spin" /> : <Plus size={16} />}
                                    Create Volume
                                </button>
                            </div>
                        </div>
                    </section>

                    <main className="containers-list premium-card">
                        <div className="list-header flex-between mb-4">
                            <h2 style={{ fontSize: '20px' }}>Your Volumes</h2>
                            <button className="btn btn-ghost" onClick={refreshData}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {volumes.length === 0 ? (
                            <div className="empty-state">
                                <Database size={48} color="var(--text-secondary)" opacity={0.5} />
                                <p>No volumes found.</p>
                                <p className="subtitle">Create one above to persist container data.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table">
                                    <thead>
                                        <tr>
                                            <th>Name</th>
                                            <th>Driver</th>
                                            <th>Size</th>
                                            <th>Mountpoint</th>
                                            <th style={{ textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {volumes.map(vol => (
                                            <tr key={vol.name} className="container-row">
                                                <td style={{ fontWeight: 500, color: "var(--text-primary)" }}>{vol.name}</td>
                                                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{vol.driver}</td>
                                                <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                                                    {vol.sizeInBytes !== undefined ? (vol.sizeInBytes / 1024 / 1024).toFixed(2) + ' MB' : '-'}
                                                </td>
                                                <td style={{ color: "var(--text-secondary)", fontSize: "11px", wordBreak: 'break-all' }}>{vol.source}</td>
                                                <td style={{ textAlign: "right" }}>
                                                    <div className="action-buttons">
                                                        <button
                                                            className="btn-icon text-danger"
                                                            onClick={() => handleDelete(vol.name)}
                                                            disabled={actionLoading === "delete-" + vol.name}
                                                            title="Delete Volume"
                                                        >
                                                            {actionLoading === "delete-" + vol.name ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
                                                        </button>
                                                    </div>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </main>
                </>
            )}
        </div>
    );
}
