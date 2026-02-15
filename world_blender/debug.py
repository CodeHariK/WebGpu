import time


def wlog(level: str, msg: str, *args):
    """Simple print-based logger used by the add-on.

    Signature mirrors logging usage so existing `logger.<level>(msg, *args)`
    calls can be converted to `wlog("LEVEL", msg, *args)` without changing
    formatting semantics.
    """
    try:
        if args:
            try:
                text = msg % args
            except Exception:
                # fallback: join args if percent-formatting fails
                text = f"{msg} {' '.join(str(a) for a in args)}"
        else:
            text = str(msg)
    except Exception:
        text = str(msg)

    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"{ts} {level}:{text}")


class _ShimLogger:
    def __init__(self, name: str | None = None):
        self.name = name or ""

    def _emit(self, level: str, msg: str, *args):
        if self.name:
            wlog(level, f"{self.name}: {msg}", *args)
        else:
            wlog(level, msg, *args)

    def debug(self, msg: str, *args):
        self._emit("DEBUG", msg, *args)

    def info(self, msg: str, *args):
        self._emit("INFO", msg, *args)

    def warning(self, msg: str, *args):
        self._emit("WARNING", msg, *args)

    def exception(self, msg: str, *args):
        # no traceback available here; just print level and message
        self._emit("EXCEPTION", msg, *args)

    def error(self, msg: str, *args):
        self._emit("ERROR", msg, *args)


def getLogger(name: str | None = None) -> _ShimLogger:
    return _ShimLogger(name)


logger = getLogger("world_blender")
