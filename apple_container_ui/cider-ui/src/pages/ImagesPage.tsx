import { useState, useEffect } from "react";
import { Link, useSearchParams } from "react-router-dom";
import {
    CornerLeftUp,
    Trash2,
    RefreshCw,
    Download,
    Play,
    Activity,
    Layers,
    AlertCircle,
    X,
    Network,
    Box,
    Tag,
    FileCode,
    Search,
    Save
} from "lucide-react";
import {
    tagImage,
    getBuilderStatus,
    startBuilder,
    stopBuilder,
    deleteBuilder,
    checkSystemStatus,
    findDockerfiles,
    readFileContent,
    listImages,
    pullImage,
    deleteImage,
    buildImage,
    saveImage,
    loadImage,
    parseCompose,
    composeUp,
    composeDown,
    discoverComposeFiles
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
    const [builderCpus, setBuilderCpus] = useState<number | "">("");
    const [builderMemory, setBuilderMemory] = useState<string>("");

    // Quick load state
    const [tarPath, setTarPath] = useState("");

    // Tabs state
    const [searchParams, setSearchParams] = useSearchParams();
    const initialTab = (searchParams.get("tab") as "images" | "dockerfiles" | "compose") || "images";
    const [activeTab, setActiveTab] = useState<"images" | "dockerfiles" | "compose">(initialTab);

    const handleTabChange = (tab: "images" | "dockerfiles" | "compose") => {
        setActiveTab(tab);
        setSearchParams({ tab });
    };

    // Dockerfiles state
    const [dockerfiles, setDockerfiles] = useState<{ path: string; name: string }[]>([]);
    const [buildLoading, setBuildLoading] = useState<string | null>(null);
    const [previewFile, setPreviewFile] = useState<{ name: string; content: string } | null>(null);

    // Compose state
    const [composeFiles, setComposeFiles] = useState<{ path: string, name: string }[]>([]);
    const [selectedComposeFile, setSelectedComposeFile] = useState<string>("");
    const [projectName, setProjectName] = useState<string>("");
    const [parsedComposeData, setParsedComposeData] = useState<any>(null);
    const [rawComposeYaml, setRawComposeYaml] = useState<string>("");
    const [composeScanPath, setComposeScanPath] = useState<string>("");
    const [isDeploying, setIsDeploying] = useState<boolean>(false);
    const [composeError, setComposeError] = useState<string | null>(null);
    const [showSudoModal, setShowSudoModal] = useState<boolean>(false);
    const [showTearDownConfirm, setShowTearDownConfirm] = useState<boolean>(false);

    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const [imgs, buildStat, files, cFiles] = await Promise.all([
                    listImages(),
                    getBuilderStatus(),
                    findDockerfiles(),
                    discoverComposeFiles(composeScanPath || undefined)
                ]);
                setImages(imgs);
                setBuilderStatus(buildStat);
                setDockerfiles(files);
                setComposeFiles(cFiles);
            } else {
                setImages([]);
                setBuilderStatus(null);
                setDockerfiles([]);
                setComposeFiles([]);
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
            executeAction("start-builder", () => startBuilder(
                builderCpus === "" ? undefined : Number(builderCpus),
                builderMemory || undefined
            ));
        }
    }

    const handleBuilderDelete = () => {
        if (confirm("Are you sure you want to delete the builder container? This is required to change its CPU/Memory limits.")) {
            executeAction("delete-builder", () => deleteBuilder());
        }
    }

    const handlePull = () => {
        if (!pullImageName) return;
        executeAction("pull-img", async () => pullImage(pullImageName));
    }

    const handleBuild = () => {
        if (!buildDir || !buildTag) return;
        executeAction("build-img", async () => buildImage(`container build -t ${buildTag} ${buildDir}`));
    }

    const handleLoad = () => {
        if (!tarPath) return;
        executeAction("load-tar", async () => loadImage(tarPath));
    }

    const handlePreview = async (path: string, name: string) => {
        try {
            const content = await readFileContent(path);
            setPreviewFile({ name, content });
        } catch (e: any) {
            alert(`Failed to read file: ${e.message}`);
        }
    };

    const handleDockerfileBuild = async (path: string) => {
        const tag = prompt("Enter tag for this image (e.g., myapp:v1):");
        if (!tag) return;

        const dir = path.substring(0, path.lastIndexOf('/'));
        setBuildLoading(path);
        try {
            await buildImage(`container build -t ${tag} -f ${path} ${dir}`);
            alert(`Successfully started build for ${tag}.`);
            refreshData();
        } catch (e: any) {
            alert(`Build failed: ${e.message}`);
        } finally {
            setBuildLoading(null);
        }
    };

    const handleComposeParse = async (path: string) => {
        setIsLoading(true);
        setComposeError(null);
        try {
            const [data, content] = await Promise.all([
                parseCompose(path),
                readFileContent(path)
            ]);
            setParsedComposeData(data);
            setRawComposeYaml(content);
            setSelectedComposeFile(path);

            const parts = path.split('/');
            const dir = parts[parts.length - 2] || "project";
            setProjectName(dir.toLowerCase().replace(/[^a-z0-9]/g, '-'));
        } catch (e: any) {
            setComposeError(e.message);
        } finally {
            setIsLoading(false);
        }
    };

    const handleComposeDeploy = async () => {
        if (!selectedComposeFile || !projectName) return;
        setIsDeploying(true);
        setComposeError(null);
        try {
            await composeUp(selectedComposeFile, projectName);
            alert("Compose deployment started successfully!");
        } catch (e: any) {
            setComposeError(e.message);
        } finally {
            setIsDeploying(false);
        }
    };

    const confirmComposeTearDown = async () => {
        setIsLoading(true);
        setShowTearDownConfirm(false);
        try {
            await composeDown(projectName);
            alert("Project torn down successfully.");
        } catch (e: any) {
            alert("Failed to tear down: " + e.message);
        } finally {
            setIsLoading(false);
        }
    };

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
                <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                    {/* Unified Page Tabs */}
                    <div className="premium-card flex-between" style={{ padding: '8px', display: 'flex', gap: '8px' }}>
                        <button
                            className={`btn ${activeTab === 'images' ? 'btn-primary' : 'btn-ghost'}`}
                            style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
                            onClick={() => handleTabChange('images')}
                        >
                            <Box size={18} /> Local Images
                        </button>
                        <button
                            className={`btn ${activeTab === 'dockerfiles' ? 'btn-primary' : 'btn-ghost'}`}
                            style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
                            onClick={() => handleTabChange('dockerfiles')}
                        >
                            <FileCode size={18} /> Workspace Dockerfiles
                        </button>
                        <button
                            className={`btn ${activeTab === 'compose' ? 'btn-primary' : 'btn-ghost'}`}
                            style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px' }}
                            onClick={() => handleTabChange('compose')}
                        >
                            <Layers size={18} /> Docker Compose
                        </button>
                    </div>

                    {activeTab === 'compose' ? (
                        <div className="compose-layout animate-fade-in" style={{ display: 'grid', gridTemplateColumns: '350px 1fr', gap: '24px' }}>
                            {/* Compose Sidebar */}
                            <aside className="premium-card" style={{ padding: '20px' }}>
                                <h2 style={{ fontSize: '18px', marginBottom: '16px', display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <Search size={18} /> Discover Projects
                                </h2>
                                <div className="input-group mb-4">
                                    <label style={{ fontSize: '11px', color: 'var(--text-secondary)' }}>Scan Directory</label>
                                    <div style={{ display: 'flex', gap: '8px' }}>
                                        <input
                                            className="premium-input"
                                            placeholder="e.g. /Users/project"
                                            value={composeScanPath}
                                            onChange={e => setComposeScanPath(e.target.value)}
                                            style={{ flex: 1, padding: '6px 10px', fontSize: '12px' }}
                                        />
                                        <button className="btn btn-ghost btn-sm" onClick={refreshData}><RefreshCw size={12} /></button>
                                    </div>
                                </div>
                                <div className="compose-file-list" style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                                    {composeFiles.length === 0 ? (
                                        <p style={{ color: 'var(--text-secondary)', fontSize: '14px', textAlign: 'center', padding: '20px' }}>
                                            No compose files found.
                                        </p>
                                    ) : (
                                        composeFiles.map(file => (
                                            <div
                                                key={file.path}
                                                className={`compose-file-item premium-card ${selectedComposeFile === file.path ? 'active' : ''}`}
                                                onClick={() => handleComposeParse(file.path)}
                                                style={{
                                                    padding: '12px',
                                                    cursor: 'pointer',
                                                    background: selectedComposeFile === file.path ? 'rgba(0, 102, 255, 0.1)' : 'rgba(255,255,255,0.02)',
                                                    borderColor: selectedComposeFile === file.path ? 'var(--accent-primary)' : 'var(--border-color)',
                                                }}
                                            >
                                                <div style={{ fontWeight: 500, fontSize: '14px' }}>{file.name}</div>
                                                <div style={{ fontSize: '11px', color: 'var(--text-secondary)', wordBreak: 'break-all' }}>{file.path}</div>
                                            </div>
                                        ))
                                    )}
                                </div>
                            </aside>

                            {/* Compose Main Content */}
                            <main>
                                {composeError && (
                                    <div className="premium-card mb-4" style={{ borderColor: 'var(--danger-color)', background: 'rgba(239, 68, 68, 0.05)', color: 'var(--danger-color)', display: 'flex', alignItems: 'center', gap: '12px', padding: '16px' }}>
                                        <AlertCircle size={20} />
                                        <div style={{ fontSize: '14px' }}>{composeError}</div>
                                    </div>
                                )}

                                {!selectedComposeFile ? (
                                    <div className="premium-card flex-center" style={{ height: '300px', flexDirection: 'column', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                                        <Layers size={48} opacity={0.3} />
                                        <p style={{ marginTop: '16px' }}>Select a project to begin</p>
                                    </div>
                                ) : parsedComposeData ? (
                                    <div className="project-details">
                                        <section className="premium-card mb-4" style={{ padding: '24px' }}>
                                            <div className="flex-between mb-4">
                                                <div>
                                                    <h2 style={{ fontSize: '20px', margin: 0 }}>{projectName}</h2>
                                                    <p style={{ fontSize: '12px', color: 'var(--text-secondary)', marginTop: '4px' }}>{selectedComposeFile}</p>
                                                </div>
                                                <div className="flex-center" style={{ gap: '12px', display: 'flex', alignItems: 'center' }}>
                                                    <button className="btn btn-ghost" onClick={() => setShowSudoModal(true)}>DNS Setup</button>
                                                    <button className="btn btn-danger" onClick={() => setShowTearDownConfirm(true)}>Tear Down</button>
                                                    <button className="btn btn-primary" onClick={handleComposeDeploy} disabled={isDeploying}>
                                                        {isDeploying ? <RefreshCw className="spin" size={16} /> : <Play size={16} />} Deploy All
                                                    </button>
                                                </div>
                                            </div>
                                            <div className="services-grid" style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '16px' }}>
                                                {Object.entries(parsedComposeData.services || {}).map(([name, svc]: [string, any]) => (
                                                    <div key={name} className="service-card premium-card" style={{ padding: '16px' }}>
                                                        <h3 style={{ margin: 0, fontSize: '16px', color: 'var(--accent-primary)' }}>{name}</h3>
                                                        <p style={{ fontSize: '12px', color: 'var(--text-secondary)', marginTop: '8px' }}>
                                                            <strong>Image:</strong> {svc.image || `build:${svc.build?.context || '.'}`}
                                                        </p>
                                                    </div>
                                                ))}
                                            </div>
                                        </section>
                                        <section className="premium-card">
                                            <h3 style={{ fontSize: '14px', padding: '12px 20px', borderBottom: '1px solid var(--border-color)', margin: 0 }}>YAML Preview</h3>
                                            <pre style={{ margin: 0, padding: '20px', background: '#000', color: '#ccc', fontSize: '12px', overflowX: 'auto', maxHeight: '300px' }}>{rawComposeYaml}</pre>
                                        </section>
                                    </div>
                                ) : (
                                    <div className="flex-center" style={{ height: '300px', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                                        <RefreshCw className="spin" size={32} />
                                    </div>
                                )}
                            </main>
                        </div>
                    ) : (
                        <div style={{ display: 'grid', gridTemplateColumns: '320px 1fr', gap: '24px' }}>
                            {/* Builders Sidebar */}
                            <aside style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
                                <section className="premium-card">
                                    <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>BuildKit</h2>
                                    <div className="flex-between">
                                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                            <span className={`status-indicator status-${builderStatus?.status === 'running' ? 'running' : 'stopped'}`} />
                                            <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                                {builderStatus?.status === 'running' && <Activity size={14} className="text-accent" />}
                                                {builderStatus?.status === 'running' ? "Running" : "Stopped"}
                                            </span>
                                        </div>
                                        <div style={{ display: 'flex', gap: '4px' }}>
                                            <button className="btn btn-ghost btn-sm" onClick={handleBuilderToggle}>
                                                {builderStatus?.status === 'running' ? "Stop" : "Start"}
                                            </button>
                                            <button className="btn btn-ghost btn-sm text-danger" onClick={handleBuilderDelete} title="Delete builder">
                                                <Trash2 size={12} />
                                            </button>
                                        </div>
                                    </div>
                                    {builderStatus?.status !== 'running' && (
                                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginTop: '16px' }}>
                                            <div className="input-group">
                                                <label style={{ fontSize: '11px', color: 'var(--text-secondary)' }}>CPUs</label>
                                                <input
                                                    type="number"
                                                    className="premium-input"
                                                    placeholder="e.g. 8"
                                                    value={builderCpus}
                                                    onChange={e => setBuilderCpus(e.target.value === "" ? "" : parseInt(e.target.value))}
                                                    style={{ padding: '6px 10px', fontSize: '12px', minWidth: '0' }}
                                                />
                                            </div>
                                            <div className="input-group">
                                                <label style={{ fontSize: '11px', color: 'var(--text-secondary)' }}>Memory</label>
                                                <input
                                                    className="premium-input"
                                                    placeholder="e.g. 32g"
                                                    value={builderMemory}
                                                    onChange={e => setBuilderMemory(e.target.value)}
                                                    style={{ padding: '6px 10px', fontSize: '12px', minWidth: '0' }}
                                                />
                                            </div>
                                        </div>
                                    )}
                                </section>
                                <section className="premium-card">
                                    <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>Quick Build</h2>
                                    <div className="input-group mb-3">
                                        <label style={{ fontSize: '11px', color: 'var(--text-secondary)' }}>Context DIR</label>
                                        <input className="premium-input mb-2" placeholder="/path/to/project" value={buildDir} onChange={e => setBuildDir(e.target.value)} style={{ width: '100%' }} />
                                    </div>
                                    <div className="input-group mb-3">
                                        <label style={{ fontSize: '11px', color: 'var(--text-secondary)' }}>Tag</label>
                                        <input className="premium-input mb-3" placeholder="app:latest" value={buildTag} onChange={e => setBuildTag(e.target.value)} style={{ width: '100%' }} />
                                    </div>
                                    <button className="btn btn-primary" style={{ width: '100%' }} disabled={!buildDir || !buildTag || !!actionLoading} onClick={handleBuild}>
                                        {actionLoading === "build-img" ? <RefreshCw size={14} className="spin" /> : <Box size={14} />} Build Now
                                    </button>
                                </section>
                                <section className="premium-card">
                                    <h2 style={{ fontSize: '16px', marginBottom: '16px', borderBottom: '1px solid var(--border-color)', paddingBottom: '8px' }}>Load External Image</h2>
                                    <div className="input-group" style={{ marginBottom: '16px' }}>
                                        <label style={{ display: 'block', fontSize: '12px', marginBottom: '4px' }}>Tarball Path</label>
                                        <input className="premium-input" placeholder="/path/to/image.tar" value={tarPath} onChange={e => setTarPath(e.target.value)} style={{ width: '100%' }} />
                                    </div>
                                    <button className="btn btn-ghost" style={{ width: '100%', border: '1px solid var(--border-color)' }} disabled={!tarPath || !!actionLoading} onClick={handleLoad}>
                                        {actionLoading === "load-tar" ? <RefreshCw size={14} className="spin" /> : <Layers size={14} />} Load Tarball
                                    </button>
                                </section>
                            </aside>

                            {/* Main Content Areas */}
                            <main>
                                {activeTab === 'images' ? (
                                    <div className="premium-card" style={{ padding: '24px' }}>
                                        <div className="flex-between mb-4">
                                            <h2 style={{ fontSize: '20px', margin: 0 }}>Local Images</h2>
                                            <button className="btn btn-ghost" onClick={refreshData}>
                                                <RefreshCw size={14} className={isLoading ? "spin" : ""} /> Refresh
                                            </button>
                                        </div>

                                        <div className="input-group mb-4" style={{ display: 'flex', gap: '12px' }}>
                                            <input
                                                className="premium-input"
                                                placeholder="Pull image (e.g. alpine:latest)"
                                                value={pullImageName}
                                                onChange={e => setPullImageName(e.target.value)}
                                                style={{ flex: 1 }}
                                            />
                                            <button className="btn btn-primary" onClick={handlePull} disabled={!pullImageName || !!actionLoading}>
                                                {actionLoading === "pull-img" ? <RefreshCw size={14} className="spin" /> : <Download size={14} />} Pull
                                            </button>
                                        </div>

                                        <div className="table-responsive">
                                            <table className="container-table" style={{ width: '100%' }}>
                                                <thead>
                                                    <tr>
                                                        <th>Repository</th>
                                                        <th>Tag</th>
                                                        <th>Size</th>
                                                        <th style={{ textAlign: "right" }}>Actions</th>
                                                    </tr>
                                                </thead>
                                                <tbody>
                                                    {images.map((img, i) => (
                                                        <tr key={i}>
                                                            <td style={{ padding: '12px' }}>{img.Repository}</td>
                                                            <td><span className="premium-badge">{img.Tag}</span></td>
                                                            <td style={{ fontSize: '12px', color: 'var(--text-secondary)' }}>{img.Size}</td>
                                                            <td style={{ textAlign: "right" }}>
                                                                <button
                                                                    className="btn-icon"
                                                                    onClick={() => {
                                                                        const out = prompt("Enter path to save .tar file:", "/tmp/image.tar");
                                                                        if (out) executeAction("save-" + i, () => saveImage(img.Repository, out));
                                                                    }}
                                                                    title="Save image"
                                                                >
                                                                    <Save size={14} />
                                                                </button>
                                                                <button
                                                                    className="btn-icon"
                                                                    onClick={() => {
                                                                        const tg = prompt("New tag:", img.Repository + ":latest");
                                                                        if (tg) tagImage(img.Repository, tg).then(refreshData);
                                                                    }}
                                                                    title="Tag image"
                                                                >
                                                                    <Tag size={14} />
                                                                </button>
                                                                <button
                                                                    className="btn-icon text-danger"
                                                                    onClick={() => {
                                                                        if (confirm(`Delete ${img.Repository}?`)) deleteImage(img.Repository).then(refreshData);
                                                                    }}
                                                                    title="Delete image"
                                                                >
                                                                    <Trash2 size={14} />
                                                                </button>
                                                            </td>
                                                        </tr>
                                                    ))}
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                ) : (
                                    <div className="premium-card" style={{ padding: '24px' }}>
                                        <div className="flex-between mb-4">
                                            <h2 style={{ fontSize: '20px', margin: 0 }}>Workspace Dockerfiles</h2>
                                            <button className="btn btn-ghost" onClick={refreshData}>
                                                <RefreshCw size={14} className={isLoading ? "spin" : ""} /> Refresh
                                            </button>
                                        </div>
                                        <div className="table-responsive">
                                            <table className="container-table" style={{ width: '100%' }}>
                                                <thead>
                                                    <tr>
                                                        <th>Path</th>
                                                        <th style={{ textAlign: "right" }}>Build</th>
                                                    </tr>
                                                </thead>
                                                <tbody>
                                                    {dockerfiles.map((df, i) => (
                                                        <tr key={i}>
                                                            <td
                                                                style={{ padding: '12px', cursor: 'pointer', color: 'var(--accent-primary)' }}
                                                                onClick={() => handlePreview(df.path, df.name)}
                                                            >
                                                                {df.name}
                                                            </td>
                                                            <td style={{ textAlign: "right" }}>
                                                                <button
                                                                    className="btn btn-primary btn-sm"
                                                                    onClick={() => handleDockerfileBuild(df.path)}
                                                                    disabled={buildLoading === df.path}
                                                                >
                                                                    {buildLoading === df.path ? <RefreshCw className="spin" size={14} /> : "Build"}
                                                                </button>
                                                            </td>
                                                        </tr>
                                                    ))}
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                )}
                            </main>
                        </div>
                    )}
                </div>
            )}

            {/* Modals */}
            {showSudoModal && (
                <div className="modal-overlay flex-center" style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.8)', zIndex: 1000, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <div className="premium-card animate-scale-in" style={{ width: '500px', padding: '32px' }}>
                        <div className="flex-between mb-4" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <h2 style={{ margin: 0, display: 'flex', alignItems: 'center', gap: '12px', fontSize: '20px' }}>
                                <Network size={24} color="var(--accent-primary)" /> Service Discovery
                            </h2>
                            <button className="btn-icon" onClick={() => setShowSudoModal(false)}><X size={20} /></button>
                        </div>
                        <p style={{ color: 'var(--text-secondary)', fontSize: '14px', lineHeight: '1.6' }}>
                            Run this command to enable DNS discovery for project <strong>{projectName}</strong>:
                        </p>
                        <div style={{ background: '#000', padding: '12px', borderRadius: '6px', fontFamily: 'monospace', fontSize: '13px', margin: '16px 0' }}>
                            <code>sudo container system dns create {projectName}</code>
                        </div>
                        <button className="btn btn-primary" style={{ width: '100%' }} onClick={() => setShowSudoModal(false)}>Got it</button>
                    </div>
                </div>
            )}

            {showTearDownConfirm && (
                <div className="modal-overlay flex-center" style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.8)', zIndex: 1001, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                    <div className="premium-card" style={{ maxWidth: '400px', padding: '32px', textAlign: 'center' }}>
                        <AlertCircle size={48} color="var(--danger-color)" style={{ margin: '0 auto 16px' }} />
                        <h2 style={{ fontSize: '20px', marginBottom: '12px' }}>Tear Down Project?</h2>
                        <p style={{ color: 'var(--text-secondary)', marginBottom: '24px' }}>This will stop and remove all services in project <strong>{projectName}</strong>.</p>
                        <div style={{ display: 'flex', gap: '12px' }}>
                            <button className="btn btn-ghost" style={{ flex: 1 }} onClick={() => setShowTearDownConfirm(false)}>Cancel</button>
                            <button className="btn btn-danger" style={{ flex: 1 }} onClick={confirmComposeTearDown}>Tear Down</button>
                        </div>
                    </div>
                </div>
            )}

            {previewFile && (
                <div className="modal-overlay flex-center" style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.8)', zIndex: 1000, display: 'flex', alignItems: 'center', justifyContent: 'center' }} onClick={() => setPreviewFile(null)}>
                    <div className="premium-card" style={{ width: '80%', maxWidth: '800px', maxHeight: '80vh', padding: 0, overflow: 'hidden' }} onClick={e => e.stopPropagation()}>
                        <div className="flex-between" style={{ padding: '16px 20px', borderBottom: '1px solid var(--border-color)', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <h2 style={{ fontSize: '18px', margin: 0 }}>{previewFile.name}</h2>
                            <button className="btn-icon" onClick={() => setPreviewFile(null)}><X size={20} /></button>
                        </div>
                        <div style={{ padding: '20px', overflowY: 'auto', background: '#000', maxHeight: 'calc(80vh - 60px)' }}>
                            <pre style={{ margin: 0, color: '#ccc', fontSize: '12px' }}>{previewFile.content}</pre>
                        </div>
                    </div>
                </div>
            )}
        </div>
    );
}
