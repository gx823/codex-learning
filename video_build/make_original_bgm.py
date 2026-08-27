from pathlib import Path

import numpy as np
import soundfile as sf


SR = 48_000
DURATION = 15.25
OUT = Path(__file__).with_name("bgm_v3.wav")
rng = np.random.default_rng(20260827)


def midi_hz(note: int) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def envelope(n: int, attack: float, release: float) -> np.ndarray:
    env = np.ones(n, dtype=np.float64)
    a = min(n, max(1, int(attack * SR)))
    r = min(n, max(1, int(release * SR)))
    env[:a] = np.linspace(0.0, 1.0, a, endpoint=False)
    env[-r:] *= np.linspace(1.0, 0.0, r)
    return env


def add_tone(track: np.ndarray, note: int, start: float, duration: float,
             volume: float, pan: float = 0.0, kind: str = "piano") -> None:
    i0 = max(0, int(start * SR))
    i1 = min(len(track), i0 + int(duration * SR))
    if i1 <= i0:
        return
    t = np.arange(i1 - i0, dtype=np.float64) / SR
    f = midi_hz(note)
    if kind == "piano":
        wave = (
            np.sin(2 * np.pi * f * t)
            + 0.38 * np.sin(2 * np.pi * 2 * f * t + 0.2)
            + 0.15 * np.sin(2 * np.pi * 3 * f * t + 0.45)
        )
        wave *= np.exp(-2.35 * t) * envelope(len(t), 0.018, min(0.55, duration * 0.5))
    elif kind == "pluck":
        wave = (
            np.sin(2 * np.pi * f * t)
            + 0.24 * np.sin(2 * np.pi * 2 * f * t)
            + 0.09 * np.sin(2 * np.pi * 4 * f * t)
        )
        wave *= np.exp(-4.2 * t) * envelope(len(t), 0.006, min(0.22, duration * 0.45))
    elif kind == "bell":
        wave = (
            np.sin(2 * np.pi * f * t)
            + 0.26 * np.sin(2 * np.pi * 2.01 * f * t)
            + 0.10 * np.sin(2 * np.pi * 3.98 * f * t)
        )
        wave *= np.exp(-1.55 * t) * envelope(len(t), 0.012, min(0.65, duration * 0.5))
    else:  # soft strings
        vibrato = 0.003 * np.sin(2 * np.pi * 5.1 * t)
        wave = (
            np.sin(2 * np.pi * f * (1 + vibrato) * t)
            + 0.31 * np.sin(2 * np.pi * 2 * f * t)
        )
        wave *= envelope(len(t), 0.55, min(0.8, duration * 0.4))
    wave *= volume
    left = np.sqrt((1.0 - pan) * 0.5)
    right = np.sqrt((1.0 + pan) * 0.5)
    track[i0:i1, 0] += wave * left
    track[i0:i1, 1] += wave * right


def main() -> None:
    music = np.zeros((int(DURATION * SR), 2), dtype=np.float64)
    beat = 60.0 / 80.0
    bar = 4 * beat

    # Original five-bar progression: Bm7 - Gmaj7 - Dadd9 - Asus4 - Dadd9.
    chords = [
        (47, [59, 62, 66, 69]),
        (43, [55, 59, 62, 66]),
        (38, [54, 57, 62, 64]),
        (45, [57, 62, 64, 69]),
        (38, [54, 57, 62, 64]),
    ]
    for bar_idx, (bass, notes) in enumerate(chords):
        start = bar_idx * bar
        add_tone(music, bass, start, bar * 0.96, 0.065, -0.08, "strings")
        for j, note in enumerate(notes):
            add_tone(music, note, start, bar * 0.96, 0.032, (j - 1.5) * 0.18, "strings")
        # Gentle acoustic-style eighth-note arpeggio.
        pattern = [0, 2, 1, 3, 0, 2, 1, 3]
        for step, tone_idx in enumerate(pattern):
            add_tone(
                music, notes[tone_idx] + 12, start + step * beat / 2,
                beat * 0.72, 0.045, -0.25 + 0.5 * (step % 2), "pluck"
            )

    # A small original lyrical melody, leaving space for dialogue.
    melody = [
        (0.00, 71, 0.75), (0.75, 74, 0.75), (1.50, 78, 1.25),
        (3.00, 74, 0.75), (3.75, 71, 0.75), (4.50, 69, 1.30),
        (6.00, 66, 0.75), (6.75, 69, 0.75), (7.50, 74, 1.30),
        (9.00, 73, 0.75), (9.75, 71, 0.75), (10.50, 69, 1.30),
        (12.00, 66, 0.75), (12.75, 69, 0.75), (13.50, 74, 1.60),
    ]
    for idx, (start, note, dur) in enumerate(melody):
        add_tone(music, note, start, dur, 0.050, -0.12 if idx % 2 else 0.12, "bell")
        add_tone(music, note - 12, start, min(dur, 1.05), 0.030, 0.05, "piano")

    # Very quiet airy ambience to soften the synthesized instruments.
    noise = rng.normal(0.0, 1.0, len(music))
    kernel = np.ones(1800, dtype=np.float64) / 1800
    air = np.convolve(noise, kernel, mode="same") * 0.010
    music[:, 0] += air
    music[:, 1] += np.roll(air, 713)

    # Fade in/out and normalize conservatively so speech remains clear.
    fade = np.ones(len(music))
    fade[: int(0.7 * SR)] = np.linspace(0.0, 1.0, int(0.7 * SR))
    fade[-int(1.25 * SR):] = np.linspace(1.0, 0.0, int(1.25 * SR))
    music *= fade[:, None]
    peak = np.max(np.abs(music))
    if peak > 0:
        music *= 0.72 / peak
    sf.write(OUT, music.astype(np.float32), SR, subtype="PCM_16")
    print(OUT)


if __name__ == "__main__":
    main()
