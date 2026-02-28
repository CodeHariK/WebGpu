"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
    CornerLeftUp,
    KeySquare,
    Trash2,
    RefreshCw,
    LogIn,
    LogOut,
    Lock
} from "lucide-react";
import {
    listRegistries,
    registryLogin,
    registryLogout,
    checkSystemStatus
} from "@/lib/container";
import "../Dashboard.css";

export default function RegistriesPage() {
    const [registries, setRegistries] = useState<string[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [systemRunning, setSystemRunning] = useState(false);

    // Form states
    const [server, setServer] = useState("");
    const [username, setUsername] = useState("");
    const [password, setPassword] = useState("");

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const regs = await listRegistries();
                setRegistries(regs);
            } else {
                setRegistries([]);
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

    const executeAction = async (actionId: string, func: () => Promise<any>) => {
        setActionLoading(actionId);
        try {
            await func();
            await refreshData();
        } catch (e: any) {
            alert(e.message || "An error occurred");
        } finally {
            setActionLoading(null);
        }
    };

    const handleLogin = () => {
        if (!server || !username || !password) return;
        executeAction("login", async () => {
            await registryLogin(server, username, password);
            setServer("");
            setUsername("");
            setPassword("");
        });
    }

    const handleLogout = (regServer: string) => {
        if (!confirm(`Are you sure you want to log out of ${regServer}?`)) return;
        executeAction("logout-" + regServer, () => registryLogout(regServer));
    }

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1000px', margin: '0 auto' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px" }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link href="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Registries</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card">
                    <KeySquare size={48} color="var(--text-secondary)" opacity={0.5} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage registry credentials.</p>
                </div>
            ) : (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '24px' }}>

                    {/* Login Form */}
                    <section className="premium-card">
                        <h2 style={{ fontSize: '18px', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                            <Lock size={18} /> Add Registry Login
                        </h2>
                        <div style={{ display: 'flex', gap: '16px', flexWrap: 'wrap' }}>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label>Registry Server</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., docker.io or ghcr.io"
                                    value={server}
                                    onChange={e => setServer(e.target.value)}
                                />
                            </div>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label>Username</label>
                                <input
                                    className="premium-input"
                                    placeholder="Username"
                                    value={username}
                                    onChange={e => setUsername(e.target.value)}
                                />
                            </div>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label>Password / Token</label>
                                <input
                                    className="premium-input"
                                    type="password"
                                    placeholder="Password"
                                    value={password}
                                    onChange={e => setPassword(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handleLogin()}
                                />
                            </div>
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: '16px' }}>
                            <button
                                className="btn btn-primary"
                                onClick={handleLogin}
                                disabled={!server || !username || !password || !!actionLoading}
                            >
                                {actionLoading === "login" ? <RefreshCw size={16} className="spin" /> : <LogIn size={16} />}
                                Login
                            </button>
                        </div>
                    </section>

                    {/* Registries List */}
                    <main className="containers-list premium-card">
                        <div className="list-header flex-between mb-4">
                            <h2 style={{ fontSize: '20px' }}>Active Sessions</h2>
                            <button className="btn btn-ghost" onClick={refreshData}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {registries.length === 0 ? (
                            <div className="empty-state">
                                <KeySquare size={48} color="var(--text-secondary)" opacity={0.5} />
                                <p>No active registry sessions found.</p>
                                <p className="subtitle">Login above to authenticate with a registry.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table">
                                    <thead>
                                        <tr>
                                            <th>Server</th>
                                            <th style={{ textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {registries.map((reg, i) => (
                                            <tr key={i} className="container-row">
                                                <td style={{ fontWeight: 500, color: "var(--text-primary)" }}>
                                                    {reg}
                                                </td>
                                                <td style={{ textAlign: "right" }}>
                                                    <div className="action-buttons">
                                                        <button
                                                            className="btn-icon text-danger"
                                                            onClick={() => handleLogout(reg)}
                                                            disabled={!!actionLoading}
                                                            title="Logout"
                                                        >
                                                            {actionLoading === "logout-" + reg ? <RefreshCw size={16} className="spin" /> : <LogOut size={16} />}
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

                </div>
            )}
        </div>
    );
}
