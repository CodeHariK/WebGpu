"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import { CornerLeftUp, Save, CheckCircle } from "lucide-react";
import "../Dashboard.css";

export default function SettingsPage() {
    const [refreshInterval, setRefreshInterval] = useState<number>(60000);
    const [saved, setSaved] = useState(false);

    useEffect(() => {
        const stored = localStorage.getItem("refreshInterval");
        if (stored !== null) {
            setRefreshInterval(parseInt(stored, 10));
        }
    }, []);

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
        </div>
    );
}
