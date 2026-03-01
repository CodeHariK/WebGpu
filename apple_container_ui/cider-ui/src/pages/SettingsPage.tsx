import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import {
    CornerLeftUp,
    Save,
    CheckCircle,
    Settings as SettingsIcon,
    Edit2,
    Trash2,
    RefreshCw,
    Loader2
} from "lucide-react";
import {
    listSystemProperties,
    setSystemProperty,
    clearSystemProperty,
    checkSystemStatus
} from "../lib/container";
import type { SystemProperty } from "../lib/container";
import "../Dashboard.css";

export default function SettingsPage() {
    const [refreshInterval, setRefreshInterval] = useState<number>(60000);
    const [saved, setSaved] = useState(false);

    // System Properties State
    const [properties, setProperties] = useState<SystemProperty[]>([]);
    const [isLoadingProps, setIsLoadingProps] = useState(false);
    const [systemRunning, setSystemRunning] = useState(false);

    // Modal State
    const [editingProp, setEditingProp] = useState<SystemProperty | null>(null);
    const [editValue, setEditValue] = useState("");
    const [isSavingProp, setIsSavingProp] = useState(false);

    useEffect(() => {
        const stored = localStorage.getItem("refreshInterval");
        if (stored !== null) {
            setRefreshInterval(parseInt(stored, 10));
        }
        loadProperties();
    }, []);

    const loadProperties = async () => {
        setIsLoadingProps(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const props = await listSystemProperties();
                setProperties(props);
            }
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoadingProps(false);
        }
    };

    const handleEditClick = (prop: SystemProperty) => {
        setEditingProp(prop);
        setEditValue(prop.Value);
    };

    const handleSaveProp = async () => {
        if (!editingProp) return;
        setIsSavingProp(true);
        try {
            const res = await setSystemProperty(editingProp.ID, editValue);
            if (!res.ok) throw new Error(await res.text());
            await loadProperties();
            setEditingProp(null);
        } catch (e: any) {
            alert(e.message || "Failed to set property");
        } finally {
            setIsSavingProp(false);
        }
    };

    const handleClearProp = async (id: string) => {
        if (!confirm(`Are you sure you want to clear the custom value for ${id}? It will revert to default.`)) return;
        setIsLoadingProps(true);
        try {
            const res = await clearSystemProperty(id);
            if (!res.ok) throw new Error(await res.text());
            await loadProperties();
        } catch (e: any) {
            alert(e.message || "Failed to clear property");
        } finally {
            setIsLoadingProps(false);
        }
    };

    const handleSave = () => {
        localStorage.setItem("refreshInterval", refreshInterval.toString());
        setSaved(true);
        setTimeout(() => setSaved(false), 3000);
    };

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '800px', margin: '0 auto', padding: '24px' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '16px 24px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Settings</h1>
                </div>
            </header>

            <main className="premium-card" style={{ padding: '24px' }}>
                <h2 style={{ fontSize: '18px', marginBottom: '24px', borderBottom: '1px solid rgba(255,255,255,0.1)', paddingBottom: '12px' }}>
                    Dashboard Preferences
                </h2>

                <div className="input-group" style={{ marginBottom: '32px' }}>
                    <label style={{ fontSize: '14px', color: 'var(--text-secondary)', marginBottom: '8px', display: 'block' }}>
                        Auto-Refresh Interval
                    </label>
                    <p style={{ fontSize: '13px', color: 'rgba(255,255,255,0.5)', marginBottom: '16px' }}>
                        How often the dashboard should poll the container daemon for realtime updates (stats, running containers, etc).
                    </p>
                    <select
                        className="premium-input"
                        value={refreshInterval}
                        onChange={e => setRefreshInterval(parseInt(e.target.value, 10))}
                        style={{ maxWidth: '300px', width: '100%', padding: '10px 16px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px', color: 'white' }}
                    >
                        <option value={0}>Disabled</option>
                        <option value={5000}>5 Seconds</option>
                        <option value={10000}>10 Seconds</option>
                        <option value={30000}>30 Seconds</option>
                        <option value={60000}>1 Minute (Default)</option>
                        <option value={300000}>5 Minutes</option>
                    </select>
                </div>

                <div style={{ display: 'flex', alignItems: 'center', gap: '16px', borderTop: '1px solid rgba(255,255,255,0.1)', paddingTop: '24px' }}>
                    <button className="btn btn-primary" onClick={handleSave} style={{ display: 'inline-flex', alignItems: 'center', gap: '8px' }}>
                        <Save size={16} /> Save Settings
                    </button>
                    {saved && (
                        <span style={{ color: 'var(--status-running)', display: 'flex', alignItems: 'center', gap: '6px', fontSize: '14px' }}>
                            <CheckCircle size={16} /> Saved Successfully
                        </span>
                    )}
                </div>
            </main>

            <main className="premium-card" style={{ marginTop: '24px', padding: '24px' }}>
                <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                    <h2 style={{ fontSize: '18px', display: 'flex', alignItems: 'center', gap: '8px', margin: 0 }}>
                        <SettingsIcon size={18} /> Global System Properties
                    </h2>
                    <button className="btn btn-ghost" onClick={loadProperties} disabled={!systemRunning || isLoadingProps} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <RefreshCw size={16} className={isLoadingProps ? "spin" : ""} /> Refresh
                    </button>
                </div>

                {!systemRunning ? (
                    <div className="empty-state" style={{ textAlign: 'center', padding: '40px' }}>
                        <p>The container daemon is offline.</p>
                        <p className="subtitle" style={{ fontSize: '13px', color: 'var(--text-secondary)' }}>Start the system to view properties.</p>
                    </div>
                ) : properties.length === 0 && !isLoadingProps ? (
                    <div className="empty-state" style={{ textAlign: 'center', padding: '40px' }}>
                        <p>No system properties found.</p>
                    </div>
                ) : isLoadingProps ? (
                    <div className="flex-center" style={{ padding: '40px', display: 'flex', justifyContent: 'center' }}>
                        <Loader2 className="spin" size={32} />
                    </div>
                ) : (
                    <div className="table-responsive">
                        <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                            <thead>
                                <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                    <th style={{ padding: '12px' }}>Property ID</th>
                                    <th style={{ padding: '12px' }}>Value</th>
                                    <th style={{ padding: '12px' }}>Type</th>
                                    <th style={{ textAlign: "right", padding: '12px' }}>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {properties.map((p) => (
                                    <tr key={p.ID} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                        <td style={{ padding: '12px' }}>
                                            <div style={{ fontWeight: 500, color: "var(--text-primary)" }}>{p.ID}</div>
                                            <div style={{ fontSize: '12px', color: "var(--text-secondary)", marginTop: '4px' }}>{p.Description}</div>
                                        </td>
                                        <td style={{ fontFamily: 'monospace', padding: '12px' }}>
                                            {p.Value || <span style={{ color: 'var(--text-secondary)', fontStyle: 'italic' }}>default</span>}
                                        </td>
                                        <td style={{ padding: '12px' }}><span className="status-badge" style={{ padding: '2px 8px', background: 'rgba(255,255,255,0.1)', borderRadius: '4px', fontSize: '12px' }}>{p.Type}</span></td>
                                        <td style={{ textAlign: "right", padding: '12px' }}>
                                            <div className="action-buttons" style={{ display: 'flex', justifyContent: 'flex-end', gap: '8px' }}>
                                                <button className="btn-icon text-primary" onClick={() => handleEditClick(p)} title="Edit" style={{ background: 'transparent', border: 'none', color: 'var(--accent-primary)', cursor: 'pointer' }}>
                                                    <Edit2 size={16} />
                                                </button>
                                                {p.Value && (
                                                    <button className="btn-icon text-danger" onClick={() => handleClearProp(p.ID)} title="Clear (Revert to default)" style={{ background: 'transparent', border: 'none', color: 'var(--danger-color)', cursor: 'pointer' }}>
                                                        <Trash2 size={16} />
                                                    </button>
                                                )}
                                            </div>
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>
                )}
            </main>

            {/* Edit Property Modal */}
            {editingProp && (
                <div className="modal-overlay" style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.8)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000 }}>
                    <div className="premium-card animate-scale-in" style={{ width: '90%', maxWidth: '450px', padding: '24px', background: 'var(--bg-card)', borderRadius: '12px' }}>
                        <h2 className="modal-title" style={{ margin: '0 0 20px 0', fontSize: '20px' }}>Edit System Property</h2>
                        <div style={{ marginBottom: "24px" }}>
                            <p style={{ color: "var(--text-secondary)", fontSize: "14px", marginBottom: "12px", lineHeight: 1.5 }}>
                                {editingProp.Description}
                            </p>
                            <label style={{ display: "block", marginBottom: "12px", fontWeight: 500, fontSize: '15px' }}>
                                {editingProp.ID} <span style={{ opacity: 0.5, fontWeight: 'normal' }}>({editingProp.Type})</span>
                            </label>
                            {editingProp.Type === "Bool" ? (
                                <div style={{ display: 'flex', alignItems: 'center', gap: '12px', padding: '12px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px' }}>
                                    <input
                                        type="checkbox"
                                        id="prop-bool-input"
                                        checked={editValue === "true"}
                                        onChange={(e) => setEditValue(e.target.checked ? "true" : "false")}
                                        style={{ width: '20px', height: '20px', cursor: 'pointer' }}
                                    />
                                    <label htmlFor="prop-bool-input" style={{ cursor: 'pointer', fontSize: '14px' }}>
                                        {editValue === "true" ? "Enabled" : "Disabled"}
                                    </label>
                                </div>
                            ) : (
                                <input
                                    type="text"
                                    className="premium-input"
                                    value={editValue}
                                    onChange={(e) => setEditValue(e.target.value)}
                                    autoFocus
                                    style={{ width: '100%', padding: '10px 16px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px', color: 'white' }}
                                />
                            )}
                        </div>
                        <div className="modal-actions" style={{ display: 'flex', justifyContent: 'flex-end', gap: '12px' }}>
                            <button
                                className="btn btn-ghost"
                                onClick={() => setEditingProp(null)}
                                disabled={isSavingProp}
                            >
                                Cancel
                            </button>
                            <button
                                className="btn btn-primary"
                                onClick={handleSaveProp}
                                disabled={isSavingProp}
                                style={{ display: 'flex', alignItems: 'center', gap: '8px' }}
                            >
                                {isSavingProp ? <RefreshCw className="spin" size={16} /> : "Save Changes"}
                            </button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
