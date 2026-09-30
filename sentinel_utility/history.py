from __future__ import annotations
import json, threading
from datetime import datetime
from pathlib import Path
from typing import Any
from config import atomic_write

class History:
    def __init__(self, path: Path, limit: int = 200):
        self.path, self.limit, self.lock = path, limit, threading.Lock()
        try:
            data = json.loads(path.read_text(encoding="utf-8")) if path.exists() else []
            self.rows = data[-limit:] if isinstance(data, list) else []
        except (OSError, json.JSONDecodeError): self.rows = []
    def add(self, row: dict[str, Any]) -> dict[str, Any]:
        item = {"timestamp": datetime.now().astimezone().isoformat(timespec="seconds"), **row}
        with self.lock:
            self.rows.insert(0, item); self.rows = self.rows[:self.limit]
            atomic_write(self.path, json.dumps(self.rows, ensure_ascii=False, indent=2))
        return item
    def list(self) -> list[dict[str, Any]]:
        with self.lock: return list(self.rows)
