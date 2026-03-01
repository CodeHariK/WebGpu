"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
    CornerLeftUp,
    FileCode,
    RefreshCw,
    Play,
    Search,
    Loader2,
    Power
} from "lucide-react";
import { findDockerfiles, buildImage, checkSystemStatus, readFileContent } from "@/lib/container";
import "../Dashboard.css";

export default function DockerfilesPage() {
    const [dockerfiles, setDockerfiles] = useState<{ path: string; name: string }[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [buildLoading, setBuildLoading] = useState<string | null>(null);
    const [systemRunning, setSystemRunning] = useState(false);
    const [searchPath, setSearchPath] = useState("");
    const [previewFile, setPreviewFile] = useState<{ name: string; content: string } | null>(null);
    const [isPreviewLoading, setIsPreviewLoading] = useState(false);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const [isRunning, files] = await Promise.all([
                checkSystemStatus(),
                findDockerfiles(searchPath || undefined)
            ]);
            setSystemRunning(isRunning);
            setDockerfiles(files);
        } catch (e) {
            console.error(e);
        } finally {
            setIsLoading(false);
        }
    };

    useEffect(() => {
        refreshData();
    }, []);

    const handlePreview = async (path: string, name: string) => {
        setIsPreviewLoading(true);
        try {
            const content = await readFileContent(path);
            setPreviewFile({ name, content });
        } catch (e: any) {
            alert(`Failed to read file: ${e.message}`);
        } finally {
            setIsPreviewLoading(false);
        }
    };

    const handleBuild = async (path: string) => {
        // ... existing handleBuild logic

        const tag = prompt("Enter tag for this image (e.g., myapp:v1):");
        if (!tag) return;

        // Get the directory of the Dockerfile
        const dir = path.substring(0, path.lastIndexOf('/'));

        setBuildLoading(path);
        try {
            await buildImage(dir, tag);
            alert(`Successfully built ${tag}`);
        } catch (e: any) {
            alert(`Build failed: ${e.message}`);
        } finally {
            setBuildLoading(null);
        }
    };

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1000px', margin: '0 auto' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px" }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link href="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Dockerfile Discovery</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            <section className="premium-card">
                <div className="list-header mb-4">
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', width: '100%', marginBottom: '16px' }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                            <Search size={20} color="var(--text-secondary)" />
                            <h2 style={{ fontSize: '18px', margin: 0 }}>Discovered Dockerfiles</h2>
                            <span className="premium-badge">{dockerfiles.length} found</span>
                        </div>
                        <button className="btn btn-ghost" onClick={refreshData} disabled={isLoading}>
                            <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                        </button>
                    </div>

                    <div className="flex-center" style={{ gap: '12px', width: '100%' }}>
                        <input
                            className="premium-input"
                            style={{ flex: 1 }}
                            placeholder="Working directory (optional, e.g. ./docs)"
                            value={searchPath}
                            onChange={(e) => setSearchPath(e.target.value)}
                            onKeyDown={(e) => e.key === 'Enter' && refreshData()}
                        />
                        <button className="btn btn-secondary" onClick={refreshData} disabled={isLoading}>
                            Search
                        </button>
                    </div>
                </div>

                {isLoading ? (
                    <div className="flex-center" style={{ padding: '60px', flexDirection: 'column', gap: '16px' }}>
                        <Loader2 size={40} className="spin" color="var(--accent-primary)" />
                        <p style={{ color: 'var(--text-secondary)' }}>Scanning project for Dockerfiles...</p>
                    </div>
                ) : dockerfiles.length === 0 ? (
                    <div className="empty-state" style={{ padding: '60px' }}>
                        <FileCode size={48} color="var(--text-secondary)" opacity={0.3} />
                        <p>No Dockerfiles found in the workspace.</p>
                        <p className="subtitle">Dockerfiles, Containerfiles, and *.Dockerfile are supported.</p>
                    </div>
                ) : (
                    <div className="table-responsive">
                        <table className="container-table">
                            <thead>
                                <tr>
                                    <th>Location (Click to Preview)</th>
                                    <th style={{ textAlign: "right" }}>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {dockerfiles.map((df, i) => (
                                    <tr key={i} className="container-row">
                                        <td
                                            onClick={() => handlePreview(df.path, df.name)}
                                            style={{ cursor: 'pointer' }}
                                        >
                                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                                <div className="flex-center" style={{ width: '32px', height: '32px', background: 'rgba(255,255,255,0.05)', borderRadius: '6px' }}>
                                                    {isPreviewLoading && previewFile?.name === df.name ? <Loader2 size={16} className="spin" /> : <FileCode size={16} />}
                                                </div>
                                                <div>
                                                    <div style={{ fontWeight: 500 }}>{df.name.split('/').pop()}</div>
                                                    <div style={{ fontSize: '12px', color: 'var(--text-secondary)', fontFamily: 'monospace' }}>{df.name}</div>
                                                </div>
                                            </div>
                                        </td>
                                        <td style={{ textAlign: "right" }}>
                                            <button
                                                className="btn btn-primary"
                                                onClick={(e) => { e.stopPropagation(); handleBuild(df.path); }}
                                                disabled={!systemRunning || !!buildLoading}
                                            >
                                                {buildLoading === df.path ? <RefreshCw size={14} className="spin" /> : <Play size={14} />}
                                                Build Image
                                            </button>
                                        </td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>
                )}
            </section>

            {!systemRunning && !isLoading && (
                <div className="premium-card" style={{ marginTop: '24px', border: '1px solid var(--danger-color)', display: 'flex', gap: '16px', alignItems: 'center', background: 'rgba(239, 68, 68, 0.05)' }}>
                    <Power size={24} color="var(--danger-color)" />
                    <div>
                        <h3 style={{ fontSize: '16px', margin: 0, color: 'var(--danger-color)' }}>Daemon Offline</h3>
                        <p style={{ fontSize: '14px', margin: '4px 0 0 0', opacity: 0.8 }}>You must start the container daemon from the dashboard to build images.</p>
                    </div>
                </div>
            )}
            {previewFile && (
                <div className="modal-overlay" onClick={() => setPreviewFile(null)}>
                    <div className="premium-card animate-scale-in" style={{ width: '80%', maxWidth: '800px', maxHeight: '80vh', display: 'flex', flexDirection: 'column', padding: 0 }} onClick={e => e.stopPropagation()}>
                        <div className="flex-between" style={{ padding: '20px', borderBottom: '1px solid var(--border-color)' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                                <FileCode size={20} color="var(--accent-primary)" />
                                <h2 style={{ fontSize: '18px', margin: 0 }}>{previewFile.name}</h2>
                            </div>
                            <button className="btn-icon" onClick={() => setPreviewFile(null)}>
                                <span style={{ fontSize: '24px' }}>&times;</span>
                            </button>
                        </div>
                        <div className="terminal-logs" style={{ flex: 1, overflowY: 'auto', padding: '20px', margin: 0, background: '#0a0a0a', borderBottomLeftRadius: '12px', borderBottomRightRadius: '12px' }}>
                            <pre style={{ margin: 0, color: '#e0e0e0', fontSize: '13px', lineHeight: 1.6 }}>{previewFile.content}</pre>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
