import { createContext, useContext, useState, useEffect, useCallback } from "react";
import type { ReactNode } from "react";
import { Toast } from "@base-ui/react/toast";
import type { CommandLog } from "../lib/container";
import { getActionLogs, clearActionLogs as apiClearActionLogs } from "../lib/container";

interface CommandLogContextType {
    logs: CommandLog[];
    clearLogs: () => Promise<void>;
    refreshLogs: () => Promise<void>;
}

const CommandLogContext = createContext<CommandLogContextType | undefined>(undefined);

const toastManager = Toast.createToastManager();

export function CommandLogProvider({ children }: { children: ReactNode }) {
    const [logs, setLogs] = useState<CommandLog[]>([]);

    const refreshLogs = useCallback(async () => {
        try {
            const logList = await getActionLogs();
            setLogs(logList);
        } catch (e) {
            console.error("Failed to fetch logs:", e);
        }
    }, []);

    const clearLogs = useCallback(async () => {
        try {
            await apiClearActionLogs();
            setLogs([]);
        } catch (e) {
            console.error("Failed to clear logs:", e);
        }
    }, []);

    useEffect(() => {
        refreshLogs();

        const eventSource = new EventSource("/api/logs/stream");

        eventSource.onmessage = (event) => {
            try {
                const logEntry: CommandLog = JSON.parse(event.data);

                setLogs(prev => {
                    // If it's partial, we might want to update the latest log if it's the same command
                    // But for now, just prepending is simpler as the backend sends lines.
                    // However, to avoid spamming the log list with partials, we could consolidate.
                    if (logEntry.isPartial && prev.length > 0 && prev[0].command === logEntry.command && prev[0].isPartial) {
                        const newLogs = [...prev];
                        newLogs[0] = {
                            ...newLogs[0],
                            output: newLogs[0].output + "\n" + logEntry.output
                        };
                        return newLogs;
                    }
                    return [logEntry, ...prev.slice(0, 99)];
                });

                if (!logEntry.isPartial) {
                    toastManager.add({
                        title: logEntry.isError ? "Command Failed" : "Command Successful",
                        description: logEntry.command,
                        type: logEntry.isError ? "error" : "success"
                    });
                    // Notify other components that logs or system state might have changed
                    // We can use a custom event or just let them poll
                    window.dispatchEvent(new CustomEvent('cider:refresh'));
                }
            } catch (e) {
                console.error("Failed to parse SSE message", e);
            }
        };

        eventSource.onerror = (err) => {
            console.error("EventSource failed:", err);
            eventSource.close();
        };

        return () => {
            eventSource.close();
        };
    }, [refreshLogs]);

    return (
        <Toast.Provider toastManager={toastManager}>
            <CommandLogContext.Provider value={{ logs, clearLogs, refreshLogs }}>
                {children}
            </CommandLogContext.Provider>
        </Toast.Provider>
    );
}

export function useCommandLog() {
    const context = useContext(CommandLogContext);
    if (context === undefined) {
        throw new Error("useCommandLog must be used within a CommandLogProvider");
    }
    return context;
}

export { toastManager };
