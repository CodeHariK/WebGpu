"use client";

import { useState, useEffect } from "react";
import Link from "next/link";
import {
    CornerLeftUp,
    Server,
    Trash2,
    RefreshCw,
    Download,
    Upload,
    Play,
    Square,
    Box,
    Tag,
    Save
} from "lucide-react";
import {
    listImages,
    deleteImage,
    pullImage,
    buildImage,
    saveImage,
    loadImage,
    tagImage,
    getBuilderStatus,
    startBuilder,
    stopBuilder,
    checkSystemStatus,
    ImageInfo,
    BuilderStatus
} from "@/lib/container";
import "../Dashboard.css";

export default function ImagesPage() {
    const [images, setImages] = useState<ImageInfo[]>([]);
    const [builderStatus, setBuilderStatus] = useState<BuilderStatus | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [actionLoading, setActionLoading] = useState<string | null>(null);
    const [systemRunning, setSystemRunning] = useState(false);

    // Form states
    const [pullImageName, setPullImageName] = useState("");
    const [buildDir, setBuildDir] = useState("");
    const [buildTag, setBuildTag] = useState("");

    // Quick load state
    const [tarPath, setTarPath] = useState("");

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const [imgs, buildStat] = await Promise.all([
                    listImages(),
                    getBuilderStatus()
                ]);
                setImages(imgs);
                setBuilderStatus(buildStat);
            } else {
                setImages([]);
                setBuilderStatus(null);
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

    const handleBuilderToggle = () => {
        if (builderStatus?.status === "running") {
            executeAction("stop-builder", () => stopBuilder());
        } else {
            executeAction("start-builder", () => startBuilder());
        }
    }

    const handlePull = () => {
        if (!pullImageName) return;
        executeAction("pull-img", async () => {
            await pullImage(pullImageName);
            setPullImageName("");
        });
    }

    const handleBuild = () => {
        if (!buildDir || !buildTag) return;
        executeAction("build-img", async () => {
            await buildImage(buildDir, buildTag);
            setBuildDir("");
            setBuildTag("");
        });
    }

    const handleLoad = () => {
        if (!tarPath) return;
        executeAction("load-tar", async () => {
            await loadImage(tarPath);
            setTarPath("");
        });
    }

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1200px', margin: '0 auto' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px" }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link href="/" className="btn-icon">
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Images & Builders</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card">
                    <Box size={48} color="var(--text-secondary)" opacity={0.5} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage images.</p>
                </div>
            ) : (
                <div style={{ display: 'grid', gridTemplateColumns: '300px 1fr', gap: '24px' }}>

                    {/* Left Sidebar Control Panel */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>

                        {/* Builder Status */}
                        <section className="premium-card">
                            <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>BuildKit Daemon</h2>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <span className={`status-indicator status-${builderStatus?.status === 'running' ? 'running' : 'stopped'}`} />
                                    <span style={{ fontSize: '14px' }}>{builderStatus?.status === 'running' ? "Running" : "Stopped"}</span>
                                </div>
                                <button
                                    className={`btn ${builderStatus?.status === 'running' ? 'btn-ghost' : 'btn-primary'}`}
                                    onClick={handleBuilderToggle}
                                    disabled={actionLoading === "start-builder" || actionLoading === "stop-builder"}
                                >
                                    {actionLoading?.includes("builder") ? <RefreshCw size={14} className="spin" /> : (builderStatus?.status === 'running' ? <Square size={14} /> : <Play size={14} />)}
                                    {builderStatus?.status === 'running' ? "Stop" : "Start"}
                                </button>
                            </div>
                            {builderStatus && (
                                <div style={{ fontSize: '13px', color: 'var(--text-secondary)' }}>
                                    <p>ID: {builderStatus.id}</p>
                                    {builderStatus.cpus && <p>CPUs: {builderStatus.cpus}</p>}
                                    {builderStatus.memoryInBytes && <p>Memory: {Math.round(builderStatus.memoryInBytes / 1024 / 1024 / 1024)}GB</p>}
                                </div>
                            )}
                        </section>

                        {/* Build Image */}
                        <section className="premium-card">
                            <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>Build Image</h2>
                            <div className="input-group" style={{ marginBottom: '12px' }}>
                                <label>Context Directory</label>
                                <input className="premium-input" placeholder="/path/to/project" value={buildDir} onChange={e => setBuildDir(e.target.value)} />
                            </div>
                            <div className="input-group" style={{ marginBottom: '16px' }}>
                                <label>Image Tag</label>
                                <input className="premium-input" placeholder="myapp:v1" value={buildTag} onChange={e => setBuildTag(e.target.value)} />
                            </div>
                            <button className="btn btn-primary" style={{ width: '100%' }} disabled={!buildDir || !buildTag || !!actionLoading} onClick={handleBuild}>
                                {actionLoading === "build-img" ? <RefreshCw size={14} className="spin" /> : <Box size={14} />}
                                Build Image
                            </button>
                        </section>

                        {/* Load Tarball */}
                        <section className="premium-card">
                            <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>Load External Image</h2>
                            <div className="input-group" style={{ marginBottom: '16px' }}>
                                <label>Tarball Path</label>
                                <input className="premium-input" placeholder="/path/to/image.tar" value={tarPath} onChange={e => setTarPath(e.target.value)} />
                            </div>
                            <button className="btn btn-ghost" style={{ width: '100%', border: '1px solid var(--border-color)' }} disabled={!tarPath || !!actionLoading} onClick={handleLoad}>
                                {actionLoading === "load-tar" ? <RefreshCw size={14} className="spin" /> : <Upload size={14} />}
                                Load Tarball
                            </button>
                        </section>

                    </div>


                    {/* Right Main Panel: Images List & Pull */}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>

                        <section className="premium-card" style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>Pull Remote Image</label>
                                <input
                                    className="premium-input"
                                    placeholder="docker.io/library/nginx:latest"
                                    value={pullImageName}
                                    onChange={e => setPullImageName(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handlePull()}
                                />
                            </div>
                            <button
                                className="btn btn-primary"
                                onClick={handlePull}
                                disabled={!pullImageName.trim() || !!actionLoading}
                            >
                                {actionLoading === "pull-img" ? <RefreshCw size={16} className="spin" /> : <Download size={16} />}
                                Pull Image
                            </button>
                        </section>

                        <main className="containers-list premium-card" style={{ flex: 1 }}>
                            <div className="list-header flex-between mb-4">
                                <h2 style={{ fontSize: '20px' }}>Your Local Images</h2>
                                <button className="btn btn-ghost" onClick={refreshData}>
                                    <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                                </button>
                            </div>

                            {images.length === 0 ? (
                                <div className="empty-state">
                                    <Box size={48} color="var(--text-secondary)" opacity={0.5} />
                                    <p>No images found.</p>
                                    <p className="subtitle">Pull or build an image to get started.</p>
                                </div>
                            ) : (
                                <div className="table-responsive">
                                    <table className="container-table">
                                        <thead>
                                            <tr>
                                                <th>Repository / Name</th>
                                                <th>Tag</th>
                                                <th>Size</th>
                                                <th style={{ textAlign: "right" }}>Actions</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            {images.map((img, i) => (
                                                <tr key={i} className="container-row">
                                                    <td style={{ fontWeight: 500, color: "var(--text-primary)" }}>{img.Name ? img.Name.split(":")[0] : img.Repository}</td>
                                                    <td style={{ color: "var(--text-secondary)", fontSize: "12px" }}>
                                                        <span className="premium-badge">{img.Tag}</span>
                                                    </td>
                                                    <td style={{ color: "var(--text-secondary)", fontSize: "13px" }}>{img.Size}</td>

                                                    <td style={{ textAlign: "right" }}>
                                                        <div className="action-buttons">
                                                            <button
                                                                className="btn-icon"
                                                                onClick={() => {
                                                                    const out = prompt("Enter path to save .tar file:", "/tmp/image.tar");
                                                                    if (out) executeAction("save-" + img.ID, () => saveImage(img.Name, out));
                                                                }}
                                                                disabled={!!actionLoading}
                                                                title="Save as Tarball"
                                                            >
                                                                {actionLoading === "save-" + img.ID ? <RefreshCw size={16} className="spin" /> : <Save size={16} />}
                                                            </button>
                                                            <button
                                                                className="btn-icon"
                                                                onClick={() => {
                                                                    const tg = prompt("Enter new tag for this image:", img.Repository + ":new-tag");
                                                                    if (tg) executeAction("tag-" + img.ID, () => tagImage(img.Name, tg));
                                                                }}
                                                                disabled={!!actionLoading}
                                                                title="Retag Image"
                                                            >
                                                                {actionLoading === "tag-" + img.ID ? <RefreshCw size={16} className="spin" /> : <Tag size={16} />}
                                                            </button>
                                                            <button
                                                                className="btn-icon text-danger"
                                                                onClick={() => {
                                                                    if (confirm(`Delete image ${img.Name}?`)) {
                                                                        executeAction("delete-" + img.ID, () => deleteImage(img.Name, true));
                                                                    }
                                                                }}
                                                                disabled={!!actionLoading}
                                                                title="Delete Image"
                                                            >
                                                                {actionLoading === "delete-" + img.ID ? <RefreshCw size={16} className="spin" /> : <Trash2 size={16} />}
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

                </div>
            )}
        </div>
    );
}
