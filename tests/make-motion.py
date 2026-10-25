#!/usr/bin/env python3
"""Real physics and rendering, with automated steering; not a device capture."""
import os
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="doodle-hop-motion-") as temp:
    out = Path(temp)
    binary = out / "motion"
    subprocess.run([os.environ.get("HOST_CXX", "c++"), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root / "include"), str(root / "tests/motion.cpp"),
                    str(root / "source/game.cpp"), str(root / "source/render.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(out)], check=True)
    # One shared palette avoids per-frame colour flicker in the preview.
    palette_sheet = Image.new("RGB", (400, 240 * 12))
    for row, i in enumerate(range(0, 240, 20)):
        with Image.open(out / f"{i}.ppm") as frame:
            palette_sheet.paste(frame, (0, row * 240))
    palette = palette_sheet.quantize(colors=256)
    frames = []
    for i in range(240):
        with Image.open(out / f"{i}.ppm") as frame:
            frames.append(frame.quantize(palette=palette, dither=Image.Dither.NONE))
    frames[0].save(root / "motion-preview.gif", save_all=True, append_images=frames[1:],
                   duration=[30, 30, 40] * 80, loop=0, optimize=False, disposal=2)
print("Saved motion-preview.gif (8 seconds, automated steering, actual game physics).")
