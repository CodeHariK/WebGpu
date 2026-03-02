import { useState, useEffect, useRef } from "react";
import { Dialog } from "@base-ui/react/dialog";
import { useCommandLog } from "../contexts/CommandLogContext";
import { X, Trash2, Terminal, ChevronRight } from "lucide-react";

export default function CommandLogOverlay() {
    const { logs, clearLogs } = useCommandLog();
    const [open, setOpen] = useState(false);
    const scrollRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.key === "`") {
                e.preventDefault();
                setOpen(prev => !prev);
            }
            if (e.key === "Escape" && open) {
                setOpen(false);
            }
        };
        window.addEventListener("keydown", handleKeyDown);
        return () => window.removeEventListener("keydown", handleKeyDown);
    }, [open]);

    useEffect(() => {
        if (scrollRef.current) {
            scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
        }
    }, [logs]);

    return (
        <Dialog.Root open={open} onOpenChange={setOpen}>
            <Dialog.Portal>
                <Dialog.Backdrop className="dialog-backdrop" style={{ backdropFilter: 'blur(4px)', backgroundColor: 'rgba(0,0,0,0.4)' }} />
                <Dialog.Popup className="premium-card animate-slide-up" style={{
                    position: 'fixed',
                    bottom: '24px',
                    right: '24px',
                    width: '600px',
                    height: '500px',
                    maxWidth: '90vw',
                    maxHeight: '80vh',
                    display: 'flex',
                    flexDirection: 'column',
                    overflow: 'hidden',
                    padding: 0,
                    zIndex: 1000,
                    boxShadow: '0 20px 50px rgba(0,0,0,0.5), 0 0 0 1px rgba(255,255,255,0.1)',
                    background: 'rgba(15, 15, 15, 0.9)',
                    backdropFilter: 'blur(20px)'
                }}>
                    <div className="flex-between" style={{ padding: '16px 20px', borderBottom: '1px solid rgba(255,255,255,0.1)' }}>
                        <div className="flex-center" style={{ gap: '10px' }}>
                            <Terminal size={18} className="text-secondary" />
                            <h2 style={{ fontSize: '16px', fontWeight: 600, margin: 0 }}>Command Console</h2>
                            <span className="badge" style={{ fontSize: '10px', background: 'rgba(255,255,255,0.1)', padding: '2px 8px' }}>Press ` to toggle</span>
                        </div>
                        <div className="flex-center" style={{ gap: '12px' }}>
                            <button className="btn btn-ghost btn-small" onClick={clearLogs} title="Clear Logs">
                                <Trash2 size={16} />
                            </button>
                            <Dialog.Close className="btn-icon" style={{ padding: '4px' }}>
                                <X size={20} />
                            </Dialog.Close>
                        </div>
                    </div>

                    <div
                        ref={scrollRef}
                        className="custom-scrollbar"
                        style={{
                            flex: 1,
                            overflowY: 'auto',
                            padding: '20px',
                            fontFamily: '"Fira Code", monospace',
                            fontSize: '13px',
                            backgroundColor: 'rgba(0,0,0,0.2)'
                        }}
                    >
                        {logs.length === 0 ? (
                            <div className="flex-center" style={{ height: '100%', flexDirection: 'column', opacity: 0.4 }}>
                                <Terminal size={48} style={{ marginBottom: '16px' }} />
                                <p>No commands executed yet.</p>
                            </div>
                        ) : (
                            <div style={{ display: 'flex', flexDirection: 'column-reverse', gap: '20px' }}>
                                {logs.map((log) => (
                                    <div key={log.id} style={{
                                        padding: '12px',
                                        borderRadius: '8px',
                                        background: log.isError ? 'rgba(255, 59, 48, 0.05)' : 'rgba(255, 255, 255, 0.02)',
                                        borderLeft: `3px solid ${log.isError ? '#ff3b30' : log.isPartial ? '#ffcc00' : '#34c759'}`
                                    }}>
                                        <div className="flex-between" style={{ marginBottom: '8px' }}>
                                            <div className="flex-center" style={{ gap: '8px', color: log.isError ? '#ff3b30' : 'var(--accent-primary)', fontWeight: 600 }}>
                                                <ChevronRight size={14} />
                                                <span style={{ fontSize: '12px' }}>{log.command}</span>
                                            </div>
                                            <span style={{ fontSize: '10px', opacity: 0.5 }}>
                                                {new Date(log.timestamp).toLocaleTimeString()}
                                            </span>
                                        </div>
                                        <div style={{
                                            color: 'var(--text-primary)',
                                            whiteSpace: 'pre-wrap',
                                            fontSize: '12px',
                                            lineHeight: '1.5',
                                            opacity: 0.9
                                        }}>
                                            {log.output || (log.isPartial ? "Running..." : "No output")}
                                        </div>
                                    </div>
                                ))}
                            </div>
                        )}
                    </div>
                </Dialog.Popup>
            </Dialog.Portal>
        </Dialog.Root>
    );
}
