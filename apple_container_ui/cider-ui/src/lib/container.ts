// Browser-side library for interacting with the Cider Go Backend

export interface SystemProperty {
    ID: string;
    Value: string;
    Type: string;
    Description: string;
}

const API_BASE = "http://localhost:7777/api";

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
    status: "running" | "stopped";
    id: string;
    cpus?: number;
    memoryInBytes?: number;
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

export async function listContainers(): Promise<ContainerInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/containers`);
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        const raw: any[] = await res.json();
        return raw.map(c => ({
            ID: c.configuration?.id || c.ID || c.id || "",
            Image: c.configuration?.image?.reference || c.Image || c.image || "",
            Status: c.status || c.Status || "",
            State: c.state || (c.status === "running" ? "running" : "stopped"),
            Names: c.Names || c.names || c.configuration?.id || "",
            CreatedAt: c.CreatedAt || c.createdAt || "",
            Ports: c.Ports || c.ports || [],
            Networks: c.Networks || c.networks || []
        }));
    } catch (err) {
        console.error("Failed to fetch containers:", err);
        return [];
    }
}

export async function inspectContainer(id: string): Promise<ContainerInfo | null> {
    try {
        const res = await fetch(`${API_BASE}/containers/inspect/${id}`);
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        const c: any = await res.json();
        return {
            ID: c.configuration?.id || c.ID || c.id || "",
            Image: c.configuration?.image?.reference || c.Image || c.image || "",
            Status: c.status || c.Status || "",
            State: c.state || (c.status === "running" ? "running" : "stopped"),
            Names: c.Names || c.names || c.configuration?.id || "",
            CreatedAt: c.CreatedAt || c.createdAt || "",
            Ports: c.Ports || c.ports || [],
            Networks: c.Networks || c.networks || []
        };
    } catch (err) {
        console.error(`Failed to inspect container ${id}:`, err);
        return null;
    }
}

export async function checkSystemStatus(): Promise<boolean> {
    try {
        const res = await fetch(`${API_BASE}/status`);
        return res.ok;
    } catch {
        return false;
    }
}

export async function listImages(): Promise<ImageInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/images`);
        if (!res.ok) return [];
        const raw: any[] = await res.json();
        return raw.map(img => ({
            ID: img.ID || img.id || "",
            Repository: img.Repository || img.repository || "",
            Tag: img.Tag || img.tag || "latest",
            Size: img.Size || img.size || "0B",
            CreatedAt: img.CreatedAt || img.createdAt || ""
        }));
    } catch {
        return [];
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

export async function getActionLogs(): Promise<any[]> {
    return [];
}

export async function pruneSystem() {
    return fetch(`${API_BASE}/system/prune`, { method: "POST" });
}

// --- Volumes ---
export async function listVolumes(): Promise<any[]> {
    try {
        const res = await fetch(`${API_BASE}/volumes`);
        return await res.json();
    } catch {
        return [];
    }
}

export async function createVolume(name: string) {
    return fetch(`${API_BASE}/volumes`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name })
    });
}

export async function removeVolume(name: string) {
    return fetch(`${API_BASE}/volumes/${name}`, { method: "DELETE" });
}

// --- Networks ---
export async function listNetworks(): Promise<NetworkInfo[]> {
    try {
        const res = await fetch(`${API_BASE}/networks`);
        if (!res.ok) return [];
        return await res.json();
    } catch {
        return [];
    }
}

export async function createNetwork(name: string) {
    return fetch(`${API_BASE}/networks`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name })
    });
}

export async function removeNetwork(name: string) {
    return fetch(`${API_BASE}/networks/${name}`, { method: "DELETE" });
}

// --- Builder ---
export async function getBuilderStatus(): Promise<any> {
    try {
        const res = await fetch(`${API_BASE}/builder`);
        if (!res.ok) return null;
        return await res.json();
    } catch {
        return null;
    }
}

export async function startBuilder() {
    return fetch(`${API_BASE}/builder/start`, { method: "POST" });
}

export async function stopBuilder() {
    return fetch(`${API_BASE}/builder/stop`, { method: "POST" });
}

// --- Images ---
export async function pullImage(reference: string) {
    return fetch(`${API_BASE}/images/pull`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ reference })
    });
}

export async function deleteImage(id: string) {
    return fetch(`${API_BASE}/images/${id}`, { method: "DELETE" });
}

// --- Exec ---
export async function execContainer(id: string, command: string) {
    try {
        const res = await fetch(`${API_BASE}/containers/exec/${id}`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ command })
        });
        return await res.json();
    } catch (err: any) {
        return { stdout: "", stderr: "", error: err.message };
    }
}

// --- Registries ---
export async function listRegistries(): Promise<string[]> {
    try {
        const res = await fetch(`${API_BASE}/registry`);
        if (!res.ok) return [];
        return await res.json();
    } catch {
        return [];
    }
}

export async function registryLogin(server: string, user: string, pass: string) {
    return fetch(`${API_BASE}/registry/login`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ server, username: user, password: pass })
    });
}

export async function registryLogout(server: string) {
    return fetch(`${API_BASE}/registry/logout/${server}`, { method: "POST" });
}

// --- System Properties ---
export async function listSystemProperties(): Promise<SystemProperty[]> {
    try {
        const res = await fetch(`${API_BASE}/system/properties`);
        if (!res.ok) return [];
        return await res.json();
    } catch {
        return [];
    }
}

export async function setSystemProperty(id: string, value: string) {
    return fetch(`${API_BASE}/system/properties/set`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, value })
    });
}

export async function clearSystemProperty(id: string) {
    return fetch(`${API_BASE}/system/properties/clear/${id}`, { method: "POST" });
}

// --- Logs ---
// --- DNS ---
export async function listDnsDomains(): Promise<string[]> {
    try {
        const res = await fetch(`${API_BASE}/dns/domains`);
        if (!res.ok) return [];
        const text = await res.text();
        if (!text.trim()) return [];
        return text.split('\n').map(line => line.trim()).filter(line => line);
    } catch {
        return [];
    }
}

export async function createDnsDomain(domain: string) {
    return fetch(`${API_BASE}/dns/domains`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ domain })
    });
}

export async function deleteDnsDomain(domain: string) {
    return fetch(`${API_BASE}/dns/domains/${domain}`, { method: "DELETE" });
}

export async function getContainerLogs(id: string, lines = 100): Promise<string> {
    try {
        const res = await fetch(`${API_BASE}/containers/logs/${id}?n=${lines}`);
        return await res.text();
    } catch (err) {
        return `Error: ${err}`;
    }
}

export async function buildImage(context: string, tag: string) {
    return fetch(`${API_BASE}/images/build`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ context, tag })
    });
}

export async function saveImage(name: string, path: string) {
    return fetch(`${API_BASE}/images/save`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, path })
    });
}

export async function loadImage(path: string) {
    return fetch(`${API_BASE}/images/load`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path })
    });
}

export async function tagImage(name: string, tag: string) {
    return fetch(`${API_BASE}/images/tag`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name, tag })
    });
}

// --- Lifecycle ---
export async function startContainer(id: string) {
    return fetch(`${API_BASE}/containers/${id}/start`, { method: "POST" });
}

export async function stopContainer(id: string) {
    return fetch(`${API_BASE}/containers/${id}/stop`, { method: "POST" });
}

export async function removeContainer(id: string) {
    return fetch(`${API_BASE}/containers/${id}`, { method: "DELETE" });
}

/**
 * Recursively find all Dockerfiles in the given directory or project root
 */
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

/**
 * Read the content of a file
 */
export async function readFileContent(path: string): Promise<string> {
    try {
        const res = await fetch(`${API_BASE}/files/read?path=${encodeURIComponent(path)}`);
        if (!res.ok) throw new Error(`Failed to read file: ${res.statusText}`);
        return await res.text();
    } catch (e: any) {
        throw new Error(`Failed to read file: ${e.message}`);
    }
}
