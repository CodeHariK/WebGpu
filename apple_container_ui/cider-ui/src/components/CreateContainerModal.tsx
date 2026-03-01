import { useState } from "react";
import { X, Plus, Trash2 } from "lucide-react";
import type { ContainerConfig } from "../lib/container";
import "../Dashboard.css"; // Reuse dashboard styles

interface Props {
    imageName: string;
    onClose: () => void;
    onCreate: (config: ContainerConfig) => void;
    isCreating: boolean;
}

export default function CreateContainerModal({ imageName, onClose, onCreate, isCreating }: Props) {
    const [config, setConfig] = useState<ContainerConfig>({
        image: imageName,
        command: "",
        ports: [],
        env: [],
        volumes: [],
        cpus: undefined,
        memory: "" // e.g., "4G"
    });

    const handleCreate = () => {
        // Filter out empty rows
        const cleanedConfig = {
            ...config,
            ports: config.ports?.filter(p => p.host && p.container),
            env: config.env?.filter(e => e.key && e.value),
            volumes: config.volumes?.filter(v => v.host && v.container),
        };
        onCreate(cleanedConfig);
    };

    const addPort = () => setConfig({ ...config, ports: [...(config.ports || []), { host: "", container: "" }] });
    const updatePort = (idx: number, field: "host" | "container", val: string) => {
        const newPorts = [...(config.ports || [])];
        newPorts[idx][field] = val;
        setConfig({ ...config, ports: newPorts });
    }
    const removePort = (idx: number) => setConfig({ ...config, ports: config.ports?.filter((_, i) => i !== idx) });

    const addEnv = () => setConfig({ ...config, env: [...(config.env || []), { key: "", value: "" }] });
    const updateEnv = (idx: number, field: "key" | "value", val: string) => {
        const newEnv = [...(config.env || [])];
        newEnv[idx][field] = val;
        setConfig({ ...config, env: newEnv });
    }
    const removeEnv = (idx: number) => setConfig({ ...config, env: config.env?.filter((_, i) => i !== idx) });

    const addVol = () => setConfig({ ...config, volumes: [...(config.volumes || []), { host: "", container: "" }] });
    const updateVol = (idx: number, field: "host" | "container", val: string) => {
        const newVols = [...(config.volumes || [])];
        newVols[idx][field] = val;
        setConfig({ ...config, volumes: newVols });
    }
    const removeVol = (idx: number) => setConfig({ ...config, volumes: config.volumes?.filter((_, i) => i !== idx) });

    return (
        <div className="modal-backdrop flex-center animate-fade-in">
            <div className="modal-content premium-card">
                <div className="modal-header flex-between mb-4">
                    <h2 style={{ fontSize: "20px" }}>Create Container from {imageName}</h2>
                    <button className="btn-icon" onClick={onClose} disabled={isCreating}><X size={20} /></button>
                </div>

                <div className="modal-body" style={{ maxHeight: "60vh", overflowY: "auto", paddingRight: "8px" }}>

                    {/* General */}
                    <section className="mb-4">
                        <h3 style={{ fontSize: "16px", color: "var(--accent-primary)", marginBottom: "12px" }}>General</h3>
                        <div className="input-group mb-2">
                            <label>Command Override (Optional)</label>
                            <input
                                className="premium-input"
                                placeholder="e.g., tail -f /dev/null"
                                value={config.command}
                                onChange={e => setConfig({ ...config, command: e.target.value })}
                            />
                        </div>
                        <div style={{ display: "flex", gap: "16px" }}>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>CPU Cores (Optional)</label>
                                <input
                                    type="number"
                                    step="0.1"
                                    min="0"
                                    className="premium-input"
                                    placeholder="e.g., 2"
                                    value={config.cpus || ""}
                                    onChange={e => setConfig({ ...config, cpus: parseFloat(e.target.value) || undefined })}
                                />
                            </div>
                            <div className="input-group" style={{ flex: 1 }}>
                                <label>Memory Limit (Min 200M)</label>
                                <input
                                    className="premium-input"
                                    placeholder="e.g., 4G or 512M"
                                    value={config.memory}
                                    onChange={e => setConfig({ ...config, memory: e.target.value })}
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
                        {config.ports?.map((port, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input className="premium-input" placeholder="Host Port" value={port.host} onChange={e => updatePort(idx, "host", e.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>:</span>
                                <input className="premium-input" placeholder="Container Port" value={port.container} onChange={e => updatePort(idx, "container", e.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removePort(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {config.ports?.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No ports configured.</p>}
                    </section>

                    {/* Env Vars */}
                    <section className="mb-4">
                        <div className="flex-between mb-2">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)" }}>Environment Variables (-e)</h3>
                            <button className="btn btn-secondary" style={{ padding: "4px 8px", fontSize: "12px" }} onClick={addEnv}>
                                <Plus size={14} style={{ marginRight: "4px" }} /> Add Env
                            </button>
                        </div>
                        {config.env?.map((e, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input className="premium-input" placeholder="KEY" value={e.key} onChange={ev => updateEnv(idx, "key", ev.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>=</span>
                                <input className="premium-input" placeholder="Value" value={e.value} onChange={ev => updateEnv(idx, "value", ev.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removeEnv(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {config.env?.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No environment variables configured.</p>}
                    </section>

                    {/* Volumes */}
                    <section className="mb-4">
                        <div className="flex-between mb-2">
                            <h3 style={{ fontSize: "16px", color: "var(--accent-primary)" }}>Bind Mounts (-v)</h3>
                            <button className="btn btn-secondary" style={{ padding: "4px 8px", fontSize: "12px" }} onClick={addVol}>
                                <Plus size={14} style={{ marginRight: "4px" }} /> Add Mount
                            </button>
                        </div>
                        {config.volumes?.map((vol, idx) => (
                            <div key={idx} style={{ display: "flex", gap: "8px", marginBottom: "8px" }}>
                                <input className="premium-input" placeholder="Host Path (/Users/)" value={vol.host} onChange={e => updateVol(idx, "host", e.target.value)} style={{ flex: 1 }} />
                                <span className="flex-center" style={{ color: "var(--text-secondary)" }}>:</span>
                                <input className="premium-input" placeholder="Container Path (/app)" value={vol.container} onChange={e => updateVol(idx, "container", e.target.value)} style={{ flex: 1 }} />
                                <button className="btn-icon text-danger" onClick={() => removeVol(idx)}><Trash2 size={16} /></button>
                            </div>
                        ))}
                        {config.volumes?.length === 0 && <p style={{ color: "var(--text-secondary)", fontSize: "14px", fontStyle: "italic" }}>No bind mounts configured.</p>}
                    </section>

                </div>

                <div className="modal-footer" style={{ marginTop: "16px", display: "flex", justifyContent: "flex-end", gap: "12px" }}>
                    <button className="btn btn-secondary" onClick={onClose} disabled={isCreating}>Cancel</button>
                    <button className="btn btn-primary" onClick={handleCreate} disabled={isCreating}>
                        {isCreating ? "Creating..." : "Create Container"}
                    </button>
                </div>
            </div>
        </div>
    );
}
