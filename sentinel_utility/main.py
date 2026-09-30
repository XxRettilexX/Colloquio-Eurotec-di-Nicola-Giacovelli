from __future__ import annotations
import ctypes, logging, logging.handlers, os, queue, sys, tkinter as tk
from pathlib import Path
from tkinter import messagebox
from config import load_config
from history import History
from watcher import Watcher
from gui import MainWindow
from tray import Tray

def app_dir() -> Path:
    return Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent

def acquire_mutex() -> bool:
    if os.name != "nt": return True
    handle = ctypes.windll.kernel32.CreateMutexW(None, True, "Local\\SentinelUtility.SingleInstance")
    if not handle: return True
    if ctypes.windll.kernel32.GetLastError() == 183:
        ctypes.windll.user32.MessageBoxW(None, "Sentinel Utility è già in esecuzione.", "Sentinel Utility", 0x40); return False
    return True

def autostart(enabled: bool) -> None:
    if os.name != "nt": return
    import winreg
    key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Microsoft\Windows\CurrentVersion\Run", 0, winreg.KEY_SET_VALUE)
    try:
        if enabled: winreg.SetValueEx(key,"SentinelUtility",0,winreg.REG_SZ,f'"{sys.executable}"' + (f' "{Path(__file__).resolve()}"' if not getattr(sys,"frozen",False) else ""))
        else:
            try: winreg.DeleteValue(key,"SentinelUtility")
            except FileNotFoundError: pass
    finally: winreg.CloseKey(key)

def main() -> None:
    if not acquire_mutex(): return
    base=app_dir(); logger=logging.getLogger("sentinel"); logger.setLevel(logging.INFO)
    handler=logging.handlers.RotatingFileHandler(base/"sentinel.log",maxBytes=1_000_000,backupCount=3,encoding="utf-8"); handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s")); logger.addHandler(handler)
    try: config=load_config(base/"config.json")
    except Exception as exc:
        root=tk.Tk(); root.withdraw(); messagebox.showerror("Configurazione",str(exc)); root.destroy(); return
    try: autostart(bool(config.get("autostart",False)))
    except OSError: logger.exception("Aggiornamento avvio automatico non riuscito")
    history=History(base/"history.json"); events=queue.Queue(); watcher=Watcher(config,history,events,logger)
    root=tk.Tk(); root.withdraw(); holder={}
    def quit_app():
        watcher.stop(); holder["tray"].stop(); root.destroy()
    def set_tray_state(state: str) -> None:
        if "tray" in holder: holder["tray"].set_state(state)
    window=MainWindow(root,config,base/"config.json",history,watcher,events,lambda:holder["window"].edit_settings(),quit_app,autostart,set_tray_state); holder["window"]=window
    tray=Tray(window.show,window.edit_settings,window.toggle,quit_app); holder["tray"]=tray
    tray.start(); watcher.start()
    try: root.mainloop()
    finally: watcher.stop(); tray.stop()

if __name__ == "__main__": main()
