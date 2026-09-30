from __future__ import annotations
import threading
from typing import Callable
import pystray
from PIL import Image, ImageDraw

COLORS = {"active":"#2e9d55", "paused":"#d99b22", "error":"#c94343"}

def make_image(state: str) -> Image.Image:
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0)); draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((4, 4, 60, 60), radius=14, fill=COLORS.get(state, COLORS["active"]))
    draw.ellipse((21, 21, 43, 43), fill="white")
    return image

class Tray:
    def __init__(self, open_cb: Callable[[], None], settings_cb: Callable[[], None], toggle_cb: Callable[[], None], quit_cb: Callable[[], None]):
        self.icon = pystray.Icon("SentinelUtility", make_image("active"), "Sentinel Utility", menu=pystray.Menu(
            pystray.MenuItem("Apri", lambda: open_cb(), default=True),
            pystray.MenuItem("Impostazioni", lambda: settings_cb()),
            pystray.MenuItem("Pausa/Riprendi", lambda: toggle_cb()),
            pystray.MenuItem("Esci", lambda: quit_cb())))
        self.thread: threading.Thread | None = None
    def start(self) -> None:
        self.thread = threading.Thread(target=self.icon.run, name="tray", daemon=True); self.thread.start()
    def set_state(self, state: str) -> None:
        self.icon.icon = make_image(state)
    def stop(self) -> None:
        try: self.icon.stop()
        except Exception: pass
