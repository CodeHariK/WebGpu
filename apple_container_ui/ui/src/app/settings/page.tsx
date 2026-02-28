"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import { CornerLeftUp, Save, CheckCircle, Settings as SettingsIcon, Edit2, Trash2, RefreshCw } from "lucide-react";
import { listSystemProperties, setSystemProperty, clearSystemProperty, SystemProperty, checkSystemStatus } from "@/lib/container";
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
            await setSystemProperty(editingProp.ID, editValue);
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
            await clearSystemProperty(id);
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
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '800px', margin: '0 auto' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px" }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link href="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Settings</h1>
                </div>
            </header>

            <main className="premium-card">
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
                        style={{ maxWidth: '300px' }}
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
                    <button className="btn btn-primary" onClick={handleSave}>
                        <Save size={16} /> Save Settings
                    </button>
                    {saved && (
                        <span style={{ color: 'var(--status-running)', display: 'flex', alignItems: 'center', gap: '6px', fontSize: '14px' }}>
                            <CheckCircle size={16} /> Saved Successfully
                        </span>
                    )}
                </div>
            </main>

            <main className="premium-card" style={{ marginTop: '24px' }}>
                <div className="list-header flex-between mb-4">
                    <h2 style={{ fontSize: '18px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                        <SettingsIcon size={18} /> Global System Properties
                    </h2>
                    <button className="btn btn-ghost" onClick={loadProperties} disabled={!systemRunning || isLoadingProps}>
                        <RefreshCw size={16} className={isLoadingProps ? "spin" : ""} /> Refresh
                    </button>
                </div>

                {!systemRunning ? (
                    <div className="empty-state">
                        <p>The container daemon is offline.</p>
                        <p className="subtitle">Start the system to view properties.</p>
                    </div>
                ) : properties.length === 0 ? (
                    <div className="empty-state">
                        <p>No system properties found.</p>
                    </div>
                ) : (
                    <div className="table-responsive">
                        <table className="container-table">
                            <thead>
                                <tr>
                                    <th>Property ID</th>
                                    <th>Value</th>
                                    <th>Type</th>
                                    <th style={{ textAlign: "right" }}>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {properties.map((p) => (
                                    <tr key={p.ID} className="container-row">
                                        <td>
                                            <div style={{ fontWeight: 500, color: "var(--text-primary)" }}>{p.ID}</div>
                                            <div style={{ fontSize: '12px', color: "var(--text-secondary)", marginTop: '4px' }}>{p.Description}</div>
                                        </td>
                                        <td style={{ fontFamily: 'monospace' }}>
                                            {p.Value || <span style={{ color: 'var(--text-secondary)', fontStyle: 'italic' }}>default</span>}
                                        </td>
                                        <td><span className="status-badge">{p.Type}</span></td>
                                        <td style={{ textAlign: "right" }}>
                                            <div className="action-buttons">
                                                <button className="btn-icon text-primary" onClick={() => handleEditClick(p)} title="Edit">
                                                    <Edit2 size={16} />
                                                </button>
                                                {p.Value && (
                                                    <button className="btn-icon text-danger" onClick={() => handleClearProp(p.ID)} title="Clear (Revert to default)">
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
                <div className="modal-overlay">
                    <div className="modal-content premium-card animate-scale-in">
                        <h2 className="modal-title">Edit System Property</h2>
                        <div style={{ marginBottom: "16px" }}>
                            <p style={{ color: "var(--text-secondary)", fontSize: "14px", marginBottom: "8px" }}>
                                {editingProp.Description}
                            </p>
                            <label style={{ display: "block", marginBottom: "8px", fontWeight: 500 }}>
                                {editingProp.ID} <span style={{ opacity: 0.5, fontWeight: 'normal' }}>({editingProp.Type})</span>
                            </label>
                            <input
                                type="text"
                                className="premium-input w-full"
                                value={editValue}
                                onChange={(e) => setEditValue(e.target.value)}
                                autoFocus
                            />
                        </div>
                        <div className="modal-actions">
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
