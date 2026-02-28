"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
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
    NetworkInfo,
    checkSystemStatus,
    listDnsDomains,
    createDnsDomain,
    deleteDnsDomain
} from "@/lib/container";
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
        const stored = localStorage.getItem("refreshInterval");
        const intervalMs = stored ? parseInt(stored, 10) : 60000;
        if (intervalMs > 0) {
            const interval = setInterval(refreshData, intervalMs);
            return () => clearInterval(interval);
        }
    }, []);

    const handleCreate = async () => {
        if (!newNetworkName.trim()) return;
        setActionLoading("create");
        try {
            await createNetwork(newNetworkName.trim());
            setNewNetworkName("");
            await refreshData();
        } catch (e: any) {
            alert(e.message);
        } finally {
            setActionLoading(null);
        }
    };

    const handleDelete = async (name: string) => {
        if (!confirm(`Are you sure you want to delete network '${name}'?`)) return;
        setActionLoading("delete-" + name);
        try {
            await removeNetwork(name, true);
            await refreshData();
        } catch (e: any) {
            alert(e.message);
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
            await fetchDnsDomains();
        } catch (e: any) {
            alert(e.message || "Failed to create DNS domain");
        } finally {
            setActionLoading(null);
        }
    };

    const handleDeleteDns = async (domain: string) => {
        if (!confirm(`Are you sure you want to delete local DNS domain '${domain}'? (Requires admin privileges)`)) return;
        setActionLoading("delete-dns-" + domain);
        try {
            await deleteDnsDomain(domain);
            await fetchDnsDomains();
        } catch (e: any) {
            alert(e.message || "Failed to delete DNS domain");
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
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Container Networks</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card">
                    <Network size={48} color="var(--text-secondary)" opacity={0.5} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage networks.</p>
                </div>
            ) : (
                <>
                    <section className="create-container-section premium-card mb-4" style={{ marginTop: '-4px' }}>
                        <h2 style={{ fontSize: '18px', marginBottom: '16px' }}>Create User Network</h2>
                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>Network Name</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., custom-bridge"
                                    value={newNetworkName}
                                    onChange={e => setNewNetworkName(e.target.value)}
                                    // Submit on Enter key
                                    onKeyDown={e => e.key === 'Enter' && handleCreate()}
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
                        <div className="list-header flex-between mb-4">
                            <h2 style={{ fontSize: '20px' }}>Network Interfaces</h2>
                            <button className="btn btn-ghost" onClick={refreshData}>
                                <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        {networks.length === 0 ? (
                            <div className="empty-state">
                                <Network size={48} color="var(--text-secondary)" opacity={0.5} />
                                <p>No networks found.</p>
                                <p className="subtitle">Create one to isolate container traffic.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table">
                                    <thead>
                                        <tr>
                                            <th>Network Name</th>
                                            <th>Subnet (IPv4)</th>
                                            <th>Gateway (IPv4)</th>
                                            <th>Driver / Mode</th>
                                            <th style={{ textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {networks.map(net => {
                                            const isSystem = net.config.labels?.["com.apple.container.resource.role"] === "builtin";
                                            return (
                                                <tr key={net.id} className="container-row">
                                                    <td>
                                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                                            <span style={{ fontWeight: 500, color: "var(--text-primary)" }}>{net.id}</span>
                                                            {isSystem && (
                                                                <span style={{ fontSize: '10px', padding: '2px 6px', background: 'rgba(255,255,255,0.1)', borderRadius: '4px', color: 'var(--text-secondary)' }}>System</span>
                                                            )}
                                                        </div>
                                                    </td>
                                                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{net.status.ipv4Subnet || "-"}</td>
                                                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{net.status.ipv4Gateway || "-"}</td>
                                                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                                                        {net.config.pluginInfo?.plugin || "unknown"} ({net.config.mode || "unknown"})
                                                    </td>
                                                    <td style={{ textAlign: "right" }}>
                                                        <div className="action-buttons">
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

                    {/* DNS Management Section */}
                    <section className="premium-card" style={{ marginTop: '24px' }}>
                        <div className="list-header flex-between mb-4">
                            <h2 style={{ fontSize: '20px' }}>Local DNS Domains</h2>
                            <button className="btn btn-ghost" onClick={fetchDnsDomains} disabled={isLoadingDns}>
                                <RefreshCw size={16} className={isLoadingDns ? "spin" : ""} /> Refresh
                            </button>
                        </div>

                        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end', marginBottom: '16px' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>Add DNS Domain (sudo maybe required)</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., containers.local"
                                    value={newDnsDomain}
                                    onChange={e => setNewDnsDomain(e.target.value)}
                                    // Submit on Enter key
                                    onKeyDown={e => e.key === 'Enter' && handleCreateDns()}
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
                            <div className="empty-state" style={{ padding: '24px 0' }}>
                                <p>No local DNS domains configured.</p>
                            </div>
                        ) : (
                            <div className="table-responsive">
                                <table className="container-table">
                                    <thead>
                                        <tr>
                                            <th>Domain Name</th>
                                            <th style={{ textAlign: "right" }}>Actions</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        {dnsDomains.map((domain, i) => (
                                            <tr key={i} className="container-row">
                                                <td style={{ fontWeight: 500, color: "var(--text-primary)" }}>{domain}</td>
                                                <td style={{ textAlign: "right" }}>
                                                    <div className="action-buttons">
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
