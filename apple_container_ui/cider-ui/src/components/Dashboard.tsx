"use client";

import React, { useState, useEffect, useMemo } from "react";
import { Link } from "react-router-dom";
import type { ContainerInfo } from "../lib/container";
import {
    checkSystemStatus,
    startSystem,
    stopSystem,
    listContainers,
    startContainer,
    stopContainer,
    removeContainer,
    getContainerStats,
    runContainer,
    createContainer
} from "../lib/container";
import { AlertDialog } from "@base-ui/react/alert-dialog";
import { Toast } from "@base-ui/react/toast";

import {
    RefreshCw,
    Search,
    Play,
    Square,
    Trash2,
    FileText,
    Terminal as TerminalIcon,
    RotateCcw,
    Rocket,
    Plus,
    Box,
    AlertCircle
} from 'lucide-react';

import "../Dashboard.css";

const toastManager = Toast.createToastManager();

export default function Dashboard() {
    return (
        <Toast.Provider toastManager={toastManager}>
            <DashboardContent />
        </Toast.Provider>
    );
}

function DashboardContent() {
    const { toasts } = Toast.useToastManager();
    const [systemRunning, setSystemRunning] = useState<boolean>(false);
    const [containers, setContainers] = useState<ContainerInfo[]>([]);
    const [stats, setStats] = useState<Record<string, any>>({});
    const [isLoading, setIsLoading] = useState<boolean>(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [searchQuery, setSearchQuery] = useState<string>("");
    const [statusFilter, setStatusFilter] = useState<string>("All");
    const [confirmAction, setConfirmAction] = useState<{ id: string, name: string } | null>(null);

    const refreshData = async () => {
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);

            if (isRunning) {
                const [cList, sList] = await Promise.all([
                    listContainers(),
                    getContainerStats()
                ]);
                setContainers(cList);

                const sMap: Record<string, any> = {};
                if (sList && Array.isArray(sList)) {
                    sList.forEach(s => sMap[s.id] = s);
                }
                setStats(sMap);
            } else {
                setContainers([]);
                setStats({});
            }
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        refreshData();
        const interval = setInterval(refreshData, 5000); // Poll every 5 seconds for real-time feel
        return () => clearInterval(interval);
    }, []);

    const filteredContainers = useMemo(() => {
        return containers.filter(c => {
            const matchesSearch = (c.Names || "").toLowerCase().includes(searchQuery.toLowerCase()) ||
                (c.ID || "").toLowerCase().includes(searchQuery.toLowerCase()) ||
                (c.Image || "").toLowerCase().includes(searchQuery.toLowerCase());

            const matchesStatus = statusFilter === "All" ||
                (statusFilter === "Running" && c.State === "running") ||
                (statusFilter === "Exited" && (c.State === "exited" || c.State === "stopped")) ||
                (statusFilter === "Healthy" && c.State === "running"); // Simplified healthy check

            return matchesSearch && matchesStatus;
        });
    }, [containers, searchQuery, statusFilter]);

    const handleContainerAction = async (id: string, action: "start" | "stop" | "remove") => {
        if (action === "remove") {
            const c = containers.find(item => item.ID === id);
            setConfirmAction({ id, name: c?.Names || id });
            return;
        }

        setActionLoading(id + action);
        try {
            if (action === "start") await startContainer(`container start ${id}`);
            if (action === "stop") await stopContainer(id);
            await refreshData();
        } catch (e) {
            toastManager.add({ title: "Action Failed", description: String(e), type: "error" });
        } finally {
            setActionLoading(null);
        }
    };

    const confirmDelete = async () => {
        if (!confirmAction) return;
        setActionLoading(confirmAction.id + "remove");
        try {
            await removeContainer(confirmAction.id);
            setConfirmAction(null);
            await refreshData();
        } catch (e) {
            toastManager.add({ title: "Removal Failed", description: String(e), type: "error" });
        } finally {
            setActionLoading(null);
        }
    };

    const getStatusCounters = () => {
        return {
            All: containers.length,
            Running: containers.filter(c => c.State === "running").length,
            Exited: containers.filter(c => c.State === "exited" || c.State === "stopped").length,
            Healthy: containers.filter(c => c.State === "running").length // Placeholder
        };
    };

    const counters = getStatusCounters();

    if (!systemRunning) {
        return (
            <div className="bg-background-light dark:bg-background-dark font-display min-h-screen flex flex-col overflow-x-hidden text-slate-900 dark:text-slate-100">
                <header className="flex items-center justify-between whitespace-nowrap border-b border-solid border-slate-200 dark:border-surface-border px-10 py-3 bg-white dark:bg-background-dark sticky top-0 z-50">
                    <div className="flex items-center gap-4 dark:text-white text-slate-900">
                        <div className="size-8 text-primary flex items-center justify-center">
                            <span className="material-symbols-outlined text-3xl">deployed_code</span>
                        </div>
                        <h2 className="text-lg font-bold leading-tight tracking-[-0.015em]">Docker Manager</h2>
                        <button
                            onClick={() => { startSystem() }}
                            className="flex min-w-[84px] cursor-pointer items-center justify-center overflow-hidden rounded-lg h-9 px-4 bg-primary hover:bg-primary-hover text-white text-sm font-bold leading-normal tracking-[0.015em] transition-colors shadow-lg shadow-violet-900/20"
                        >
                            <span className="flex items-center gap-2">
                                <span className="truncate">System Start</span>
                            </span>
                        </button>
                    </div>
                </header>
            </div>
        );
    }

    return (
        <div className="bg-background-light dark:bg-background-dark font-display min-h-screen flex flex-col overflow-x-hidden text-slate-900 dark:text-slate-100">
            <header className="flex items-center justify-between whitespace-nowrap border-b border-solid border-slate-200 dark:border-surface-border px-10 py-3 bg-white dark:bg-background-dark sticky top-0 z-50">
                <div className="flex items-center gap-4 dark:text-white text-slate-900">
                    <div className="size-8 text-primary flex items-center justify-center">
                        <span className="material-symbols-outlined text-3xl">deployed_code</span>
                    </div>
                    <h2 className="text-lg font-bold leading-tight tracking-[-0.015em]">Docker Manager</h2>
                </div>
                <div className="flex flex-1 justify-end gap-8">
                    <nav className="hidden md:flex items-center gap-9">
                        <Link className="text-sm font-medium leading-normal hover:text-primary transition-colors text-text-secondary" to="/registries">Registry</Link>
                        <Link className="text-sm font-medium leading-normal hover:text-primary transition-colors text-text-secondary" to="/settings">Settings</Link>
                        <Link className="text-sm font-medium leading-normal hover:text-primary transition-colors text-text-secondary" to="/images">Images</Link>
                        <Link className="text-sm font-medium leading-normal hover:text-primary transition-colors text-text-secondary" to="/volumes">Volumes</Link>
                        <Link className="text-sm font-medium leading-normal hover:text-primary transition-colors text-text-secondary" to="/networks">Networks</Link>
                    </nav>
                    <div className="flex items-center gap-4">
                        <button
                            onClick={() => { stopSystem() }}
                            className="flex min-w-[84px] cursor-pointer items-center justify-center overflow-hidden rounded-lg h-9 px-4 bg-primary hover:bg-primary-hover text-white text-sm font-bold leading-normal tracking-[0.015em] transition-colors shadow-lg shadow-violet-900/20"
                        >
                            <span className="flex items-center gap-2">
                                <span className="truncate">System Stop</span>
                            </span>
                        </button>
                        <div className="bg-center bg-no-repeat bg-cover rounded-full size-9 ring-2 ring-surface-border cursor-pointer"
                            style={{ backgroundImage: 'url("https://lh3.googleusercontent.com/aida-public/AB6AXuBUNbar0KRi2wwoj-G12zeDmUygKG1JDIZQEvok0pXeY7ATbGAa8vMRrf9BFWC8sCXKeQgGWTT6LAh7-YSso1o_t41sh55QYRMMVD68lsOwuPJ10FPZeBXDLOCnfaYGrxCffQHFzPlBuPVN0JUsC350XAFwm3qUGQqM6cCtr6vxJ2dZTYVQWWYwLujGf3wQCEDWHBj4E5UgJmyXR55UBodZT8Y2mZ9pkPbMsuV0WHoNYaL0fT7v_HWV9IJVEAbTQL_EPLDnRpE96vg")' }}>
                        </div>
                    </div>
                </div>
            </header>

            <main className="flex-1 px-4 md:px-10 py-8 max-w-[1400px] mx-auto w-full">
                <div className="flex flex-col gap-6 mb-8">
                    <div className="flex flex-wrap gap-2 items-center text-sm">
                        <Link className="text-text-secondary hover:text-white transition-colors" to="/">Home</Link>
                        <span className="text-text-secondary">/</span>
                        <Link className="text-text-secondary hover:text-white transition-colors" to="/">Projects</Link>
                        <span className="text-text-secondary">/</span>
                        <span className="dark:text-white text-slate-900 font-medium">web-app-prod</span>
                    </div>
                    <div className="flex flex-row md:flex-row justify-between items-start md:items-center gap-4">
                        <div className="flex flex-col gap-1">
                            <h1 className="text-3xl md:text-4xl font-black tracking-tight text-slate-900 dark:text-white">Containers</h1>
                            <p className="text-slate-500 dark:text-text-secondary">Manage your running containers and view real-time resource usage.</p>
                        </div>
                        <button className="flex items-center gap-2 h-10 px-4 bg-surface-dark border border-surface-border hover:bg-surface-border text-white text-sm font-bold rounded-lg transition-all">
                            <Rocket size={18} />
                            <span>Compose Up</span>
                        </button>
                    </div>
                </div>

                <div className="grid grid-cols-1 lg:grid-cols-12 gap-4 mb-6">
                    <div className="lg:col-span-5">
                        <div className="relative group">
                            <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none text-text-secondary group-focus-within:text-primary transition-colors">
                                <Search size={18} />
                            </div>
                            <input
                                className="block w-full pl-10 pr-3 py-2.5 bg-white dark:bg-surface-dark border border-slate-200 dark:border-surface-border rounded-lg leading-5 text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-text-secondary focus:outline-none focus:ring-1 focus:ring-primary focus:border-primary sm:text-sm transition-all shadow-sm"
                                placeholder="Search containers by name, id, or image..."
                                type="text"
                                value={searchQuery}
                                onChange={(e) => setSearchQuery(e.target.value)}
                            />
                        </div>
                    </div>
                    <div className="lg:col-span-7 flex items-center gap-2 overflow-x-auto custom-scrollbar pb-1 lg:pb-0">
                        {["All", "Running", "Exited", "Healthy"].map((filter) => (
                            <button
                                key={filter}
                                onClick={() => setStatusFilter(filter)}
                                className={`flex items-center gap-2 px-3 py-1.5 rounded-lg border transition-colors whitespace-nowrap text-sm font-medium ${statusFilter === filter
                                    ? "bg-primary/10 text-primary border-primary/20"
                                    : "bg-white dark:bg-surface-dark border-slate-200 dark:border-surface-border text-text-secondary hover:text-slate-900 dark:hover:text-white hover:border-slate-500"
                                    }`}
                            >
                                {filter} <span className={`${statusFilter === filter ? "bg-primary" : "bg-slate-200 dark:bg-surface-border"} ${statusFilter === filter ? "text-white" : "text-slate-500 dark:text-text-secondary"} text-xs px-1.5 rounded-md py-0.5`}>
                                    {(counters as any)[filter]}
                                </span>
                            </button>
                        ))}
                    </div>
                </div>

                <div className="bg-white dark:bg-surface-dark border border-slate-200 dark:border-surface-border rounded-xl shadow-sm overflow-hidden">
                    <div className="overflow-x-auto custom-scrollbar">
                        <table className="w-full text-left border-collapse">
                            <thead>
                                <tr className="border-b border-slate-200 dark:border-surface-border bg-slate-50 dark:bg-[#181818]">
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary w-16 text-center">Status</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary">Name</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary hidden sm:table-cell">Image ID</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary hidden md:table-cell">Ports</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary w-48 hidden lg:table-cell">CPU</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary w-48 hidden lg:table-cell">Memory</th>
                                    <th className="py-4 px-6 text-xs font-semibold uppercase tracking-wider text-slate-500 dark:text-text-secondary text-right">Actions</th>
                                </tr>
                            </thead>
                            <tbody className="divide-y divide-slate-200 dark:divide-surface-border">
                                {isLoading ? (
                                    <tr>
                                        <td colSpan={7} className="py-12 text-center text-text-secondary">
                                            <div className="flex flex-col items-center gap-3">
                                                <RefreshCw size={24} className="spin" />
                                                <span>Loading containers...</span>
                                            </div>
                                        </td>
                                    </tr>
                                ) : filteredContainers.length === 0 ? (
                                    <tr>
                                        <td colSpan={7} className="py-12 text-center text-text-secondary">
                                            <div className="flex flex-col items-center gap-3">
                                                <Box size={32} opacity={0.5} />
                                                <span>No containers found matching your search.</span>
                                            </div>
                                        </td>
                                    </tr>
                                ) : (
                                    filteredContainers.map((container) => {
                                        const cStats = stats[container.ID];
                                        const cpuUsage = cStats ? Math.min(Math.round(cStats.cpuUsageUsec / 10000), 100) : 0;
                                        const memUsageBytes = cStats?.memoryUsageBytes || 0;
                                        const memLimitBytes = cStats?.memoryLimitBytes || 1024 * 1024 * 1024; // Default 1GB
                                        const memPercent = Math.min(Math.round((memUsageBytes / memLimitBytes) * 100), 100);

                                        return (
                                            <tr key={container.ID} className="group hover:bg-slate-50 dark:hover:bg-[#262626] transition-colors">
                                                <td className="py-4 px-6 whitespace-nowrap">
                                                    <div className="relative flex items-center justify-center group-hover:scale-110 transition-transform">
                                                        <div className={`h-3 w-3 rounded-full ${container.State === "running" ? "bg-green-500 status-pulse-green" :
                                                            (container.State === "exited" || container.State === "stopped") ? "bg-slate-500" :
                                                                "bg-red-500 status-pulse-red"
                                                            }`}></div>
                                                    </div>
                                                </td>
                                                <td className="py-4 px-6">
                                                    <div className="flex flex-col">
                                                        <Link to={`/container/${container.ID}`} className="text-sm font-semibold text-slate-900 dark:text-white hover:text-primary transition-colors">
                                                            {container.Names || container.ID.substring(0, 12)}
                                                        </Link>
                                                        <span className="text-xs text-slate-500 dark:text-text-secondary">
                                                            {container.State === "running" ? `Up ${container.Status}` : container.Status}
                                                        </span>
                                                    </div>
                                                </td>
                                                <td className="py-4 px-6 hidden sm:table-cell">
                                                    <div className="flex items-center gap-2">
                                                        <span className="font-mono text-xs text-primary bg-primary/10 px-2 py-1 rounded">
                                                            {container.ID.substring(0, 12)}
                                                        </span>
                                                        <span className="text-xs text-slate-400 truncate max-w-[150px]" title={container.Image}>
                                                            {container.Image}
                                                        </span>
                                                    </div>
                                                </td>
                                                <td className="py-4 px-6 hidden md:table-cell">
                                                    <div className="flex flex-wrap gap-1">
                                                        {Array.isArray(container.Ports) && container.Ports.length > 0 ? (
                                                            container.Ports.map((p: any, idx) => (
                                                                <span key={idx} className="text-[10px] font-medium text-slate-700 dark:text-slate-300 bg-slate-100 dark:bg-surface-border px-1.5 py-0.5 rounded border border-slate-200 dark:border-neutral-700">
                                                                    {p.hostPort}:{p.containerPort}
                                                                </span>
                                                            ))
                                                        ) : (
                                                            <span className="text-xs text-slate-400">-</span>
                                                        )}
                                                    </div>
                                                </td>
                                                <td className="py-4 px-6 hidden lg:table-cell">
                                                    {container.State === "running" ? (
                                                        <div className="flex flex-col gap-1 w-full max-w-[120px]">
                                                            <div className="flex justify-between text-xs">
                                                                <span className="text-slate-500">Usage</span>
                                                                <span className={`font-medium ${cpuUsage > 80 ? "text-red-500" : cpuUsage > 50 ? "text-orange-400" : "text-green-400"}`}>
                                                                    {cpuUsage}%
                                                                </span>
                                                            </div>
                                                            <div className="h-1.5 w-full bg-slate-200 dark:bg-surface-border rounded-full overflow-hidden">
                                                                <div className={`h-full rounded-full transition-all duration-500 ${cpuUsage > 80 ? "bg-red-500" : cpuUsage > 50 ? "bg-orange-400" : "bg-green-500"}`} style={{ width: `${cpuUsage}%` }}></div>
                                                            </div>
                                                        </div>
                                                    ) : (
                                                        <div className="text-xs text-slate-400 dark:text-slate-600 italic">No usage data</div>
                                                    )}
                                                </td>
                                                <td className="py-4 px-6 hidden lg:table-cell">
                                                    {container.State === "running" ? (
                                                        <div className="flex flex-col gap-1 w-full max-w-[120px]">
                                                            <div className="flex justify-between text-xs">
                                                                <span className="text-slate-500">{(memUsageBytes / 1024 / 1024).toFixed(0)}MB</span>
                                                                <span className="font-medium text-primary">{memPercent}%</span>
                                                            </div>
                                                            <div className="h-1.5 w-full bg-slate-200 dark:bg-surface-border rounded-full overflow-hidden">
                                                                <div className="h-full bg-primary rounded-full transition-all duration-500" style={{ width: `${memPercent}%` }}></div>
                                                            </div>
                                                        </div>
                                                    ) : (
                                                        <div className="text-xs text-slate-400 dark:text-slate-600 italic">No usage data</div>
                                                    )}
                                                </td>
                                                <td className="py-4 px-6 text-right">
                                                    <div className="flex items-center justify-end gap-1 opacity-100 sm:opacity-0 sm:group-hover:opacity-100 transition-opacity">
                                                        <Link to={`/container/${container.ID}`} className="p-1.5 rounded hover:bg-slate-200 dark:hover:bg-surface-border text-slate-500 dark:text-text-secondary hover:text-slate-900 dark:hover:text-white transition-colors" title="Logs">
                                                            <FileText size={18} />
                                                        </Link>
                                                        <Link to={`/container/${container.ID}`} className="p-1.5 rounded hover:bg-slate-200 dark:hover:bg-surface-border text-slate-500 dark:text-text-secondary hover:text-slate-900 dark:hover:text-white transition-colors" title="Terminal">
                                                            <TerminalIcon size={18} />
                                                        </Link>
                                                        <button onClick={() => handleContainerAction(container.ID, "start")} className="p-1.5 rounded hover:bg-slate-200 dark:hover:bg-surface-border text-slate-500 dark:text-text-secondary hover:text-orange-400 transition-colors" title="Restart">
                                                            <RotateCcw size={18} />
                                                        </button>
                                                        {container.State === "running" ? (
                                                            <button onClick={() => handleContainerAction(container.ID, "stop")} className="p-1.5 rounded hover:bg-red-500/20 text-slate-500 dark:text-text-secondary hover:text-red-500 transition-colors" title="Stop">
                                                                <Square size={18} fill="currentColor" />
                                                            </button>
                                                        ) : (
                                                            <button onClick={() => handleContainerAction(container.ID, "start")} className="p-1.5 rounded hover:bg-green-500/20 text-slate-500 dark:text-text-secondary hover:text-green-500 transition-colors" title="Start">
                                                                <Play size={18} fill="currentColor" />
                                                            </button>
                                                        )}
                                                        <button onClick={() => handleContainerAction(container.ID, "remove")} className="p-1.5 rounded hover:bg-red-500/20 text-slate-500 dark:text-text-secondary hover:text-red-500 transition-colors" title="Remove">
                                                            <Trash2 size={18} />
                                                        </button>
                                                    </div>
                                                </td>
                                            </tr>
                                        );
                                    })
                                )}
                            </tbody>
                        </table>
                    </div>
                </div>
            </main>


            <AlertDialog.Root open={!!confirmAction} onOpenChange={(open) => !open && setConfirmAction(null)}>
                <AlertDialog.Portal>
                    <AlertDialog.Backdrop className="dialog-backdrop" />
                    <AlertDialog.Popup className="dialog-popup">
                        <AlertDialog.Title className="dialog-title flex items-center gap-2">
                            <AlertCircle className="text-red-500" />
                            Remove Container
                        </AlertDialog.Title>
                        <AlertDialog.Description className="dialog-description text-slate-600 dark:text-text-secondary">
                            Are you sure you want to remove <strong>{confirmAction?.name}</strong>? This action cannot be undone.
                        </AlertDialog.Description>
                        <div className="dialog-actions">
                            <AlertDialog.Close className="btn btn-secondary">Cancel</AlertDialog.Close>
                            <button className="btn btn-danger" onClick={confirmDelete} disabled={actionLoading?.includes("remove")}>
                                {actionLoading?.includes("remove") ? <RefreshCw size={16} className="spin" /> : "Remove"}
                            </button>
                        </div>
                    </AlertDialog.Popup>
                </AlertDialog.Portal>
            </AlertDialog.Root>

            <Toast.Portal>
                <Toast.Viewport className="toast-viewport">
                    {toasts.map((toast: any) => (
                        <Toast.Root key={toast.id} toast={toast} className={`toast-root toast-${toast.type || 'info'}`}>
                            <div className="toast-content">
                                <Toast.Title className="toast-title">{toast.title}</Toast.Title>
                                <Toast.Description className="toast-description">{toast.description}</Toast.Description>
                            </div>
                            <Toast.Close className="toast-close">×</Toast.Close>
                        </Toast.Root>
                    ))}
                </Toast.Viewport>
            </Toast.Portal>
        </div>
    );
}
