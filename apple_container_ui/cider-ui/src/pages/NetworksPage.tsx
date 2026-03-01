import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import {
    CornerLeftUp,
    Network,
    Trash2,
    RefreshCw,
    Plus
} from "lucide-react";
import {
    listNetworks,
    createNetwork,
    removeNetwork,
    checkSystemStatus,
    listDnsDomains,
    createDnsDomain,
    deleteDnsDomain
} from "../lib/container";
import type {
    NetworkInfo
} from "../lib/container";
import "../Dashboard.css";

export default function NetworksPage() {
    const [networks, setNetworks] = useState<NetworkInfo[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [newNetworkName, setNewNetworkName] = useState("");
    const [systemRunning, setSystemRunning] = useState(false);

    // DNS Domains State
    const [dnsDomains, setDnsDomains] = useState<string[]>([]);
    const [newDnsDomain, setNewDnsDomain] = useState("");
    const [isLoadingDns, setIsLoadingDns] = useState(false);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const nets = await listNetworks();

                // Sort builtin networks to the top, then alphabetically
                nets.sort((a, b) => {
                    const aIsBuiltin = a.config.labels?.["com.apple.container.resource.role"] === "builtin";
                    const bIsBuiltin = b.config.labels?.["com.apple.container.resource.role"] === "builtin";

                    if (aIsBuiltin && !bIsBuiltin) return -1;
                    if (!aIsBuiltin && bIsBuiltin) return 1;
                    return a.id.localeCompare(b.id);
                });

                setNetworks(nets);

                // Fetch DNS Domains concurrently
                fetchDnsDomains();
            } else {
                setNetworks([]);
                setDnsDomains([]);
            }
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoading(false);
        }
    };

    const fetchDnsDomains = async () => {
        setIsLoadingDns(true);
        try {
            const domains = await listDnsDomains();
            setDnsDomains(domains);
        } catch (e) {
            console.error("Failed to fetch DNS domains:", e);
        } finally {
            setIsLoadingDns(false);
        }
    };

    useEffect(() => {
        refreshData();
        const interval = setInterval(refreshData, 30000);
        return () => clearInterval(interval);
    }, []);

    const handleCreate = async () => {
        if (!newNetworkName.trim()) return;
        setActionLoading("create");
        try {
            await createNetwork(newNetworkName.trim());
            setNewNetworkName("");
            refreshData();
        } catch (e: any) {
            alert(e);
        } finally {
            setActionLoading(null);
        }
    };

    const handleDelete = async (name: string) => {
        if (!confirm(`Are you sure you want to delete network '${name}'?`)) return;
        setActionLoading("delete-" + name);
        try {
            await removeNetwork(name);
            refreshData();
        } catch (e: any) {
            alert(e);
        } finally {
            setActionLoading(null);
        }
    };

    const handleCreateDns = async () => {
        if (!newDnsDomain.trim()) return;
        setActionLoading("create-dns");
        try {
            await createDnsDomain(newDnsDomain.trim());
            setNewDnsDomain("");
            refreshData();
        } catch (e: any) {
            alert(e);
        } finally {
            setActionLoading(null);
        }
    };

    const handleDeleteDns = async (domain: string) => {
        if (!confirm(`Are you sure you want to delete local DNS domain '${domain}'? (Requires admin privileges)`)) return;
        setActionLoading("delete-dns-" + domain);
        try {
            await deleteDnsDomain(domain);
            refreshData();
        } catch (e: any) {
            alert(e);
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
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Container Networks</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px', display: 'flex', alignItems: 'center' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card" style={{ textAlign: 'center', padding: '48px' }}>
                    <Network size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage networks.</p>
                </div>
            ) : (
                <>
                    <section className="create-container-section premium-card mb-4" style={{ marginBottom: '24px' }}>
                        <h2 style={{ fontSize: '18px', marginBottom: '16px' }}>Create User Network</h2>
                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Network Name</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., custom-bridge"
                                    value={newNetworkName}
                                    onChange={e => setNewNetworkName(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handleCreate()}
                                    style={{ width: '100%' }}
                                />
                            </div>
                            <div>
                                <button
                                    className="btn btn-primary"
                                    onClick={handleCreate}
                                    disabled={!newNetworkName.trim() || actionLoading === "create"}
                                    style={{ height: '40px', padding: '0 24px' }}
                                >
                                    {actionLoading === "create" ? <RefreshCw size={16} className="spin" /> : <Plus size={16} />}
                                    Create Network
                                </button>
                            </div>
                        </div>
                    </section>

                    <main className="containers-list premium-card">
                        <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <h2 style={{ fontSize: '20px', margin: 0 }}>Network Interfaces</h2>
                            <button className="btn btn-ghost" onClick={refreshData}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {networks.length === 0 ? (
                            <div className="empty-state" style={{ textAlign: 'center', padding: '48px' }}>
                                <Network size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                                <p>No networks found.</p>
                                <p className="subtitle">Create one to isolate container traffic.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                            <th style={{ padding: '12px' }}>Network Name</th>
                                            <th style={{ padding: '12px' }}>Subnet (IPv4)</th>
                                            <th style={{ padding: '12px' }}>Gateway (IPv4)</th>
                                            <th style={{ padding: '12px' }}>Driver / Mode</th>
                                            <th style={{ padding: '12px', textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {networks.map(net => {
                                            const isSystem = net.config.labels?.["com.apple.container.resource.role"] === "builtin";
                                            return (
                                                <tr key={net.id} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                                    <td style={{ padding: '12px' }}>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <span style={{ fontWeight: 500, color: "var(--text-primary)" }}>{net.id}</span>
                                                            {isSystem && (
                                                                <span className="premium-badge">System</span>
                                                            )}
                                                        </div>
                                                    </td>
                                                    <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>{net.status.ipv4Subnet || "-"}</td>
                                                    <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>{net.status.ipv4Gateway || "-"}</td>
                                                    <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>
                                                        {net.config.pluginInfo?.plugin || "unknown"} ({net.config.mode || "unknown"})
                                                    </td>
                                                    <td style={{ padding: '12px', textAlign: "right" }}>
                                                        <div className="action-buttons" style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                                                            <button
                                                                className="btn-icon text-danger"
                                                                onClick={() => handleDelete(net.id)}
                                                                disabled={isSystem || actionLoading === "delete-" + net.id}
                                                                title={isSystem ? "Cannot delete system networks" : "Delete Network"}
                                                                style={{ opacity: isSystem ? 0.3 : 1, cursor: isSystem ? 'not-allowed' : 'pointer' }}
                                                            >
                                                                {actionLoading === "delete-" + net.id ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
                                                            </button>
                                                        </div>
                                                    </td>
                                                </tr>
                                            );
                                        })}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </main>

                    <section className="premium-card" style={{ marginTop: '24px' }}>
                        <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                            <h2 style={{ fontSize: '20px', margin: 0 }}>Local DNS Domains</h2>
                            <button className="btn btn-ghost" onClick={fetchDnsDomains} disabled={isLoadingDns}>
                                <RefreshCw size={16} className={isLoadingDns ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end', marginBottom: '16px' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Add DNS Domain</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., containers.local"
                                    value={newDnsDomain}
                                    onChange={e => setNewDnsDomain(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handleCreateDns()}
                                    style={{ width: '100%' }}
                                />
                            </div>
                            <div>
                                <button
                                    className="btn btn-primary"
                                    onClick={handleCreateDns}
                                    disabled={!newDnsDomain.trim() || actionLoading === "create-dns"}
                                    style={{ height: '40px', padding: '0 24px' }}
                                >
                                    {actionLoading === "create-dns" ? <RefreshCw size={16} className="spin" /> : <Plus size={16} />}
                                    Add Domain
                                </button>
                            </div>
                        </div>

                        {dnsDomains.length === 0 ? (
                            <div className="empty-state" style={{ textAlign: 'center', padding: '24px 0' }}>
                                <p>No local DNS domains configured.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                    <thead>
                                        <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                            <th style={{ padding: '12px' }}>Domain Name</th>
                                            <th style={{ padding: '12px', textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {dnsDomains.map((domain, i) => (
                                            <tr key={i} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                                <td style={{ padding: '12px', fontWeight: 500, color: "var(--text-primary)" }}>{domain}</td>
                                                <td style={{ padding: '12px', textAlign: "right" }}>
                                                    <div className="action-buttons" style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                                                        <button
                                                            className="btn-icon text-danger"
                                                            onClick={() => handleDeleteDns(domain)}
                                                            disabled={actionLoading === "delete-dns-" + domain}
                                                            title="Delete Domain"
                                                        >
                                                            {actionLoading === "delete-dns-" + domain ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
                                                        </button>
                                                    </div>
                                                </td>
                                            </tr>
                                        ))}
                                    </tbody>
                                </table>
                            </div>
                        )}
                    </section>
                </>
            )}
        </div>
    );
}
