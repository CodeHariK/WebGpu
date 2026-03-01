import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import {
    CornerLeftUp,
    KeySquare,
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
} from "../lib/container";
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
    }, []);

    const executeAction = async (actionId: string, func: () => Promise<any>) => {
        setActionLoading(actionId);
        try {
            const res = await func();
            if (!res.ok) {
                const err = await res.text();
                throw new Error(err || "Action failed");
            }
            await refreshData();
        } catch (e: any) {
            alert(e.message || "An error occurred");
        } finally {
            setActionLoading(null);
        }
    };

    const handleLogin = () => {
        if (!server || !username || !password) return;
        executeAction("login", () => registryLogin(server, username, password));
        setServer("");
        setUsername("");
        setPassword("");
    }

    const handleLogout = (regServer: string) => {
        if (!confirm(`Are you sure you want to log out of ${regServer}?`)) return;
        executeAction("logout-" + regServer, () => registryLogout(regServer));
    }

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1000px', margin: '0 auto', padding: '24px' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '16px 24px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Registries</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px', display: 'flex', alignItems: 'center' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card" style={{ padding: '60px', textAlign: 'center' }}>
                    <KeySquare size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto' }} />
                    <p style={{ marginTop: '16px' }}>The container daemon is offline.</p>
                    <p className="subtitle" style={{ fontSize: '13px', color: 'var(--text-secondary)' }}>Start the system from the dashboard to manage registry credentials.</p>
                </div>
            ) : (
                <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: '24px' }}>

                    {/* Login Form */}
                    <section className="premium-card" style={{ padding: '24px' }}>
                        <h2 style={{ fontSize: '18px', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                            <Lock size={18} /> Add Registry Login
                        </h2>
                        <div style={{ display: 'flex', gap: '16px', flexWrap: 'wrap' }}>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label style={{ display: 'block', marginBottom: '8px', fontSize: '14px', color: 'var(--text-secondary)' }}>Registry Server</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., docker.io or ghcr.io"
                                    value={server}
                                    style={{ width: '100%', padding: '10px 16px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px', color: 'white' }}
                                    onChange={e => setServer(e.target.value)}
                                />
                            </div>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label style={{ display: 'block', marginBottom: '8px', fontSize: '14px', color: 'var(--text-secondary)' }}>Username</label>
                                <input
                                    className="premium-input"
                                    placeholder="Username"
                                    value={username}
                                    style={{ width: '100%', padding: '10px 16px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px', color: 'white' }}
                                    onChange={e => setUsername(e.target.value)}
                                />
                            </div>
                            <div className="input-group" style={{ flex: '1 1 200px' }}>
                                <label style={{ display: 'block', marginBottom: '8px', fontSize: '14px', color: 'var(--text-secondary)' }}>Password / Token</label>
                                <input
                                    className="premium-input"
                                    type="password"
                                    placeholder="Password"
                                    value={password}
                                    style={{ width: '100%', padding: '10px 16px', background: 'rgba(255,255,255,0.05)', border: '1px solid var(--border-color)', borderRadius: '8px', color: 'white' }}
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
                                style={{ display: 'inline-flex', alignItems: 'center', gap: '8px' }}
                            >
                                {actionLoading === "login" ? <RefreshCw size={16} className="spin" /> : <LogIn size={16} />}
                                Login
                            </button>
                        </div>
                    </section>

                    {/* Registries List */}
                    <main className="premium-card" style={{ padding: '24px' }}>
                        <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <h2 style={{ fontSize: '20px', margin: 0 }}>Active Sessions</h2>
                            <button className="btn btn-ghost" onClick={refreshData} disabled={isLoading} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {registries.length === 0 ? (
                            <div className="empty-state" style={{ padding: '40px', textAlign: 'center' }}>
                                <KeySquare size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto' }} />
                                <p style={{ marginTop: '16px' }}>No active registry sessions found.</p>
                                <p className="subtitle" style={{ fontSize: '13px', color: 'var(--text-secondary)' }}>Login above to authenticate with a registry.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                            <th style={{ padding: '12px' }}>Server</th>
                                            <th style={{ textAlign: "right", padding: '12px' }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {registries.map((reg, i) => (
                                            <tr key={i} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                                <td style={{ fontWeight: 500, color: "var(--text-primary)", padding: '12px' }}>
                                                    {reg}
                                                </td>
                                                <td style={{ textAlign: "right", padding: '12px' }}>
                                                    <button
                                                        className="btn-icon text-danger"
                                                        onClick={() => handleLogout(reg)}
                                                        disabled={!!actionLoading}
                                                        title="Logout"
                                                        style={{ background: 'transparent', border: 'none', color: 'var(--danger-color)', cursor: 'pointer' }}
                                                    >
                                                        {actionLoading === "logout-" + reg ? <RefreshCw size={16} className="spin" /> : <LogOut size={16} />}
                                                    </button>
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
