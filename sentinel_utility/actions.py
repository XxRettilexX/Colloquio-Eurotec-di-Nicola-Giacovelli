from __future__ import annotations
import os, shutil, subprocess
from pathlib import Path
from typing import Any

CREATE_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)

def move_file(source: Path, job: dict[str, Any]) -> Path:
    dest_dir = Path(os.path.expandvars(job["destination"])); dest_dir.mkdir(parents=True, exist_ok=True)
    target = dest_dir / (job.get("rename") or source.name)
    shutil.copy2(source, target)
    if not target.is_file() or target.stat().st_size != source.stat().st_size:
        raise OSError("Verifica copia MOVE non riuscita (dimensione diversa)")
    archive = job.get("archive", "")
    if archive:
        archive_dir = Path(os.path.expandvars(archive)); archive_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, archive_dir / source.name)
    source.unlink()
    return target

def printer_unc(job: dict[str, Any]) -> str:
    if job.get("unc"): return job["unc"]
    return "\\\\" + job["computer"].strip("\\") + "\\" + job["printer"].strip("\\")

def _net_use(args: list[str], timeout: int) -> None:
    result = subprocess.run(["net", "use", *args], capture_output=True, text=True, timeout=timeout, creationflags=CREATE_NO_WINDOW)
    if result.returncode: raise OSError((result.stderr or result.stdout or "net use non riuscito").strip())

def print_raw(source: Path, job: dict[str, Any], timeout: int = 30) -> None:
    unc = printer_unc(job); port = job.get("port", "").strip().rstrip(":")
    if port:
        # The delete command returns a nonzero exit code when no mapping exists;
        # that is the normal first-run case, so only the subsequent map matters.
        try: _net_use([port + ":", "/delete", "/y"], timeout)
        except (OSError, subprocess.TimeoutExpired): pass
        _net_use([port + ":", unc, "/persistent:no"], timeout)
        target = Path("\\\\.\\" + port + ":")
    else: target = Path(unc)
    try:
        with source.open("rb") as src, target.open("wb") as dst: shutil.copyfileobj(src, dst)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise OSError(f"Invio RAW a {unc} non riuscito: {exc}") from exc
    archive = job.get("archive", "")
    if archive:
        archive_dir = Path(os.path.expandvars(archive)); archive_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, archive_dir / source.name)
    source.unlink()

def process_file(source: Path, job: dict[str, Any], timeout: int = 30) -> str:
    if job["mode"] == "MOVE": return str(move_file(source, job))
    print_raw(source, job, timeout); return printer_unc(job)
