#!/usr/bin/env python3
"""Generate sounds/switch.wav — the track-change blip for the music app.

Pure synthesis (no assets, no deps): 0.14s two-step blip (B5 -> E6) with an
exponential fade-out, 22050 Hz 16-bit mono. The wav is checked in so builds
don't need Python; re-run after changing parameters.
"""
import math
import struct
import wave
from pathlib import Path

RATE = 22050
DUR = 0.14         # seconds
AMP = 0.5          # peak amplitude (0..1)

out = Path(__file__).parent / "sounds" / "switch.wav"
out.parent.mkdir(parents=True, exist_ok=True)

n = int(RATE * DUR)
frames = bytearray()
phase = 0.0
for i in range(n):
    t = i / RATE
    freq = 987.77 if t < DUR / 2 else 1318.51   # B5 then E6
    phase += 2 * math.pi * freq / RATE
    env = math.exp(-5.0 * t / DUR)              # exponential fade-out
    attack = min(1.0, i / (0.004 * RATE))       # 4ms attack, no click
    v = AMP * env * attack * math.sin(phase)
    frames += struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767))

with wave.open(str(out), "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(bytes(frames))
print(f"wrote {out} ({n} samples, {DUR}s @ {RATE}Hz)")
