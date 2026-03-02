import type { ContainerConfig } from "./container";

/**
 * Constructs a 'container run' command string from a ContainerConfig object.
 */
export function buildRunCommand(config: ContainerConfig): string {
    const parts = ["container", "run", "-d"];

    // Resource Limits
    if (config.cpus) {
        parts.push("--cpus", config.cpus.toString());
    }
    if (config.memory) {
        parts.push("--memory", config.memory);
    }

    // Network
    if (config.network) {
        parts.push("--network", config.network);
    }

    // Environment Variables
    if (config.env && config.env.length > 0) {
        config.env.forEach(e => {
            if (e) parts.push("-e", `"${e}"`);
        });
    }

    // Ports
    if (config.ports && config.ports.length > 0) {
        config.ports.forEach(p => {
            if (p) parts.push("-p", p);
        });
    }

    // Volumes
    if (config.volumes && config.volumes.length > 0) {
        config.volumes.forEach(v => {
            if (v) parts.push("-v", `"${v}"`);
        });
    }

    // Name (if available in config, though currently not explicitly in the state)
    // if (config.name) {
    //     parts.push("--name", config.name);
    // }

    // Image
    parts.push(config.image);

    // Command Override
    if (config.command) {
        parts.push(config.command);
    }

    return parts.join(" ");
}
/**
 * Constructs a 'container create' command string from a ContainerConfig object.
 */
export function buildCreateCommand(config: ContainerConfig): string {
    const parts = ["container", "create"];

    // Resource Limits
    if (config.cpus) {
        parts.push("--cpus", config.cpus.toString());
    }
    if (config.memory) {
        parts.push("--memory", config.memory);
    }

    // Network
    if (config.network) {
        parts.push("--network", config.network);
    }

    // Environment Variables
    if (config.env && config.env.length > 0) {
        config.env.forEach(e => {
            if (e) parts.push("-e", `"${e}"`);
        });
    }

    // Ports
    if (config.ports && config.ports.length > 0) {
        config.ports.forEach(p => {
            if (p) parts.push("-p", p);
        });
    }

    // Volumes
    if (config.volumes && config.volumes.length > 0) {
        config.volumes.forEach(v => {
            if (v) parts.push("-v", `"${v}"`);
        });
    }

    // Image
    parts.push(config.image);

    // Command Override
    if (config.command) {
        parts.push(config.command);
    }

    return parts.join(" ");
}

export function buildPullCommand(reference: string): string {
    return `container image pull ${reference}`;
}

export function buildRemoveImageCommand(reference: string): string {
    return `container image rm --force ${reference}`;
}

export function buildSaveImageCommand(name: string, path: string): string {
    return `container image save --output "${path}" "${name}"`;
}

export function buildLoadImageCommand(path: string): string {
    return `container image load --input "${path}"`;
}

export function buildTagImageCommand(name: string, tag: string): string {
    return `container image tag "${name}" "${tag}"`;
}

export function buildVolumeCreateCommand(name: string): string {
    return `container volume create ${name}`;
}

export function buildNetworkCreateCommand(name: string): string {
    return `container network create ${name}`;
}

export function buildDnsCreateCommand(domain: string): string {
    return `container dns create ${domain}`;
}

export function buildBuilderStartCommand(cpus?: number, memory?: string): string {
    const parts = ["container", "builder", "start"];
    if (cpus) parts.push("--cpus", cpus.toString());
    if (memory) parts.push("--memory", memory);
    return parts.join(" ");
}

export function buildSetSystemPropertyCommand(id: string, value: string): string {
    return `container system property set "${id}" "${value}"`;
}

export function buildClearSystemPropertyCommand(id: string): string {
    return `container system property clear "${id}"`;
}

export function buildRegistryLogoutCommand(server: string): string {
    return `container registry logout "${server}"`;
}
