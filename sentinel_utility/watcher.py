from __future__ import annotations
import logging, queue, threading, time
from pathlib import Path
import shutil, tempfile
from typing import Any, Callable
from actions import process_file
from config import expand_path
from history import History

def verify_exclusive_access(path: Path) -> None:
    """Try opening the file read/write with Windows sharing disabled."""
    if __import__("os").name != "nt":
        with path.open("r+b"): return
    import ctypes
    from ctypes import wintypes
    create_file = ctypes.windll.kernel32.CreateFileW
    create_file.restype = wintypes.HANDLE
    handle = create_file(str(path), 0x80000000 | 0x40000000, 0, None, 3, 0x80, None)
    invalid = ctypes.c_void_p(-1).value
    if handle == invalid: raise OSError(ctypes.get_last_error(), "File non disponibile in accesso esclusivo", str(path))
    ctypes.windll.kernel32.CloseHandle(handle)

class Watcher:
    def __init__(self, config: dict[str, Any], history: History, events: queue.Queue, logger: logging.Logger):
        self.config, self.history, self.events, self.logger = config, history, events, logger
        self.stop_event = threading.Event(); self.paused = threading.Event(); self.thread: threading.Thread | None = None
        self.attempts: dict[tuple[str, str], int] = {}; self.last_check = "mai"
    def start(self) -> None:
        if not self.thread or not self.thread.is_alive():
            self.thread = threading.Thread(target=self._run, name="folder-watcher", daemon=True); self.thread.start()
    def stop(self) -> None: self.stop_event.set()
    def set_paused(self, value: bool) -> None: self.paused.set() if value else self.paused.clear()
    def reload(self, config: dict[str, Any]) -> None: self.config = config
    def repeat(self, row: dict[str, Any]) -> None:
        threading.Thread(target=self._repeat, args=(row,), daemon=True).start()
    def _repeat(self, row: dict[str, Any]) -> None:
        job = next((j for j in self.config["jobs"] if j["name"] == row.get("job")), None)
        if not job: self._record(row.get("job", ""), row.get("filename", ""), row.get("mode", ""), "Errore", "Regola non trovata"); return
        archive = job.get("archive", "")
        archived = (expand_path(archive) / row["filename"]) if archive else None
        if archived is not None and archived.is_file():
            # Work on a temporary copy so a repeat never consumes the archive.
            with tempfile.TemporaryDirectory(prefix="sentinel-repeat-") as temp:
                source = Path(temp) / row["filename"]; shutil.copy2(archived, source)
                self._process(job, source, row.get("sentinel", ""), retry=True)
        else:
            source = expand_path(job["watch_dir"]) / row["filename"]
            self._process(job, source, row.get("sentinel", ""), retry=True)
    def _run(self) -> None:
        due: dict[str, float] = {}
        while not self.stop_event.is_set():
            if self.paused.wait(0.25): continue
            jobs = [j for j in self.config.get("jobs", []) if j.get("enabled", True)]
            if not jobs: self.stop_event.wait(0.5); continue
            now = time.monotonic()
            for job in jobs:
                if self.stop_event.is_set() or self.paused.is_set(): break
                if now >= due.get(job["name"], 0):
                    self._check(job); due[job["name"]] = now + float(job.get("interval", 3))
            self.last_check = time.strftime("%H:%M:%S")
            self.events.put({"type":"status", "last_check":self.last_check})
            self.stop_event.wait(0.25)
    def _check(self, job: dict[str, Any]) -> None:
        folder = expand_path(job["watch_dir"]); file = folder / job["filename"]; sentinel = folder / job["sentinel"]
        if not folder.is_dir():
            self.events.put({"type":"error", "message":f"Cartella non raggiungibile: {folder}"}); return
        if not (sentinel.is_file() and file.is_file()): return
        try: verify_exclusive_access(file)
        except (PermissionError, OSError): return
        self._process(job, file, job["sentinel"])
    def _process(self, job: dict[str, Any], file: Path, sentinel_name: str, retry: bool = False) -> None:
        key = (job["name"], file.name); count = 0 if retry else self.attempts.get(key, 0)
        try:
            outcome = process_file(file, job, int(self.config.get("print_timeout", 30)))
            if not retry:
                (expand_path(job["watch_dir"]) / sentinel_name).unlink(missing_ok=True)
            self.attempts.pop(key, None)
            self._record(job["name"], file.name, job["mode"], "OK", outcome)
        except Exception as exc:
            self.logger.exception("Elaborazione fallita per %s", file)
            count += 1
            status = "Fallito" if count >= int(self.config.get("max_attempts", 3)) else "Errore"
            if retry: status = "Errore"
            else: self.attempts[key] = count
            self._record(job["name"], file.name, job["mode"], status, str(exc))
            if status == "Fallito":
                (expand_path(job["watch_dir"]) / sentinel_name).unlink(missing_ok=True); self.attempts.pop(key, None)
    def _record(self, job: str, filename: str, mode: str, status: str, message: str) -> None:
        row = self.history.add({"job":job,"filename":filename,"mode":mode,"status":status,"message":message})
        self.events.put({"type":"history", "row":row})
