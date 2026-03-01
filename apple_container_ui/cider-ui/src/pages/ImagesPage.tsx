import { useState, useEffect } from "react";
import { Link } from "react-router-dom";
import {
    CornerLeftUp,
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
    loadImage,
    saveImage,
    tagImage,
    getBuilderStatus,
    startBuilder,
    stopBuilder,
    checkSystemStatus
} from "../lib/container";
import type {
    ImageInfo,
    BuilderStatus
} from "../lib/container";
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
        const interval = setInterval(refreshData, 30000);
        return () => clearInterval(interval);
    }, []);

    const executeAction = async (actionId: string, func: () => Promise<any>) => {
        setActionLoading(actionId);
        try {
            const res = await func();
            if (!res.ok) {
                const text = await res.text();
                throw new Error(text || "Action failed");
            }
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
        executeAction("pull-img", async () => pullImage(pullImageName));
    }

    const handleBuild = () => {
        if (!buildDir || !buildTag) return;
        executeAction("build-img", async () => buildImage(buildDir, buildTag));
    }

    const handleLoad = () => {
        if (!tarPath) return;
        executeAction("load-tar", async () => loadImage(tarPath));
    }

    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1200px', margin: '0 auto', padding: '24px' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/" className="btn-icon" style={{ color: 'inherit' }}>
                        <CornerLeftUp size={20} />
                    </Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Images & Builders</h1>
                </div>

                <div className="status-badge flex-center" style={{ gap: '8px', display: 'flex', alignItems: 'center' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            {!systemRunning ? (
                <div className="empty-state premium-card" style={{ textAlign: 'center', padding: '48px' }}>
                    <Box size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                    <p>The container daemon is offline.</p>
                    <p className="subtitle">Start the system from the dashboard to manage images.</p>
                </div>
            ) : (
                <div style={{ display: 'grid', gridTemplateColumns: 'minmax(300px, 1fr) 3fr', gap: '24px' }}>

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
                        </section>

                        {/* Build Image */}
                        <section className="premium-card">
                            <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>Build Image</h2>
                            <div className="input-group" style={{ marginBottom: '12px' }}>
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Context Directory</label>
                                <input className="premium-input" placeholder="/path/to/project" value={buildDir} onChange={e => setBuildDir(e.target.value)} style={{ width: '100%' }} />
                            </div>
                            <div className="input-group" style={{ marginBottom: '16px' }}>
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Image Tag</label>
                                <input className="premium-input" placeholder="myapp:v1" value={buildTag} onChange={e => setBuildTag(e.target.value)} style={{ width: '100%' }} />
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
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Tarball Path</label>
                                <input className="premium-input" placeholder="/path/to/image.tar" value={tarPath} onChange={e => setTarPath(e.target.value)} style={{ width: '100%' }} />
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
                                <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Pull Remote Image</label>
                                <input
                                    className="premium-input"
                                    placeholder="docker.io/library/nginx:latest"
                                    value={pullImageName}
                                    onChange={e => setPullImageName(e.target.value)}
                                    onKeyDown={e => e.key === 'Enter' && handlePull()}
                                    style={{ width: '100%' }}
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
                            <div className="list-header flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
                                <h2 style={{ fontSize: '20px', margin: 0 }}>Your Local Images</h2>
                                <button className="btn btn-ghost" onClick={refreshData}>
                                    <RefreshCw size={16} className={isLoading ? "spin" : ""} /> Refresh
                                </button>
                            </div>

                            {images.length === 0 ? (
                                <div className="empty-state" style={{ textAlign: 'center', padding: '48px' }}>
                                    <Box size={48} color="var(--text-secondary)" opacity={0.5} style={{ margin: '0 auto 16px' }} />
                                    <p>No images found.</p>
                                    <p className="subtitle">Pull or build an image to get started.</p>
                                </div>
                            ) : (
                                <div className="table-responsive">
                                    <table className="container-table" style={{ width: '100%', borderCollapse: 'collapse' }}>
                                        <thead>
                                            <tr style={{ textAlign: 'left', borderBottom: '1px solid var(--border-color)' }}>
                                                <th style={{ padding: '12px' }}>Repository / Name</th>
                                                <th style={{ padding: '12px' }}>Tag</th>
                                                <th style={{ padding: '12px' }}>Size</th>
                                                <th style={{ padding: '12px', textAlign: "right" }}>Actions</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            {images.map((img, i) => (
                                                <tr key={i} className="container-row" style={{ borderBottom: '1px solid var(--border-color)' }}>
                                                    <td style={{ padding: '12px', fontWeight: 500, color: "var(--text-primary)" }}>{img.Repository}</td>
                                                    <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "12px" }}>
                                                        <span className="premium-badge">{img.Tag}</span>
                                                    </td>
                                                    <td style={{ padding: '12px', color: "var(--text-secondary)", fontSize: "13px" }}>{img.Size}</td>

                                                    <td style={{ padding: '12px', textAlign: "right" }}>
                                                        <div className="action-buttons" style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                                                            <button
                                                                className="btn-icon"
                                                                onClick={() => {
                                                                    const out = prompt("Enter path to save .tar file:", "/tmp/image.tar");
                                                                    if (out) executeAction("save-" + img.ID, () => saveImage(img.Repository, out));
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
                                                                    if (tg) executeAction("tag-" + img.ID, () => tagImage(img.Repository, tg));
                                                                }}
                                                                disabled={!!actionLoading}
                                                                title="Retag Image"
                                                            >
                                                                {actionLoading === "tag-" + img.ID ? <RefreshCw size={16} className="spin" /> : <Tag size={16} />}
                                                            </button>
                                                            <button
                                                                className="btn-icon text-danger"
                                                                onClick={() => {
                                                                    if (confirm(`Delete image ${img.Repository}?`)) {
                                                                        executeAction("delete-" + img.ID, () => deleteImage(img.Repository));
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
