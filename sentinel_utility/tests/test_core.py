from __future__ import annotations
import json, logging, queue
from pathlib import Path
from unittest.mock import patch
from actions import move_file, print_raw
from config import load_config, save_config
from history import History
from watcher import Watcher

def job(src: Path, dst: Path) -> dict:
    return {"name":"test","enabled":True,"watch_dir":str(src),"filename":"data.bin","sentinel":"ready.ok","interval":1,"mode":"MOVE","destination":str(dst),"rename":"renamed.bin","archive":""}

def test_config_defaults_and_atomic_save(tmp_path):
    path=tmp_path/"config.json"; config=load_config(path)
    assert path.exists() and len(config["jobs"]) == 2
    config["max_attempts"]=4; save_config(path,config)
    assert json.loads(path.read_text())["max_attempts"] == 4

def test_sentinel_and_move_rename(tmp_path):
    src=tmp_path/"in"; dst=tmp_path/"out"; src.mkdir(); file=src/"data.bin"; file.write_bytes(b"payload")
    job_cfg=job(src,dst); queue_events=queue.Queue(); watcher=Watcher({"jobs":[job_cfg],"max_attempts":3},History(tmp_path/"history.json"),queue_events,logging.getLogger("test"))
    watcher._check(job_cfg); assert file.exists()
    (src/"ready.ok").touch(); watcher._check(job_cfg)
    assert not file.exists() and not (src/"ready.ok").exists() and (dst/"renamed.bin").read_bytes()==b"payload"

def test_retry_limit_removes_only_sentinel(tmp_path):
    src=tmp_path/"in"; src.mkdir(); file=src/"data.bin"; file.write_bytes(b"x"); (src/"ready.ok").touch()
    cfg=job(src,tmp_path/"out"); history=History(tmp_path/"history.json"); watcher=Watcher({"jobs":[cfg],"max_attempts":2},history,queue.Queue(),logging.getLogger("test"))
    with patch("watcher.process_file",side_effect=OSError("copy failed")):
        watcher._check(cfg); watcher._check(cfg)
    assert file.exists() and not (src/"ready.ok").exists()
    assert [r["status"] for r in history.list()] == ["Fallito","Errore"]

def test_print_mock(tmp_path):
    source=tmp_path/"print.bin"; source.write_bytes(b"raw bytes")
    cfg={"computer":"server","printer":"queue","port":"","archive":""}
    with patch("actions.printer_unc",return_value=str(tmp_path/"device")):
        with patch("actions.Path.open") as open_mock:
            import io
            sink=io.BytesIO(); open_mock.side_effect=[io.BytesIO(b"raw bytes"),sink]
            print_raw(source,cfg)
    assert not source.exists() and sink.getvalue()==b"raw bytes"
