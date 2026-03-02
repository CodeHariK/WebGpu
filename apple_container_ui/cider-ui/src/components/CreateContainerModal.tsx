import { useState, useEffect } from "react";
import { X, Plus, Trash2 } from "lucide-react";
import { listNetworks, type NetworkInfo, type ContainerConfig } from "../lib/container";
import BaseSelect from "./BaseSelect";
import "../Dashboard.css"; // Reuse dashboard styles
import { buildRunCommand, buildCreateCommand } from "../lib/commandBuilder";

interface Props {
    imageName?: string;
    onClose: () => void;
    onCreate: (config: { image: string, command: string, runImmediately: boolean }) => void;
    isCreating: boolean;
}

interface PortConfig { host: string; container: string; }
interface EnvConfig { key: string; value: string; }
interface VolConfig { host: string; container: string; }

export default function CreateContainerModal({ imageName, onClose, onCreate, isCreating }: Props) {
    const [customImage, setCustomImage] = useState("");
    const [command, setCommand] = useState("");
    const [cpus, setCpus] = useState<number | undefined>(undefined);
    const [memoryMB, setMemoryMB] = useState<number | undefined>(undefined);
    const [network, setNetwork] = useState("");
    const [ports, setPorts] = useState<PortConfig[]>([]);
    const [env, setEnv] = useState<EnvConfig[]>([]);
    const [volumes, setVolumes] = useState<VolConfig[]>([]);
    const [runImmediately, setRunImmediately] = useState(true);
    const [networks, setNetworks] = useState<NetworkInfo[]>([]);

    const effectiveImage = imageName || customImage;

    useEffect(() => {
        listNetworks().then(setNetworks);
    }, []);

    const config: ContainerConfig = {
        image: effectiveImage,
        command: command || undefined,
        cpus: cpus || undefined,
        memory: memoryMB ? `${memoryMB}M` : undefined,
        network: network || undefined,
        ports: ports.filter(p => p.host && p.container).map(p => `${p.host}:${p.container}`),
        env: env.filter(e => e.key && e.value).map(e => `${e.key}=${e.value}`),
        volumes: volumes.filter(v => v.host && v.container).map(v => `${v.host}:${v.container}`),
    };

    const currentCommand = runImmediately
        ? buildRunCommand(config)
        : buildCreateCommand(config);

    const handleCreate = () => {
        onCreate({ image: effectiveImage, command: currentCommand, runImmediately });
    };

    const addPort = () => setPorts([...ports, { host: "", container: "" }]);
    const updatePort = (idx: number, field: keyof PortConfig, val: string) => {
        const next = [...ports];
        next[idx][field] = val;
        setPorts(next);
    }
    const removePort = (idx: number) => setPorts(ports.filter((_, i) => i !== idx));

    const addEnv = () => setEnv([...env, { key: "", value: "" }]);
    const updateEnv = (idx: number, field: keyof EnvConfig, val: string) => {
        const next = [...env];
        next[idx][field] = val;
        setEnv(next);
    }
    const removeEnv = (idx: number) => setEnv(env.filter((_, i) => i !== idx));

    const addVol = () => setVolumes([...volumes, { host: "", container: "" }]);
    const updateVol = (idx: number, field: keyof VolConfig, val: string) => {
        const next = [...volumes];
        next[idx][field] = val;
        setVolumes(next);
    }
    const removeVol = (idx: number) => setVolumes(volumes.filter((_, i) => i !== idx));

    return (
        <div className="modal-backdrop flex-center animate-fade-in">
            <div className="modal-content premium-card">
                <div className="modal-header flex-between mb-4">
                    <h2 style={{ fontSize: "20px" }}>{imageName ? `Create Container from ${imageName}` : "Create New Container"}</h2>
                    <button className="btn-icon" onClick={onClose} disabled={isCreating}><X size={20} /></button>
                </div>

                <div className="modal-body" style={{ maxHeight: "60vh", overflowY: "auto", paddingRight: "8px" }}>
                    {/* Image Selection (if not provided) */}
                    {!imageName && (
                        <section className="mb-4">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>Image</h3>
                            <div className="input-group">
                                <label>Image Reference</label>
                                <input
                                    placeholder="e.g., redis:latest"
                                    value={customImage}
                                    onChange={e => setCustomImage(e.target.value)}
                                />
                            </div>
                        </section>
                    )}

                    {/* General */}
                    <section className="mb-4">
                        <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>General</h3>
                        <div className="input-group mb-4">
                            <label className="flex-center" style={{ justifyContent: "flex-start", gap: "8px", cursor: "pointer", userSelect: "none" }}>
                                <input
                                    type="checkbox"
                                    checked={runImmediately}
                                    onChange={e => setRunImmediately(e.target.checked)}
                                    style={{ width: "16px", height: "16px" }}
                                />
                                <span>Run immediately after creation (-d)</span>
                            </label>
                        </div>

                        <div className="input-group mb-2">
                            <label>Command Override (Optional)</label>
                            <input

                                placeholder="e.g., tail -f /dev/null"
                                value={command}
                                onChange={e => setCommand(e.target.value)}
                            />
                        </div>
                        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "16px" }}>
                            <div className="input-group">
                                <label>CPU Cores</label>
                                <input
                                    type="number"
                                    step="0.1"
                                    min="0"

                                    placeholder="e.g., 2"
                                    value={cpus || ""}
                                    onChange={e => setCpus(parseFloat(e.target.value) || undefined)}
                                />
                            </div>
                            <div className="input-group">
                                <label>Memory Limit (MB)</label>
                                <input
                                    type="number"
                                    min="4"
                                    placeholder="e.g., 512"
                                    value={memoryMB || ""}
                                    onChange={e => setMemoryMB(parseInt(e.target.value) || undefined)}
                                />
                            </div>
                            <div className="input-group">
                                <BaseSelect
                                    label="Network"
                                    value={network}
                                    onChange={setNetwork}
                                    options={[
                                        { label: "(Default)", value: "" },
                                        ...networks.map(n => ({ label: n.id, value: n.id }))
                                    ]}
                                    placeholder="Select Network"
                                />
                            </div>
                        </div>
                    </section>

                    {/* Ports */}
                    <section className="mb-4">
                        <div className="flex-between mb-2">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)" }}>Port Forwards (-p)</h3>
                            <button className="btn btn-secondary" style={{ padding: "4px 8px", fontSize: "12px" }} onClick={addPort}>
                                <Plus size={14} style={{ marginRight: "4px" }} /> Add Port
                            </button>
                        </div>
                        {ports.map((port, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input placeholder="Host Port" value={port.host} onChange={e => updatePort(idx, "host", e.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>:</span>
                                <input placeholder="Container Port" value={port.container} onChange={e => updatePort(idx, "container", e.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removePort(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {ports.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No ports configured.</p>}
                    </section>

                    {/* Env Vars */}
                    <section className="mb-4">
                        <div className="flex-between mb-2">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)" }}>Environment Variables (-e)</h3>
                            <button className="btn btn-secondary" style={{ padding: "4px 8px", fontSize: "12px" }} onClick={addEnv}>
                                <Plus size={14} style={{ marginRight: "4px" }} /> Add Env
                            </button>
                        </div>
                        {env.map((e, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input placeholder="KEY" value={e.key} onChange={ev => updateEnv(idx, "key", ev.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>=</span>
                                <input placeholder="Value" value={e.value} onChange={ev => updateEnv(idx, "value", ev.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removeEnv(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {env.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No environment variables configured.</p>}
                    </section>

                    {/* Volumes */}
                    <section className="mb-4">
                        <div className="flex-between mb-2">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)" }}>Bind Mounts (-v)</h3>
                            <button className="btn btn-secondary" style={{ padding: "4px 8px", fontSize: "12px" }} onClick={addVol}>
                                <Plus size={14} style={{ marginRight: "4px" }} /> Add Mount
                            </button>
                        </div>
                        {volumes.map((vol, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input placeholder="Host Path (/Users/)" value={vol.host} onChange={e => updateVol(idx, "host", e.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>:</span>
                                <input placeholder="Container Path (/app)" value={vol.container} onChange={e => updateVol(idx, "container", e.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removeVol(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {volumes.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No bind mounts configured.</p>}
                    </section>

                </div>

                <div className="modal-footer" style={{ marginTop: "16px" }}>
                    <div className="command-preview mb-4" style={{ background: "rgba(0,0,0,0.3)", padding: "12px", borderRadius: "8px", border: "1px solid rgba(255,255,255,0.1)" }}>
                        <div style={{ fontSize: "11px", color: "var(--text-secondary)", marginBottom: "4px", textTransform: "uppercase", letterSpacing: "1px" }}>Command Preview</div>
                        <code style={{ fontSize: "12px", color: "var(--accent-primary)", wordBreak: "break-all", fontFamily: "monospace" }}>{currentCommand}</code>
                    </div>
                    <div style={{ display: "flex", justifyContent: "flex-end", gap: "12px" }}>
                        <button className="btn btn-secondary" onClick={onClose} disabled={isCreating}>Cancel</button>
                        <button className="btn btn-primary" onClick={handleCreate} disabled={isCreating}>
                            {isCreating ? "Processing..." : (runImmediately ? "Run Container" : "Create Container")}
                        </button>
                    </div>
                </div>
            </div>
        </div>
    );
}
