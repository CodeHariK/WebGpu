"use server";

import { exec } from "child_process";
import { promisify } from "util";
import { readdir, stat, readFile } from "fs/promises";
import { join, relative, isAbsolute } from "path";

const execAsync = promisify(exec);

export type ContainerState = "running" | "stopped" | "paused" | "exited" | "dead" | "unknown";
export interface ContainerInfo {
    Command?: string;
    CreatedAt?: string;
    ID: string;
    Image: string;
    Labels?: any;
    LocalVolumes?: string;
    Mounts?: any;
    Names: string;
    Networks?: any[];
    Ports?: any;
    RunningFor?: string;
    Size?: string;
    Status: string;
    State?: ContainerState;
    Env?: string[];
    CPUs?: number;
    MemoryBytes?: number;
}


export interface CommandLog {
    id: string;
    timestamp: string;
    command: string;
    output: string;
    isError: boolean;
}

const actionLogs: CommandLog[] = [];

// Helper to execute commands and handle errors robustly
async function runCli(command: string): Promise<string> {
    const timestamp = new Date().toISOString();
    const id = Math.random().toString(36).substring(2, 9);
    try {
        const { stdout, stderr } = await execAsync(command);
        let outputStr = stdout.trim();
        if (stderr && !stderr.includes("WARN")) {
            console.warn(`CLI Stderr: ${stderr}`);
            outputStr += (outputStr ? '\n' : '') + `[stderr]: ${stderr.trim()}`;
        }

        // Exclude noisy polling commands from logs unless they error
        if (!command.includes("ls --all") && !command.includes("image list") && !command.includes("container ls")) {
            actionLogs.push({ id, timestamp, command, output: outputStr, isError: false });
        }

        return stdout.trim();
    } catch (error: any) {
        console.error(`CLI Error executing '${command}':`, error);
        const errMsg = error.message || String(error);
        if (!command.includes("ls --all") && !command.includes("image list") && !command.includes("container ls")) {
            actionLogs.push({ id, timestamp, command, output: errMsg, isError: true });
        }
        throw new Error(errMsg);
    }
}

export async function getActionLogs(): Promise<CommandLog[]> {
    return actionLogs;
}

export async function clearActionLogs(): Promise<void> {
    actionLogs.length = 0;
}

/**
 * Check if the container apiserver daemon is responding.
 */
export async function checkSystemStatus(): Promise<boolean> {
    try {
        // We can infer it's running if `container ls` returns successfully (even if empty)
        await runCli("container ls");
        return true;
    } catch (e) {
        return false;
    }
}

/**
 * Start the container system services
 */
export async function startSystem(): Promise<void> {
    await runCli("container system start");
}

/**
 * Stop the container system services
 */
export async function stopSystem(): Promise<void> {
    await runCli("container system stop");
}

/**
 * List all containers (running and stopped)
 */
export async function listContainers(): Promise<ContainerInfo[]> {
    try {
        const output = await runCli("container ls --all --format json");
        if (!output.trim()) return [];

        let raw: any[] = [];
        try {
            raw = JSON.parse(output);
            if (!Array.isArray(raw)) raw = [raw];
        } catch {
            raw = output.split('\n').filter(Boolean).map(line => {
                try { return JSON.parse(line); } catch { return null; }
            }).filter(Boolean);
        }

        return raw.map(c => {
            const id = c.configuration?.id || c.ID || c.Id || "";
            const image = c.configuration?.image?.reference || c.Image || c.image || "";
            const status = c.status || c.Status || "";
            const ports = c.configuration?.publishedPorts || c.Ports || [];
            const createdAt = c.startedDate ? new Date(c.startedDate * 1000).toLocaleString() : (c.CreatedAt || c.createdAt || "");

            let state: ContainerState = "unknown";
            const statusLower = status.toLowerCase();
            if (statusLower.includes("up") || statusLower.includes("running")) state = "running";
            else if (statusLower.includes("exited") || statusLower.includes("stopped")) state = "exited";

            return {
                ID: id,
                Image: image,
                Status: status,
                State: state,
                Ports: ports,
                Names: id, // Use ID as name if not provided
                CreatedAt: createdAt,
                Networks: c.networks || [],
                Env: c.configuration?.initProcess?.environment || [],
                Mounts: c.configuration?.mounts || [],
                CPUs: c.configuration?.resources?.cpus,
                MemoryBytes: c.configuration?.resources?.memoryInBytes
            };
        });
    } catch (e: any) {
        if (e.message?.includes("connection refused") || e.message?.includes("daemon is not running") || e.message?.includes("apiserver is not running")) {
            return [];
        }
        throw e;
    }
}

/**
 * Start a container by ID
 */
export async function startContainer(id: string): Promise<void> {
    await runCli(`container start ${id}`);
}

/**
 * Stop a container by ID
 */
export async function stopContainer(id: string): Promise<void> {
    await runCli(`container stop ${id}`);
}

/**
 * Remove a container by ID
 */
export async function removeContainer(id: string, force = false): Promise<void> {
    const forceFlag = force ? "--force" : "";
    await runCli(`container rm ${forceFlag} ${id}`);
}

/**
 * Get container logs
 */
export async function getContainerLogs(id: string, lines = 100): Promise<string> {
    try {
        return await runCli(`container logs -n ${lines} ${id}`);
    } catch (e) {
        return `Error fetching logs: ${e}`;
    }
}

/**
 * Inspect container for deep details
 */
export async function inspectContainer(id: string): Promise<any> {
    const output = await runCli(`container inspect ${id}`);
    try {
        return JSON.parse(output)[0]; // Inspect usually returns an array
    } catch {
        return null;
    }
}

/**
 * Execute a command inside a running container
 */
export async function execContainer(id: string, cmd: string): Promise<{ stdout: string; stderr: string; error?: string }> {
    try {
        // We use execAsync directly here because we want to separate stdout and stderr for the UI terminal
        // and bypass the global action logger for these interactive commands to avoid noise
        const { stdout, stderr } = await execAsync(`container exec ${id} ${cmd}`);
        return { stdout, stderr };
    } catch (error: any) {
        return {
            stdout: error.stdout || "",
            stderr: error.stderr || "",
            error: error.message || String(error)
        };
    }
}

/**
 * Get live stats for running containers
 */
export async function getContainerStats(): Promise<any[]> {
    try {
        const output = await runCli(`container stats --no-stream --format json`);
        return JSON.parse(output);
    } catch (e) {
        console.error("Failed to get container stats:", e);
        return [];
    }
}

/**
 * Prune unused containers and networks
 */
export async function pruneSystem(): Promise<string> {
    try {
        return await runCli(`container system prune --force`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to prune system");
    }
}

/**
 * List all volumes
 */
export async function listVolumes(): Promise<VolumeInfo[]> {
    try {
        const output = await runCli(`container volume ls --format json`);
        if (!output.trim()) return [];
        return JSON.parse(output);
    } catch (e: any) {
        console.error("Failed to list volumes:", e);
        return [];
    }
}

/**
 * Create a new volume
 */
export async function createVolume(name: string): Promise<string> {
    try {
        return await runCli(`container volume create ${name}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to create volume");
    }
}

/**
 * Remove a volume by name
 */
export async function removeVolume(name: string, force = false): Promise<string> {
    try {
        const forceFlag = force ? "--force" : "";
        return await runCli(`container volume rm ${forceFlag} ${name}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to remove volume");
    }
}

export interface VolumeInfo {
    name: string;
    source: string;
    format: string;
    driver: string;
    createdAt: number;
    sizeInBytes: number;
    labels: Record<string, string>;
    options: Record<string, string>;
}

/**
 * List all networks
 */
export async function listNetworks(): Promise<NetworkInfo[]> {
    try {
        const output = await runCli(`container network ls --format json`);
        if (!output.trim()) return [];
        return JSON.parse(output);
    } catch (e: any) {
        console.error("Failed to list networks:", e);
        return [];
    }
}

/**
 * Create a new network
 */
export async function createNetwork(name: string): Promise<string> {
    try {
        return await runCli(`container network create ${name}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to create network");
    }
}

/**
 * Remove a network by name
 */
export async function removeNetwork(name: string, force = false): Promise<string> {
    try {
        return await runCli(`container network rm ${name}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to remove network");
    }
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

export interface ContainerConfig {
    image: string;
    command?: string;
    ports?: { host: string; container: string }[];
    env?: { key: string; value: string }[];
    volumes?: { host: string; container: string }[];
    cpus?: number;
    memory?: string; // e.g. "4G"
}

/**
 * Create and run a new container with advanced configuration
 */
export async function createContainer(config: ContainerConfig): Promise<void> {
    let args = "-d ";

    if (config.cpus) args += `--cpus ${config.cpus} `;
    if (config.memory) args += `--memory ${config.memory} `;

    if (config.ports && config.ports.length > 0) {
        args += config.ports.map(p => `-p ${p.host}:${p.container}`).join(" ") + " ";
    }

    if (config.env && config.env.length > 0) {
        args += config.env.map(e => `-e ${e.key}=${e.value}`).join(" ") + " ";
    }

    if (config.volumes && config.volumes.length > 0) {
        args += config.volumes.map(v => `-v ${v.host}:${v.container}`).join(" ") + " ";
    }

    args += config.image;

    if (config.command) {
        args += ` ${config.command}`;
    }

    await runCli(`container run ${args}`);
}

/**
 * Pull down an image
 */
export async function pullImage(reference: string): Promise<string> {
    try {
        return await runCli(`container image pull ${reference}`);
    } catch (e: any) {
        throw new Error(`Failed to pull image ${reference}: ` + (e.message || String(e)));
    }
}

export interface ImageInfo {
    ID: string;
    Name: string;
    Repository: string;
    Tag: string;
    Size: string;
    CreatedAt: string;
}

/**
 * List all downloaded images
 */
export async function listImages(): Promise<ImageInfo[]> {
    try {
        const output = await runCli("container image list --format json");
        if (!output.trim()) return [];

        const raw: any[] = JSON.parse(output);
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

            let digest = img.descriptor?.digest || img.ID || "";
            if (digest.startsWith("sha256:")) {
                digest = digest.replace("sha256:", "");
            }

            return {
                ID: digest,
                Name: reference,
                Repository: repo,
                Tag: tag,
                Size: img.fullSize || img.descriptor?.size || "-",
                CreatedAt: img.descriptor?.annotations?.["org.opencontainers.image.created"] || "-"
            };
        });
    } catch (e: any) {
        if (e.message?.includes("connection refused") || e.message?.includes("daemon is not running")) {
            return [];
        }
        throw e;
    }
}

/**
 * Delete a downloaded image
 */
export async function deleteImage(imageRef: string, force = false): Promise<void> {
    const forceFlag = force ? "--force" : "";
    await runCli(`container image delete ${forceFlag} ${imageRef}`);
}

/**
 * Build an image
 */
export async function buildImage(dir: string, tag: string): Promise<string> {
    try {
        return await runCli(`container build -t ${tag} ${dir}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to build image");
    }
}

/**
 * Save an image to a tarball
 */
export async function saveImage(name: string, output: string): Promise<string> {
    try {
        return await runCli(`container image save --output ${output} ${name}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to save image");
    }
}

/**
 * Load an image from a tarball
 */
export async function loadImage(input: string): Promise<string> {
    try {
        return await runCli(`container image load --input ${input}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to load image");
    }
}

/**
 * Tag an image
 */
export async function tagImage(source: string, target: string): Promise<string> {
    try {
        return await runCli(`container image tag ${source} ${target}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to tag image");
    }
}

/**
 * Get Builder status
 */
export async function getBuilderStatus(): Promise<BuilderStatus | null> {
    try {
        const output = await runCli(`container builder status --format json`);
        if (!output.trim() || output.includes("builder is not running")) return null;

        const builders: any[] = JSON.parse(output);
        if (builders.length === 0) return null;

        const b = builders[0];
        return {
            id: b.configuration?.id || "buildkit",
            status: b.status || "unknown",
            cpus: b.configuration?.resources?.cpus,
            memoryInBytes: b.configuration?.resources?.memoryInBytes,
            image: b.configuration?.image?.reference
        };
    } catch (e: any) {
        return null;
    }
}

export async function startBuilder(): Promise<string> {
    try {
        return await runCli(`container builder start`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to start builder");
    }
}

export async function stopBuilder(): Promise<string> {
    try {
        return await runCli(`container builder stop`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to stop builder");
    }
}

export interface BuilderStatus {
    id: string;
    status: string;
    cpus?: number;
    memoryInBytes?: number;
    image?: string;
}

// --- Registry Management ---

/**
 * List active registry sessions
 */
export async function listRegistries(): Promise<string[]> {
    try {
        const output = await runCli(`container registry list --format json`);
        if (!output.trim()) return [];
        return JSON.parse(output);
    } catch (e: any) {
        if (e.message?.includes("connection refused") || e.message?.includes("daemon is not running")) {
            return [];
        }
        console.error("Failed to list registries:", e);
        return [];
    }
}

/**
 * Login to a registry
 */
export async function registryLogin(server: string, user: string, pass: string): Promise<string> {
    return new Promise((resolve, reject) => {
        const child = exec(`container registry login --username ${user} --password-stdin ${server}`, (error, stdout, stderr) => {
            if (error) {
                return reject(new Error(stderr || error.message));
            }
            resolve(stdout.trim());
        });

        if (child.stdin) {
            child.stdin.write(pass + "\n");
            child.stdin.end();
        } else {
            reject(new Error("Unable to write to stdin"));
        }
    });
}

/**
 * Logout of a registry
 */
export async function registryLogout(server: string): Promise<string> {
    try {
        return await runCli(`container registry logout ${server}`);
    } catch (e: any) {
        throw new Error(e.message || "Failed to logout of registry");
    }
}

// --- System Properties & DNS Management ---

export interface SystemProperty {
    ID: string;
    Value: string;
    Type: string;
    Description: string;
}

export async function listSystemProperties(): Promise<SystemProperty[]> {
    try {
        const output = await runCli(`container system property list --format json`);
        if (!output.trim()) return [];
        const rawProps: any[] = JSON.parse(output);
        return rawProps.map(p => ({
            ID: p.id || p.ID || "",
            Value: p.value !== null && p.value !== undefined ? String(p.value) : "",
            Type: p.type || p.Type || "",
            Description: p.description || p.Description || ""
        }));
    } catch (e: any) {
        if (e.message?.includes("connection refused") || e.message?.includes("daemon is not running")) {
            return [];
        }
        console.error("Failed to list system properties:", e);
        return [];
    }
}

export async function getSystemProperty(id: string): Promise<string> {
    try {
        return await runCli(`container system property get ${id}`);
    } catch (e: any) {
        throw new Error(e.message || `Failed to get property: ${id}`);
    }
}

export async function setSystemProperty(id: string, value: string): Promise<string> {
    try {
        return await runCli(`container system property set ${id} ${value}`);
    } catch (e: any) {
        throw new Error(e.message || `Failed to set property: ${id}`);
    }
}

export async function clearSystemProperty(id: string): Promise<string> {
    try {
        return await runCli(`container system property clear ${id}`);
    } catch (e: any) {
        throw new Error(e.message || `Failed to clear property: ${id}`);
    }
}

export async function listDnsDomains(): Promise<string[]> {
    try {
        const output = await runCli(`container system dns list`);
        if (!output.trim()) return [];
        return output.split('\n').map(line => line.trim()).filter(line => line);
    } catch (e: any) {
        return [];
    }
}

export async function createDnsDomain(domain: string): Promise<string> {
    try {
        return await runCli(`container system dns create ${domain}`);
    } catch (e: any) {
        throw new Error(e.message || `Failed to create DNS domain: ${domain}`);
    }
}

export async function deleteDnsDomain(domain: string): Promise<string> {
    try {
        return await runCli(`container system dns delete ${domain}`);
    } catch (e: any) {
        throw new Error(e.message || `Failed to delete DNS domain: ${domain}`);
    }
}

/**
 * Recursively find all Dockerfiles in the given directory or project root
 */
export async function findDockerfiles(baseDir?: string): Promise<{ path: string; name: string }[]> {
    const rootDir = process.cwd();
    let startDir = baseDir || rootDir;

    // Resolve relative paths if provided
    if (!isAbsolute(startDir)) {
        startDir = join(rootDir, startDir);
    }

    const results: { path: string; name: string }[] = [];

    async function search(dir: string) {
        try {
            const files = await readdir(dir);
            for (const file of files) {
                const fullPath = join(dir, file);

                if (file === "node_modules" || file.startsWith(".")) continue;

                const s = await stat(fullPath);
                if (s.isDirectory()) {
                    await search(fullPath);
                } else if (file === "Dockerfile" || file === "Containerfile" || file.endsWith(".Dockerfile")) {
                    results.push({
                        path: fullPath,
                        name: relative(startDir, fullPath)
                    });
                }
            }
        } catch (err) {
            // Silently skip unreadable directories
        }
    }

    try {
        await search(startDir);
        return results;
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
        const content = await readFile(path, 'utf8');
        return content;
    } catch (e: any) {
        throw new Error(`Failed to read file: ${e.message}`);
    }
}


