from __future__ import annotations
import json, os, tempfile
from pathlib import Path
from typing import Any

DEFAULT_CONFIG: dict[str, Any] = {
    "max_attempts": 3, "print_timeout": 30, "autostart": False,
    "jobs": [
        {"name":"Scontrino stampa","enabled":True,"watch_dir":"%UserProfile%\\Downloads","filename":"scontr00.001","sentinel":"scontr00.on","interval":3,"mode":"PRINT","destination":"","computer":"Eurotec-Master","printer":"CassaCustom","port":"LPT2","archive":""},
        {"name":"Scontrino magazzino","enabled":True,"watch_dir":"%UserProfile%\\Downloads","filename":"scontr00mag.Xml","sentinel":"scontr00mag.ok","interval":1,"mode":"MOVE","destination":"C:\\SwInstallato\\Olivetti\\ElaExecute\\EE_IN","rename":"scontrino.Xml","computer":"","printer":"","port":"","archive":""}
    ]
}

def expand_path(value: str) -> Path:
    """Expand Windows %VAR% and standard environment variables."""
    import re
    expanded = re.sub(r"%([^%]+)%", lambda m: os.environ.get(m.group(1), m.group(0)), value)
    return Path(os.path.expandvars(expanded)).expanduser()

def atomic_write(path: Path, data: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=path.name, suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except Exception:
        try: os.unlink(temp_name)
        except OSError: pass
        raise

def load_config(path: Path) -> dict[str, Any]:
    if not path.exists():
        save_config(path, DEFAULT_CONFIG)
        return json.loads(json.dumps(DEFAULT_CONFIG))
    with path.open(encoding="utf-8") as f: config = json.load(f)
    if not isinstance(config.get("jobs"), list): raise ValueError("config.json: jobs deve essere una lista")
    config.setdefault("max_attempts", 3); config.setdefault("print_timeout", 30); config.setdefault("autostart", False)
    return config

def save_config(path: Path, config: dict[str, Any]) -> None:
    validate_config(config)
    atomic_write(path, json.dumps(config, indent=2, ensure_ascii=False))

def validate_config(config: dict[str, Any]) -> None:
    if not isinstance(config.get("jobs"), list): raise ValueError("Definire l'elenco delle regole")
    if int(config.get("max_attempts", 0)) < 1: raise ValueError("Tentativi massimi deve essere almeno 1")
    for i, job in enumerate(config["jobs"], 1):
        for key in ("name", "watch_dir", "filename", "sentinel", "mode"):
            if not str(job.get(key, "")).strip(): raise ValueError(f"Regola {i}: campo {key} obbligatorio")
        if job["mode"] not in ("MOVE", "PRINT"): raise ValueError(f"Regola {i}: modalità non valida")
        if float(job.get("interval", 0)) <= 0: raise ValueError(f"Regola {i}: intervallo non valido")
        if job["mode"] == "MOVE" and not job.get("destination", "").strip(): raise ValueError(f"Regola {i}: destinazione obbligatoria")
        if job["mode"] == "PRINT" and not (job.get("unc") or (job.get("computer") and job.get("printer"))): raise ValueError(f"Regola {i}: indicare stampante UNC o computer e condivisione")

