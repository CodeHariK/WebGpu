import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
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
    checkSystemStatus
} from "../lib/container";
import type {
    VolumeInfo
} from "../lib/container";
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
        const interval = setInterval(refreshData, 30000);
        return () => clearInterval(interval);
    }, []);

    const handleCreate = async () => {
        if (!newVolumeName.trim()) return;
        setActionLoading("create");
        try {
            const res = await createVolume(newVolumeName.trim());
            if (!res.ok) throw new Error(await res.text() || "Failed to create volume");
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
            const res = await removeVolume(name);
            if (!res.ok) throw new Error(await res.text() || "Failed to delete volume");
            await refreshData();
        } catch (e: any) {
            alert(e.message);
        } finally {
            setActionLoading(null);
        }
    };

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1000px', margin: '0 auto', padding: '24px' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/" className="btn-icon" style={{ color: 'inherit' }}>
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Storage Volumes</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px', display: 'flex', alignItems: 'center' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card" style={{ textAlign: 'center', padding: '48px' }}>
                    <Database size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage volumes.</p>
                </div>
            ) : (
                <>
                    <section className="create-container-section premium-card mb-4" style={{ marginBottom: '24px' }}>
                        <h2 style={{ fontSize: '18px', marginBottom: '16px' }}>Create Named Volume</h2>
                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Volume Name</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., redis-data"
                                    value={newVolumeName}
                                    onChange={e => setNewVolumeName(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handleCreate()}
                                    style={{ width: '100%' }}
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
                        <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <h2 style={{ fontSize: '20px', margin: 0 }}>Your Volumes</h2>
                            <button className="btn btn-ghost" onClick={refreshData}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {volumes.length === 0 ? (
                            <div className="empty-state" style={{ textAlign: 'center', padding: '48px' }}>
                                <Database size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                                <p>No volumes found.</p>
                                <p className="subtitle">Create one above to persist container data.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                            <th style={{ padding: '12px' }}>Name</th>
                                            <th style={{ padding: '12px' }}>Driver</th>
                                            <th style={{ padding: '12px' }}>Size</th>
                                            <th style={{ padding: '12px' }}>Mountpoint</th>
                                            <th style={{ padding: '12px', textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {volumes.map(vol => (
                                            <tr key={vol.name} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                                <td style={{ padding: '12px', fontWeight: 500, color: "var(--text-primary)" }}>{vol.name}</td>
                                                <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>{vol.driver}</td>
                                                <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>
                                                    {vol.sizeInBytes !== undefined ? (vol.sizeInBytes / 1024 / 1024).toFixed(2) + ' MB' : '-'}
                                                </td>
                                                <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "11px", wordBreak: 'break-all' }}>{vol.source}</td>
                                                <td style={{ padding: '12px', textAlign: "right" }}>
                                                    <div className="action-buttons" style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
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
