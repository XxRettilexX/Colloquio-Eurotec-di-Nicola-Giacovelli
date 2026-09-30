from __future__ import annotations
import copy, queue, tkinter as tk
from tkinter import ttk, messagebox, filedialog
from typing import Any
from config import save_config, validate_config
from history import History
from watcher import Watcher

FIELDS = [("name","Nome"),("enabled","Attiva"),("watch_dir","Cartella monitorata"),("filename","File"),("sentinel","Sentinella"),("interval","Intervallo (s)"),("mode","Modalità"),("destination","Destinazione MOVE"),("rename","Rinomina"),("computer","Computer"),("printer","Stampante condivisa"),("port","Porta (es. LPT2)"),("unc","Percorso UNC"),("archive","Archivio")]

class RuleDialog(tk.Toplevel):
    def __init__(self, parent: tk.Misc, job: dict[str, Any] | None):
        super().__init__(parent); self.title("Regola"); self.resizable(False, True); self.result = None; self.vars = {}
        for row, (key, label) in enumerate(FIELDS):
            ttk.Label(self, text=label).grid(row=row, column=0, sticky="w", padx=6, pady=3)
            value = (job or {}).get(key, True if key == "enabled" else "MOVE" if key == "mode" else "")
            var = tk.BooleanVar(value=value) if key == "enabled" else tk.StringVar(value=str(value))
            self.vars[key] = var
            if key == "enabled": widget = ttk.Checkbutton(self, variable=var)
            elif key == "mode": widget = ttk.Combobox(self, textvariable=var, values=["MOVE","PRINT"], state="readonly")
            else: widget = ttk.Entry(self, textvariable=var, width=44)
            widget.grid(row=row, column=1, sticky="ew", padx=6, pady=3)
            if key in ("watch_dir","destination","archive"):
                ttk.Button(self, text="Sfoglia", command=lambda k=key: self._browse(k)).grid(row=row, column=2, padx=4)
        bar = ttk.Frame(self); bar.grid(row=len(FIELDS), column=0, columnspan=3, sticky="e", padx=6, pady=8)
        ttk.Button(bar, text="Salva", command=self._save).pack(side="left", padx=4); ttk.Button(bar, text="Annulla", command=self.destroy).pack(side="left")
        self.transient(parent); self.grab_set(); self.wait_visibility(); self.focus_set()
    def _browse(self, key: str) -> None:
        path = filedialog.askdirectory(parent=self)
        if path: self.vars[key].set(path)
    def _save(self) -> None:
        self.result = {k:v.get() for k,v in self.vars.items()}
        try:
            self.result["interval"] = float(self.result["interval"])
            candidate = {"jobs":[self.result], "max_attempts":1}; validate_config(candidate)
        except (ValueError, TypeError) as exc: messagebox.showerror("Regola non valida", str(exc), parent=self); return
        self.destroy()

class MainWindow:
    def __init__(self, root: tk.Tk, config: dict[str, Any], config_path, history: History, watcher: Watcher, events: queue.Queue, on_settings, on_quit, on_autostart, on_state):
        self.root, self.config, self.config_path, self.history, self.watcher, self.events = root, config, config_path, history, watcher, events
        self.on_settings, self.on_quit, self.on_autostart, self.on_state = on_settings, on_quit, on_autostart, on_state; self.paused = False; self.ids: dict[str, dict] = {}
        root.title("Sentinel Utility"); root.geometry("880x420"); root.protocol("WM_DELETE_WINDOW", self.hide)
        top = ttk.Frame(root); top.pack(fill="x", padx=8, pady=6)
        ttk.Button(top, text="Impostazioni", command=on_settings).pack(side="left")
        ttk.Button(top, text="Ripeti", command=self.repeat).pack(side="left", padx=6)
        ttk.Button(top, text="Pausa / Riprendi", command=self.toggle).pack(side="left")
        ttk.Button(top, text="Esci", command=on_quit).pack(side="right")
        columns = ("timestamp","job","filename","mode","status","message")
        self.tree = ttk.Treeview(root, columns=columns, show="headings", height=15)
        for col, label, width in zip(columns,["Data e ora","Regola","File","Modalità","Esito","Messaggio"],[150,145,150,75,80,260]):
            self.tree.heading(col, text=label); self.tree.column(col, width=width, anchor="w")
        self.tree.tag_configure("OK", foreground="#16783a"); self.tree.tag_configure("Errore", foreground="#b66a00"); self.tree.tag_configure("Fallito", foreground="#bd2525")
        self.tree.pack(fill="both", expand=True, padx=8); self.status = ttk.Label(root, text="Attivo — ultimo controllo: in attesa", anchor="w"); self.status.pack(fill="x", padx=8, pady=6)
        for row in history.list(): self._add(row)
        root.after(150, self._poll)
    def hide(self) -> None: self.root.withdraw()
    def show(self) -> None: self.root.deiconify(); self.root.lift(); self.root.focus_force()
    def toggle(self) -> None:
        self.paused = not self.paused; self.watcher.set_paused(self.paused); self.status.configure(text="In pausa" if self.paused else "Attivo"); self.on_state("paused" if self.paused else "active")
    def _add(self, row: dict[str, Any]) -> None:
        iid = self.tree.insert("", 0, values=(row.get("timestamp",""),row.get("job",""),row.get("filename",""),row.get("mode",""),row.get("status",""),row.get("message","")), tags=(row.get("status",""),))
        self.ids[iid] = row
        children = self.tree.get_children()
        for old in children[200:]: self.tree.delete(old); self.ids.pop(old, None)
    def _poll(self) -> None:
        try:
            while True:
                event = self.events.get_nowait()
                if event["type"] == "history": self._add(event["row"])
                elif event["type"] == "error": self.status.configure(text="Errore: " + event["message"]); self.on_state("error")
                else:
                    self.status.configure(text=f"{'In pausa' if self.paused else 'Attivo'} — ultimo controllo: {event.get('last_check','')}")
                    if not self.paused: self.on_state("active")
        except queue.Empty: pass
        self.root.after(150, self._poll)
    def repeat(self) -> None:
        selection = self.tree.selection()
        if not selection: messagebox.showinfo("Ripeti", "Seleziona una riga dello storico."); return
        row = self.ids.get(selection[0])
        if row and messagebox.askyesno("Ripeti elaborazione", f"Ripetere {row.get('filename')}?", parent=self.root): self.watcher.repeat(row)
    def edit_settings(self) -> None:
        win = tk.Toplevel(self.root); win.title("Impostazioni — regole"); win.geometry("650x380")
        listing = tk.Listbox(win, height=12); listing.pack(fill="both", expand=True, padx=8, pady=8)
        def refresh():
            listing.delete(0,"end")
            for j in self.config["jobs"]: listing.insert("end", f"{'✓' if j.get('enabled') else '–'}  {j['name']} — {j['mode']} — {j['filename']}")
        def chosen():
            sel = listing.curselection()
            return sel[0] if sel else None
        def edit(index):
            dialog = RuleDialog(win, self.config["jobs"][index] if index is not None else None); win.wait_window(dialog)
            if dialog.result is not None:
                if index is None: self.config["jobs"].append(dialog.result)
                else: self.config["jobs"][index] = dialog.result
                refresh()
        globals_frame=ttk.Frame(win); globals_frame.pack(fill="x", padx=8)
        attempts=tk.StringVar(value=str(self.config.get("max_attempts",3))); timeout=tk.StringVar(value=str(self.config.get("print_timeout",30)))
        autostart_var=tk.BooleanVar(value=bool(self.config.get("autostart",False)))
        ttk.Label(globals_frame,text="Tentativi massimi").pack(side="left"); ttk.Entry(globals_frame,textvariable=attempts,width=5).pack(side="left",padx=4)
        ttk.Label(globals_frame,text="Timeout stampa (s)").pack(side="left",padx=(12,0)); ttk.Entry(globals_frame,textvariable=timeout,width=5).pack(side="left",padx=4)
        ttk.Checkbutton(globals_frame,text="Avvia con Windows",variable=autostart_var).pack(side="left",padx=12)
        buttons = ttk.Frame(win); buttons.pack(fill="x", padx=8, pady=8)
        ttk.Button(buttons,text="Aggiungi",command=lambda:edit(None)).pack(side="left")
        ttk.Button(buttons,text="Modifica",command=lambda:edit(chosen()) if chosen() is not None else None).pack(side="left")
        ttk.Button(buttons,text="Duplica",command=lambda:self._duplicate(chosen(),refresh) if chosen() is not None else None).pack(side="left")
        ttk.Button(buttons,text="Elimina",command=lambda:self._delete(chosen(),refresh) if chosen() is not None else None).pack(side="left")
        ttk.Button(buttons,text="Salva",command=lambda:self._apply(win,attempts,timeout,autostart_var)).pack(side="right"); refresh()
    def _duplicate(self, i, refresh):
        row=copy.deepcopy(self.config["jobs"][i]); row["name"] += " (copia)"; self.config["jobs"].insert(i+1,row); refresh()
    def _delete(self, i, refresh):
        if messagebox.askyesno("Elimina", "Eliminare la regola selezionata?"): self.config["jobs"].pop(i); refresh()
    def _apply(self, win, attempts, timeout, autostart_var):
        try:
            self.config["max_attempts"]=int(attempts.get()); self.config["print_timeout"]=int(timeout.get()); self.config["autostart"]=bool(autostart_var.get())
            validate_config(self.config); save_config(self.config_path,self.config); self.watcher.reload(self.config); self.on_autostart(self.config["autostart"]); win.destroy()
        except (ValueError,OSError) as exc: messagebox.showerror("Impostazioni",str(exc),parent=win)
