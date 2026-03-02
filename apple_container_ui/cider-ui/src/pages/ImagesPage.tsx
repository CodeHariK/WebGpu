import { useState, useEffect } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { Dialog } from '@base-ui/react/dialog';
import { Tabs } from '@base-ui/react/tabs';
import { Button } from '@base-ui/react/button';
import { Toast } from "@base-ui/react/toast";

import {
    CornerLeftUp,
    Trash2,
    RefreshCw,
    Download,
    Play,
    Layers,
    AlertCircle,
    X,
    Box,
    FileCode,
    Search,
    Save,
    FileText
} from "lucide-react";

import {
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
    discoverComposeFiles,
    createContainer
} from "../lib/container";
import type {
    ImageInfo,
    BuilderStatus
} from "../lib/container";
import CreateContainerModal from "../components/CreateContainerModal";
import "../Dashboard.css";


import * as React from 'react';


interface ModalProps {
    open: boolean;
    onOpenChange: (open: boolean) => void;
    title: string;
    children: React.ReactNode;
    maxWidth?: string;
}

export function Modal({ open, onOpenChange, title, children, maxWidth = '500px' }: ModalProps) {
    return (
        <Dialog.Root open={open} onOpenChange={onOpenChange}>
            <Dialog.Portal>
                <Dialog.Backdrop className="modal-backdrop" />
                <Dialog.Popup className="premium-card modal-popup" style={{ maxWidth }}>
                    <div className="flex-between mb-4">
                        <Dialog.Title style={{ fontSize: '18px', fontWeight: 600, margin: 0 }}>
                            {title}
                        </Dialog.Title>
                        <Dialog.Close className="btn-icon">
                            <X size={20} />
                        </Dialog.Close>
                    </div>
                    {children}
                </Dialog.Popup>
            </Dialog.Portal>
        </Dialog.Root>
    );
}

interface ConfirmDialogProps {
    open: boolean;
    onOpenChange: (open: boolean) => void;
    title: string;
    description: string;
    confirmLabel?: string;
    onConfirm: () => void | Promise<void>;
    variant?: 'danger' | 'primary';
    isLoading?: boolean;
}

export function ConfirmDialog({
    open,
    onOpenChange,
    title,
    description,
    confirmLabel = "Confirm",
    onConfirm,
    variant = 'primary',
    isLoading = false
}: ConfirmDialogProps) {
    return (
        <Dialog.Root open={open} onOpenChange={onOpenChange}>
            <Dialog.Portal>
                <Dialog.Backdrop className="modal-backdrop" />
                <Dialog.Popup className="premium-card modal-popup" style={{ maxWidth: '400px', textAlign: 'center' }}>
                    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
                        {variant === 'danger' && (
                            <AlertCircle size={40} color="var(--danger-color)" style={{ marginBottom: '16px' }} />
                        )}

                        <Dialog.Title style={{ fontSize: '20px', fontWeight: 600, margin: '0 0 8px 0' }}>
                            {title}
                        </Dialog.Title>

                        <Dialog.Description style={{ color: 'var(--text-secondary)', fontSize: '14px', lineHeight: '1.5', marginBottom: '24px' }}>
                            {description}
                        </Dialog.Description>

                        <div style={{ display: 'flex', gap: '12px', width: '100%' }}>
                            <Dialog.Close className="btn btn-ghost" style={{ flex: 1 }} disabled={isLoading}>
                                Cancel
                            </Dialog.Close>

                            <button
                                className={`btn ${variant === 'danger' ? 'btn-danger' : 'btn-primary'}`}
                                style={{ flex: 1 }}
                                onClick={async () => {
                                    await onConfirm();
                                    onOpenChange(false);
                                }}
                                disabled={isLoading}
                            >
                                {isLoading ? 'Processing...' : confirmLabel}
                            </button>
                        </div>
                    </div>
                </Dialog.Popup>
            </Dialog.Portal>
        </Dialog.Root>
    );
}


const toastManager = Toast.createToastManager();

export default function ImagesPage() {
    return (
        <Toast.Provider toastManager={toastManager}>
            <ImagesPageContent />
        </Toast.Provider>
    );
}

function ImagesPageContent() {
    const { toasts } = Toast.useToastManager();

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
    const [tarPath, setTarPath] = useState("");
    const [selectedImageForContainer, setSelectedImageForContainer] = useState<string | null>(null);
    const [isCreatingContainer, setIsCreatingContainer] = useState(false);
    const [imageToDelete, setImageToDelete] = useState<string | null>(null);
    const [imageToSave, setImageToSave] = useState<{ repository: string; path: string } | null>(null);
    const [dockerfileToBuild, setDockerfileToBuild] = useState<{ path: string; tag: string } | null>(null);

    // Tabs state
    const [searchParams, setSearchParams] = useSearchParams();
    const activeTab = (searchParams.get("tab") as "images" | "dockerfiles" | "compose") || "images";

    const handleTabChange = (tab: "images" | "dockerfiles" | "compose") => {
        setSearchParams({ tab });
    };

    // Dockerfiles state



    const refreshData = async () => {
        setIsLoading(true);
        try {
            const isRunning = await checkSystemStatus();
            setSystemRunning(isRunning);
            if (isRunning) {
                const [imgs, buildStat, files] = await Promise.all([
                    listImages(),
                    getBuilderStatus(),
                ]);
                setImages(imgs);
                setBuilderStatus(buildStat);
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
            toastManager.add({
                title: "Error",
                description: e.message || "An error occurred",
                type: "error"
            });
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

    const handleCreateContainer = async (config: { image: string, command: string, runImmediately: boolean }) => {
        setIsCreatingContainer(true);
        try {
            await createContainer(config.command);
            toastManager.add({
                title: config.runImmediately ? "Container Started" : "Container Created",
                description: `Successfully ${config.runImmediately ? 'started' : 'created'} container from ${config.image}`,
                type: "success"
            });
            setSelectedImageForContainer(null);
        } catch (e: any) {
            toastManager.add({
                title: "Creation Failed",
                description: e.message,
                type: "error"
            });
        } finally {
            setIsCreatingContainer(false);
        }
    };



    return (
        <div className="dashboard-container animate-fade-in" style={{ maxWidth: '1200px', margin: '0 auto', padding: '24px' }}>
            <header className="dashboard-header premium-card flex-between" style={{ marginBottom: "24px", display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
                    <Link to="/" className="btn-icon" style={{ color: 'inherit' }}><CornerLeftUp size={20} /></Link>
                    <h1 style={{ fontSize: '24px', margin: 0 }}>Images & Builders</h1>
                </div>
                <div className="status-badge flex-center" style={{ gap: '8px', display: 'flex', alignItems: 'center' }}>
                    <span className={`status-indicator status-${systemRunning ? 'running' : 'stopped'}`} />
                    <span>Daemon {systemRunning ? "Running" : "Offline"}</span>
                </div>
            </header>

            <aside style={{ display: 'flex', flexDirection: 'row', gap: '24px' }}>
                <BuildKitInfo builderStatus={builderStatus} handleBuilderToggle={handleBuilderToggle} handleBuilderDelete={() => executeAction("del", deleteBuilder)} builderCpus={builderCpus} setBuilderCpus={setBuilderCpus} builderMemory={builderMemory} setBuilderMemory={setBuilderMemory} />
                <QuickBuild buildDir={buildDir} setBuildDir={setBuildDir} buildTag={buildTag} setBuildTag={setBuildTag} actionLoading={actionLoading} handleBuild={() => executeAction("build-img", () => buildImage(`container build -t ${buildTag} ${buildDir}`))} />
                <LoadExternalImage tarPath={tarPath} setTarPath={setTarPath} actionLoading={actionLoading} handleLoad={() => executeAction("load-tar", () => loadImage(tarPath))} />
            </aside>

            {!systemRunning ? (
                <div className="empty-state premium-card" style={{ textAlign: 'center', padding: '48px' }}>
                    <Box size={48} opacity={0.5} style={{ margin: '0 auto 16px' }} />
                    <p>The container daemon is offline.</p>
                </div>
            ) : (

                <Tabs.Root
                    className="tabs-root"
                    value={activeTab}
                    onValueChange={(val) => handleTabChange(val as any)}
                >
                    <Tabs.List className="premium-card tabs-list">
                        <Tabs.Tab className="tabs-tab" value="images">
                            <Box size={18} /> Images
                        </Tabs.Tab>
                        <Tabs.Tab className="tabs-tab" value="dockerfiles">
                            <FileCode size={18} /> Dockerfiles
                        </Tabs.Tab>
                        <Tabs.Tab className="tabs-tab" value="compose">
                            <Layers size={18} /> Compose
                        </Tabs.Tab>
                        {/* Optional: Animated line that moves under the active tab */}
                        <Tabs.Indicator className="tabs-indicator" />
                    </Tabs.List>

                    <Tabs.Panel value="images" className="tabs-panel">
                        <LocalImagesList
                            refreshData={refreshData}
                            isLoading={isLoading}
                            pullImageName={pullImageName}
                            setPullImageName={setPullImageName}
                            handlePull={() => executeAction("pull-img", () => pullImage(pullImageName))}
                            actionLoading={actionLoading}
                            images={images}
                            onPlay={(img: string) => setSelectedImageForContainer(img)}
                            setImageToDelete={setImageToDelete}
                            setImageToSave={(repo: string) => setImageToSave({ repository: repo, path: "/tmp/img.tar" })}
                        />
                    </Tabs.Panel>

                    <Tabs.Panel value="dockerfiles" className="tabs-panel">
                        <DockerFilesUI
                            setDockerfileToBuild={(path: string) => setDockerfileToBuild({ path, tag: "myapp:v1" })}
                            buildLoading={actionLoading === "build-df" ? dockerfileToBuild?.path || null : null}
                        />
                    </Tabs.Panel>

                    <Tabs.Panel value="compose" className="tabs-panel">
                        <DockerComposeFilesUI />
                    </Tabs.Panel>
                </Tabs.Root>


            )}


            {selectedImageForContainer && (
                <CreateContainerModal
                    imageName={selectedImageForContainer}
                    onClose={() => setSelectedImageForContainer(null)}
                    onCreate={handleCreateContainer}
                    isCreating={isCreatingContainer}
                />
            )}

            <ConfirmDialog
                open={!!imageToDelete}
                onOpenChange={(open) => !open && setImageToDelete(null)}
                variant="danger"
                title="Delete Image?"
                description={`Are you sure you want to delete ${imageToDelete}? This action cannot be undone.`}
                confirmLabel="Delete"
                onConfirm={async () => {
                    if (imageToDelete) {
                        await executeAction("del-img", () => deleteImage(imageToDelete));
                        setImageToDelete(null);
                    }
                }}
            />

            <Modal
                open={!!imageToSave}
                onOpenChange={(open) => !open && setImageToSave(null)}
                title="Save Image to Tarball"
            >
                <div className="input-group mb-4">
                    <label>Save Path</label>
                    <input
                        placeholder="/tmp/myapp.tar"
                        value={imageToSave?.path || ""}
                        onChange={(e) => setImageToSave(prev => prev ? { ...prev, path: e.target.value } : null)}
                    />
                </div>
                <div style={{ display: 'flex', gap: '12px' }}>
                    <button className="btn btn-ghost" style={{ flex: 1 }} onClick={() => setImageToSave(null)}>Cancel</button>
                    <button className="btn btn-primary" style={{ flex: 1 }} onClick={async () => {
                        if (imageToSave) {
                            await executeAction("save-img", () => saveImage(imageToSave.repository, imageToSave.path));
                            setImageToSave(null);
                        }
                    }}>Save</button>
                </div>
            </Modal>

            <Modal
                open={!!dockerfileToBuild}
                onOpenChange={(open) => !open && setDockerfileToBuild(null)}
                title="Build Dockerfile"
            >
                <div className="input-group mb-4">
                    <label>Target Image Tag</label>
                    <input
                        placeholder="myapp:latest"
                        value={dockerfileToBuild?.tag || ""}
                        onChange={(e) => setDockerfileToBuild(prev => prev ? { ...prev, tag: e.target.value } : null)}
                    />
                </div>
                <div style={{ display: 'flex', gap: '12px' }}>
                    <button className="btn btn-ghost" style={{ flex: 1 }} onClick={() => setDockerfileToBuild(null)}>Cancel</button>
                    <button className="btn btn-primary" style={{ flex: 1 }} onClick={async () => {
                        if (dockerfileToBuild) {
                            const dir = dockerfileToBuild.path.substring(0, dockerfileToBuild.path.lastIndexOf('/'));
                            await executeAction("build-df", () => buildImage(`container build -t ${dockerfileToBuild.tag} -f ${dockerfileToBuild.path} ${dir}`));
                            setDockerfileToBuild(null);
                        }
                    }}>Build Now</button>
                </div>
            </Modal>

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

// Sub-components moved outside for cleanliness
interface LocalImagesListProps {
    refreshData: () => Promise<void>;
    isLoading: boolean;
    pullImageName: string;
    setPullImageName: (val: string) => void;
    handlePull: () => void;
    actionLoading: string | null;
    images: ImageInfo[];
    onPlay: (img: string) => void;
    setImageToDelete: (img: string) => void;
    setImageToSave: (img: string) => void;
}

function LocalImagesList({ refreshData, isLoading, pullImageName, setPullImageName, handlePull, actionLoading, images, onPlay, setImageToDelete, setImageToSave }: LocalImagesListProps) {
    return (
        <div className="premium-card" style={{ padding: '24px' }}>
            <div className="flex-between mb-4">
                <h2 style={{ fontSize: '20px', margin: 0 }}>Local Images</h2>
                <button className="btn btn-ghost" onClick={refreshData}><RefreshCw size={14} className={isLoading ? "spin" : ""} /></button>
            </div>
            <div className="input-group mb-4" style={{ display: 'flex', gap: '12px' }}>
                <input placeholder="alpine:latest" value={pullImageName} onChange={e => setPullImageName(e.target.value)} style={{ flex: 1 }} />
                <Button className={"btn btn-primary"} onClick={handlePull} disabled={!pullImageName || !!actionLoading}>
                    {actionLoading === "pull-img" ? <RefreshCw size={14} className="spin" /> : <Download size={14} />} Pull
                </Button>
            </div>
            <table className="container-table" style={{ width: '100%' }}>
                <thead><tr><th>Repository</th><th>Tag</th><th>Size</th><th style={{ textAlign: "right" }}>Actions</th></tr></thead>
                <tbody>
                    {images.map((img, i) => (
                        <tr key={i}>
                            <td>{img.Repository}</td>
                            <td><span className="premium-badge">{img.Tag}</span></td>
                            <td style={{ fontSize: '12px', color: 'var(--text-secondary)' }}>{img.Size}</td>
                            <td style={{ display: "flex", gap: "4px", justifyContent: "flex-end" }}>
                                <button
                                    className="btn-icon text-success"
                                    onClick={() => onPlay(img.Name)}
                                    title="Run/Create Container"
                                >
                                    <Play size={14} />
                                </button>
                                <button className="btn-icon" onClick={() => setImageToSave(img.Name)}><Save size={14} /></button>
                                <button className="btn-icon text-danger" onClick={() => setImageToDelete(img.Name)}><Trash2 size={14} /></button>
                            </td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}


function DockerFilesUI({ setDockerfileToBuild, buildLoading }: { setDockerfileToBuild: (path: string) => void; buildLoading: string | null }) {

    const [dockerfiles, setDockerfiles] = useState<{ path: string; name: string }[]>([]);
    const [previewFile, setPreviewFile] = useState<{ name: string; content: string } | null>(null);

    return <div className="compose-layout" style={{ display: 'grid', gridTemplateColumns: '300px 1fr', gap: '24px', minHeight: '600px' }}>
        <aside className="premium-card" style={{ padding: '20px' }}>
            <h2 className="flex items-center gap-2 mb-4"><Search size={16} /> Search '.Dockerfile'</h2>
            <input className="w-full" placeholder="Enter directory path to search" onChange={async (v) => {
                setDockerfiles(await findDockerfiles(v.target.value))
            }} />
        </aside>

        <main className="premium-card">
            {dockerfiles ? (
                <table className="container-table" style={{ width: '100%' }}>
                    <tbody>
                        {dockerfiles.map((df, i) => (
                            <tr key={i}>
                                <td style={{ padding: '12px', cursor: 'pointer', color: 'var(--accent-primary)' }} onClick={async () => {
                                    const content = await readFileContent(df.path);
                                    setPreviewFile({ name: df.name, content });
                                }}>{df.name} {df.path}</td>
                                <td style={{ textAlign: "right" }}>
                                    <button className="btn btn-primary btn-sm" onClick={() => setDockerfileToBuild(df.path)} disabled={!!buildLoading}>
                                        {buildLoading === df.path ? <RefreshCw className="spin" size={14} /> : "Build"}
                                    </button>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            ) : <div className="premium-card flex-center" style={{ height: '100%', opacity: 0.5 }}><Layers size={48} /></div>}
        </main>

        <Modal
            open={!!previewFile}
            onOpenChange={(open) => !open && setPreviewFile(null)}
            title={previewFile?.name || "File Preview"}
            maxWidth="800px"
        >
            <div style={{
                margin: '-24px', // Negate padding from Modal helper if necessary to reach edges
                marginTop: '16px',
                background: '#000',
                maxHeight: 'calc(80vh - 100px)',
                overflowY: 'auto',
                borderBottomLeftRadius: '12px',
                borderBottomRightRadius: '12px'
            }}>
                <pre style={{
                    margin: 0,
                    padding: '20px',
                    color: '#ccc',
                    fontSize: '12px',
                    lineHeight: '1.5',
                    fontFamily: 'var(--font-mono, monospace)'
                }}>
                    {previewFile?.content}
                </pre>
            </div>
        </Modal>

    </div>
}


function DockerComposeFilesUI() {
    const [projectName, setProjectName] = useState("");
    const [parsedComposeData, setParsedComposeData] = useState<any>(null);
    const [rawComposeYaml, setRawComposeYaml] = useState("");
    const [isLoading, setIsLoading] = useState(false);
    const [isDeploying, setIsDeploying] = useState(false);
    const [composeError, setComposeError] = useState<string | null>(null);
    const [showSudoModal, setShowSudoModal] = useState(false);
    const [showTearDownConfirm, setShowTearDownConfirm] = useState(false);
    const [composeFiles, setComposeFiles] = useState<{ path: string, name: string }[]>([]);
    const [selectedComposeFile, setSelectedComposeFile] = useState("");

    const handleDiscover = async (composeScanPath: string) => {
        setIsLoading(true);
        try {
            const files = await discoverComposeFiles(composeScanPath || undefined);
            setComposeFiles(files);
        } catch (e: any) {
            setComposeError(e.message);
        } finally {
            setIsLoading(false);
        }
    };

    const handleComposeParse = async (path: string) => {
        setIsLoading(true);
        try {
            const [data, content] = await Promise.all([parseCompose(path), readFileContent(path)]);
            setParsedComposeData(data);
            setRawComposeYaml(content);
            setSelectedComposeFile(path);
            const parts = path.split('/');
            setProjectName((parts[parts.length - 2] || "project").toLowerCase().replace(/[^a-z0-9]/g, '-'));
        } catch (e: any) {
            setComposeError(e.message);
        } finally {
            setIsLoading(false);
        }
    };

    return (
        <div className="compose-layout" style={{ display: 'grid', gridTemplateColumns: '300px 1fr', gap: '24px', minHeight: '600px' }}>
            <aside className="premium-card" style={{ padding: '20px' }}>
                <h2 style={{ fontSize: '16px', marginBottom: '16px' }}><Search size={16} /> Discover Projects</h2>
                <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
                    <input placeholder="/Users/project" onChange={e => {
                        handleDiscover(e.target.value)
                    }} />
                    <button className="btn btn-ghost"><RefreshCw size={14} className={isLoading ? "spin" : ""} /></button>
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {composeFiles.map(file => (
                        <div key={file.path} onClick={() => handleComposeParse(file.path)} className={`premium-card ${selectedComposeFile === file.path ? 'active' : ''}`} style={{ padding: '10px', cursor: 'pointer', fontSize: '13px', borderColor: selectedComposeFile === file.path ? 'var(--accent-primary)' : '' }}>
                            <div style={{ fontWeight: 600 }}><FileText size={12} /> {file.name}</div>
                            <div style={{ fontSize: '10px', opacity: 0.6, overflow: 'hidden' }}>{file.path}</div>
                        </div>
                    ))}
                </div>
            </aside>

            <main>
                {selectedComposeFile ? (
                    <div className="project-details">
                        <section className="premium-card mb-4" style={{ padding: '24px' }}>
                            <div className="flex-between mb-4">
                                <div>
                                    <h2 style={{ margin: 0 }}>{projectName}</h2>
                                    <p style={{ fontSize: '12px', opacity: 0.6 }}>{selectedComposeFile}</p>
                                </div>
                                <div style={{ display: 'flex', gap: '8px' }}>
                                    <button className="btn btn-ghost" onClick={() => setShowSudoModal(true)}>DNS Setup</button>
                                    <button className="btn btn-danger" onClick={() => setShowTearDownConfirm(true)}>Tear Down</button>
                                    <button className="btn btn-primary" onClick={() => composeUp(selectedComposeFile, projectName)}>Deploy All</button>
                                </div>
                            </div>
                            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '12px' }}>
                                {Object.entries(parsedComposeData?.services || {}).map(([name, svc]: [string, any]) => (
                                    <div key={name} className="premium-card" style={{ padding: '12px' }}>
                                        <h4 style={{ margin: 0, color: 'var(--accent-primary)' }}>{name}</h4>
                                        <p style={{ fontSize: '11px', margin: '4px 0 0' }}>{svc.image || 'Custom Build'}</p>
                                    </div>
                                ))}
                            </div>
                        </section>
                        <pre className="premium-card" style={{ padding: '16px', background: '#000', color: '#8e8', fontSize: '11px', maxHeight: '300px', overflow: 'auto' }}>{rawComposeYaml}</pre>
                    </div>
                ) : <div className="premium-card flex-center" style={{ height: '100%', opacity: 0.5 }}><Layers size={48} /></div>}
            </main>

            <Modal
                open={showSudoModal}
                onOpenChange={setShowSudoModal}
                title="Service Discovery"
            >
                <p style={{ color: 'var(--text-secondary)', fontSize: '14px' }}>
                    Run this command to enable DNS for <b>{projectName}</b>:
                </p>
                <div className="code-block" style={{ margin: '16px 0' }}>
                    <code>sudo container system dns create {projectName}</code>
                </div>
                <button className="btn btn-primary" style={{ width: '100%' }} onClick={() => setShowSudoModal(false)}>
                    Got it
                </button>
            </Modal>

            {/* Inside DockerComposeFilesUI */}
            <ConfirmDialog
                open={showTearDownConfirm}
                onOpenChange={setShowTearDownConfirm}
                variant="danger"
                title="Tear Down Project?"
                description={`This will stop and remove all services for ${projectName}.`}
                confirmLabel="Tear Down"
                onConfirm={async () => {
                    await composeDown(projectName);
                }}
            />

        </div>
    );
}

// Logic components for the sidebar
function BuildKitInfo({ builderStatus, handleBuilderToggle, handleBuilderDelete, builderCpus, setBuilderCpus, builderMemory, setBuilderMemory }) {
    return (
        <section className="premium-card flex-col gap-2">
            <div className="flex-between mb-3">
                <h3>BuildKit</h3>
                <div className="flex-row gap-2">
                    <Button
                        onClick={handleBuilderToggle}
                        className={"Button btn-ghost btn-sm"}
                    >
                        {builderStatus?.status === 'running' ? "Stop" : "Start"}
                    </Button>
                    <Button
                        onClick={handleBuilderDelete}
                        className={"Button btn-ghost btn-sm text-danger"}
                    >
                        <Trash2 size={12} />
                    </Button>
                </div>
            </div>
            {builderStatus?.status !== 'running' && (
                <div className="flex-row gap-2">
                    <input placeholder="CPUs" value={builderCpus} onChange={e => setBuilderCpus(e.target.value)} />
                    <input placeholder="Mem" value={builderMemory} onChange={e => setBuilderMemory(e.target.value)} />
                </div>
            )}
        </section>
    );
}

function QuickBuild({ buildDir, setBuildDir, buildTag, setBuildTag, actionLoading, handleBuild }) {
    return (
        <section className="premium-card flex-col gap-2">
            <h3>Quick Build</h3>
            <input placeholder="Context Dir" value={buildDir} onChange={e => setBuildDir(e.target.value)} />
            <input placeholder="Tag (app:v1)" value={buildTag} onChange={e => setBuildTag(e.target.value)} />
            <Button
                disabled={!!actionLoading}
                onClick={handleBuild}
                className={"Button w-full"}
            >
                Build Now
            </Button>
        </section>
    );
}

function LoadExternalImage({ tarPath, setTarPath, actionLoading, handleLoad }) {
    return (
        <section className="premium-card flex-col gap-2">
            <h3>Import Tar</h3>
            <input className="mb-3" placeholder="/path/to/img.tar" value={tarPath} onChange={e => setTarPath(e.target.value)} />
            <Button
                disabled={!!actionLoading}
                onClick={handleLoad}
                className={"Button w-full"}
            >
                Load Tarball
            </Button>
        </section>
    );
}