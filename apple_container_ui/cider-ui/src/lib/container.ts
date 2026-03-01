// Browser-side library for interacting with the Cider Go Backend

export interface SystemProperty {
    ID: string;
    Value: string;
    Type: string;
    Description: string;
}

export const API_BASE = "http://localhost:7777/api";

export type ContainerState = "running" | "stopped" | "paused" | "exited" | "dead" | "unknown";

export interface ContainerInfo {
    ID: string;
    Image: string;
    Status: string;
    State: ContainerState;
    Names: string;
    CreatedAt?: string;
    Ports: any;
    Networks: any;
    Env?: string[];
    Mounts?: any;
    CPUs?: number;
    MemoryBytes?: number;
}

export interface ContainerConfig {
    image: string;
    command?: string;
    ports?: { host: string; container: string }[];
    env?: { key: string; value: string }[];
    volumes?: { host: string; container: string }[];
    cpus?: number;
    memory?: string;
}

export interface ImageInfo {
    ID: string;
    Repository: string;
    Tag: string;
    Size: string;
    CreatedAt: string;
    Name?: string; // Compatibility
}

export interface BuilderStatus {
    id: string;
    status: string;
    cpus?: number;
    memoryInBytes?: number;
    image?: string;
}

export interface VolumeInfo {
    name: string;
    driver: string;
    source: string;
    sizeInBytes?: number;
}

export interface NetworkInfo {
    id: string;
    state: string;
    config: {
        id: string;
        mode: string;
        creationDate: number;
        pluginInfo?: {
            plugin: string;
            variant: string;
        };
        labels?: Record<string, string>;
    };
    status: {
        ipv4Subnet?: string;
        ipv4Gateway?: string;
        ipv6Subnet?: string;
        ipv6Gateway?: string;
    };
}

export interface CommandLog {
    id: string;
    timestamp: string;
    command: string;
    output: string;
    isError: boolean;
}

// --- System ---

export async function checkSystemStatus(): Promise<boolean> {
    try {
        const res = await fetch(`${API_BASE}/status`);
        if (!res.ok) return false;
        const data = await res.json();
        return data.status === "ok";
    } catch {
        return false;
    }
}

export async function startSystem(): Promise<void> {
    await fetch(`${API_BASE}/system/start`, { method: "POST" });
}

export async function stopSystem(): Promise<void> {
    await fetch(`${API_BASE}/system/stop`, { method: "POST" });
}

export async function pruneSystem(): Promise<void> {
    await fetch(`${API_BASE}/system/prune`, { method: "POST" });
}

export async function getActionLogs(): Promise<CommandLog[]> {
    try {
        const res = await fetch(`${API_BASE}/logs`);
        if (!res.ok) return [];
        const data = await res.json();
        return Array.isArray(data) ? data : [];
    } catch {
        return [];
    }
}

export async function clearActionLogs(): Promise<void> {
    await fetch(`${API_BASE}/logs`, { method: "DELETE" });
}

// --- Containers ---

export async function listContainers(): Promise<ContainerInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/containers`);
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        const data = await res.json();
        const raw: any[] = Array.isArray(data) ? data : [];
        return raw.map(c => ({
            ID: c.configuration?.id || c.ID || c.id || "",
            Image: c.configuration?.image?.reference || c.Image || c.image || "",
            Status: c.status || c.Status || "",
            State: c.state || (c.status?.toLowerCase().includes("running") ? "running" : "stopped"),
            Names: c.Names || c.names || c.configuration?.id || "",
            CreatedAt: c.CreatedAt || c.createdAt || "",
            Ports: c.Ports || c.ports || c.configuration?.publishedPorts || [],
            Networks: c.Networks || c.networks || [],
            Env: c.configuration?.initProcess?.environment || [],
            Mounts: c.configuration?.mounts || [],
            CPUs: c.configuration?.resources?.cpus,
            MemoryBytes: c.configuration?.resources?.memoryInBytes
        }));
    } catch (err) {
        console.error("Failed to fetch containers:", err);
        return [];
    }
}

export async function inspectContainer(id: string): Promise<any | null> {
    try {
        const res = await fetch(`${API_BASE}/containers/inspect/${id}`);
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        const c: any = await res.json();
        return c[0] || c;
    } catch (err) {
        console.error(`Failed to inspect container ${id}:`, err);
        return null;
    }
}

export async function createContainer(config: ContainerConfig): Promise<void> {
    const res = await fetch(`${API_BASE}/containers`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(config)
    });
    if (!res.ok) throw new Error(await res.text());
}

export async function startContainer(id: string): Promise<void> {
    const res = await fetch(`${API_BASE}/containers/${id}/start`, { method: "POST" });
    if (!res.ok) throw new Error(await res.text() || `Failed to start container ${id}`);
}

export async function stopContainer(id: string): Promise<void> {
    const res = await fetch(`${API_BASE}/containers/${id}/stop`, { method: "POST" });
    if (!res.ok) throw new Error(await res.text() || `Failed to stop container ${id}`);
}

export async function removeContainer(id: string): Promise<void> {
    const res = await fetch(`${API_BASE}/containers/${id}`, { method: "DELETE" });
    if (!res.ok) throw new Error(await res.text() || `Failed to remove container ${id}`);
}

export async function getContainerLogs(id: string, lines = 100): Promise<string> {
    try {
        const res = await fetch(`${API_BASE}/containers/logs/${id}?n=${lines}`);
        return await res.text();
    } catch (err) {
        return `Error: ${err}`;
    }
}

export async function execContainer(id: string, command: string): Promise<{ stdout: string; stderr: string; error?: string }> {
    try {
        const res = await fetch(`${API_BASE}/containers/exec/${id}`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ command })
        });
        return await res.json();
    } catch (err: any) {
        return {
            stdout: err.stdout || "",
            stderr: err.stderr || "",
            error: err.message || String(err)
        };
    }
}

export async function execContainerStream(
    id: string,
    command: string,
    onOutput: (type: "out" | "err" | "sys", text: string) => void
): Promise<void> {
    const res = await fetch(`${API_BASE}/containers/exec-stream/${id}`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ command })
    });

    if (!res.body) throw new Error("Stream not supported by browser/backend");
    const reader = res.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    while (true) {
        const { value, done } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split("\n");
        buffer = lines.pop() || "";

        for (const line of lines) {
            if (!line.trim()) continue;
            try {
                const chunk = JSON.parse(line);
                onOutput(chunk.type, chunk.text);
            } catch (e) {
                console.error("Failed to parse stream chunk:", e, line);
            }
        }
    }
}

export async function getContainerStats(): Promise<any[]> {
    try {
        const res = await fetch(`${API_BASE}/stats`);
        if (!res.ok) return [];
        return await res.json();
    } catch {
        return [];
    }
}

// --- Images ---

export async function listImages(): Promise<ImageInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/images`);
        if (!res.ok) return [];
        const data = await res.json();
        const raw: any[] = Array.isArray(data) ? data : [];

        return raw.map(img => {
            const reference = img.reference || "";
            const lastColonIndex = reference.lastIndexOf(':');
            let repo = reference;
            let tag = "latest";
            if (lastColonIndex > 0) {
                repo = reference.substring(0, lastColonIndex);
                tag = reference.substring(lastColonIndex + 1);
            }
            if (repo.startsWith("docker.io/library/")) {
                repo = repo.replace("docker.io/library/", "");
            }

            let digest = img.descriptor?.digest || img.ID || img.id || "";
            if (digest.startsWith("sha256:")) {
                digest = digest.replace("sha256:", "");
            }
            const shortId = digest.substring(0, 12);

            return {
                ID: shortId,
                Name: reference,
                Repository: repo,
                Tag: tag,
                Size: img.fullSize || (img.descriptor?.size ? `${(img.descriptor.size / (1024 * 1024)).toFixed(1)} MB` : "-"),
                CreatedAt: img.descriptor?.annotations?.["org.opencontainers.image.created"] || img.CreatedAt || img.createdAt || "-"
            };
        });
    } catch (err) {
        console.error("Failed to list images:", err);
        return [];
    }
}

export async function pullImage(reference: string): Promise<void> {
    const res = await fetch(`${API_BASE}/images`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ reference })
    });
    if (!res.ok) throw new Error(await res.text());
}

export async function deleteImage(reference: string): Promise<void> {
    const res = await fetch(`${API_BASE}/images?reference=${encodeURIComponent(reference)}`, {
        method: "DELETE"
    });
    if (!res.ok) throw new Error(await res.text());
}

export async function buildImage(context: string, tag: string): Promise<void> {
    await fetch(`${API_BASE}/images/build`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ context, tag })
    });
}

export async function saveImage(name: string, path: string): Promise<void> {
    await fetch(`${API_BASE}/images/save`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, path })
    });
}

export async function loadImage(path: string): Promise<void> {
    await fetch(`${API_BASE}/images/load`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path })
    });
}

export async function tagImage(name: string, tag: string): Promise<void> {
    await fetch(`${API_BASE}/images/tag`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, tag })
    });
}

// --- Volumes ---

export async function listVolumes(): Promise<VolumeInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/volumes`);
        const data = await res.json();
        return Array.isArray(data) ? data : [];
    } catch {
        return [];
    }
}

export async function createVolume(name: string): Promise<void> {
    const res = await fetch(`${API_BASE}/volumes`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name })
    });
    if (!res.ok) throw new Error(await res.text() || "Failed to create volume");
}

export async function removeVolume(name: string): Promise<void> {
    const res = await fetch(`${API_BASE}/volumes/${name}`, { method: "DELETE" });
    if (!res.ok) throw new Error(await res.text() || "Failed to remove volume");
}

// --- Networks ---

export async function listNetworks(): Promise<NetworkInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/networks`);
        if (!res.ok) return [];
        const data = await res.json();
        return Array.isArray(data) ? data : [];
    } catch {
        return [];
    }
}

export async function createNetwork(name: string): Promise<void> {
    const res = await fetch(`${API_BASE}/networks`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name })
    });
    if (!res.ok) throw new Error(await res.text() || "Failed to create network");
}

export async function removeNetwork(name: string): Promise<void> {
    const res = await fetch(`${API_BASE}/networks/${name}`, { method: "DELETE" });
    if (!res.ok) throw new Error(await res.text() || "Failed to remove network");
}

// --- Builder ---

export async function getBuilderStatus(): Promise<BuilderStatus | null> {
    try {
        const res = await fetch(`${API_BASE}/builder`);
        if (!res.ok) return null;
        const raw = await res.json();
        const b = Array.isArray(raw) ? raw[0] : raw;
        if (!b) return null;
        return {
            id: b.configuration?.id || "buildkit",
            status: b.status || "stopped",
            cpus: b.configuration?.resources?.cpus,
            memoryInBytes: b.configuration?.resources?.memoryInBytes,
            image: b.configuration?.image?.reference
        };
    } catch {
        return null;
    }
}

export async function startBuilder(): Promise<void> {
    await fetch(`${API_BASE}/builder/start`, { method: "POST" });
}

export async function stopBuilder(): Promise<void> {
    await fetch(`${API_BASE}/builder/stop`, { method: "POST" });
}

// --- Registry ---

export async function listRegistries(): Promise<string[]> {
    try {
        const res = await fetch(`${API_BASE}/registry`);
        if (!res.ok) return [];
        const data = await res.json();
        return Array.isArray(data) ? data : [];
    } catch {
        return [];
    }
}

export async function registryLogin(server: string, user: string, pass: string): Promise<void> {
    await fetch(`${API_BASE}/registry/login`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ server, username: user, password: pass })
    });
}

export async function registryLogout(server: string): Promise<void> {
    await fetch(`${API_BASE}/registry/logout/${server}`, { method: "POST" });
}

// --- System Properties ---

export async function listSystemProperties(): Promise<SystemProperty[]> {
    try {
        const res = await fetch(`${API_BASE}/system/properties`);
        if (!res.ok) return [];
        const data = await res.json();
        const rawProps: any[] = Array.isArray(data) ? data : [];
        return rawProps.map(p => ({
            ID: p.id || p.ID || "",
            Value: p.value !== null && p.value !== undefined ? String(p.value) : "",
            Type: p.type || p.Type || "",
            Description: p.description || p.Description || ""
        }));
    } catch {
        return [];
    }
}

export async function setSystemProperty(id: string, value: string): Promise<void> {
    const res = await fetch(`${API_BASE}/system/properties/set`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, value })
    });
    if (!res.ok) throw new Error(await res.text() || "Failed to set property");
}

export async function clearSystemProperty(id: string): Promise<void> {
    const res = await fetch(`${API_BASE}/system/properties/clear/${id}`, { method: "POST" });
    if (!res.ok) throw new Error(await res.text() || "Failed to clear property");
}

// --- DNS ---

export async function listDnsDomains(): Promise<string[]> {
    try {
        const res = await fetch(`${API_BASE}/dns`);
        if (!res.ok) return [];
        const raw = await res.json();
        return Array.isArray(raw) ? raw : [];
    } catch {
        return [];
    }
}

export async function createDnsDomain(domain: string): Promise<void> {
    const res = await fetch(`${API_BASE}/dns`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ domain })
    });
    if (!res.ok) throw new Error(await res.text() || "Failed to create DNS domain");
}

export async function deleteDnsDomain(domain: string): Promise<void> {
    const res = await fetch(`${API_BASE}/dns/${domain}`, { method: "DELETE" });
    if (!res.ok) throw new Error(await res.text() || "Failed to delete DNS domain");
}

// --- Files & Dockerfiles ---

export async function findDockerfiles(baseDir?: string): Promise<{ path: string; name: string }[]> {
    try {
        const url = baseDir ? `${API_BASE}/dockerfiles?baseDir=${encodeURIComponent(baseDir)}` : `${API_BASE}/dockerfiles`;
        const res = await fetch(url);
        if (!res.ok) throw new Error("Failed to find Dockerfiles");
        return await res.json();
    } catch (e) {
        console.error("Failed to search Dockerfiles:", e);
        return [];
    }
}

export async function readFileContent(path: string): Promise<string> {
    try {
        const res = await fetch(`${API_BASE}/files/read?path=${encodeURIComponent(path)}`);
        if (!res.ok) throw new Error(`Failed to read file: ${res.statusText}`);
        return await res.text();
    } catch (e: any) {
        throw new Error(`Failed to read file: ${e.message}`);
    }
}
