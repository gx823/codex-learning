from __future__ import annotations

import asyncio
import math
import os
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont


WIDTH, HEIGHT = 1920, 1080
FPS = 30
DURATION = 28.0
ROOT = Path(__file__).resolve().parent
DEPS = ROOT / "deps"
KEYFRAMES = ROOT / "action_keyframes.png"
MUSIC = ROOT / "original_ambient.wav"
VOICE_DIR = ROOT / "voices_v2"
MIXED_AUDIO = ROOT / "mixed_audio_v2.m4a"

sys.path.insert(0, str(DEPS))
import edge_tts  # type: ignore  # noqa: E402
import imageio_ffmpeg  # type: ignore  # noqa: E402

from make_confession_short import make_music  # noqa: E402


VOICE_LINES = [
    ("miku_1", "優希也。今日は、『愛してる』って言わないで。", "ja-JP-NanamiNeural", "+12Hz", "-6%", 1.80),
    ("miku_2", "ゲームじゃなくて、ちゃんと聞いて。", "ja-JP-NanamiNeural", "+12Hz", "-6%", 7.45),
    ("miku_3", "私、優希也が好き。", "ja-JP-NanamiNeural", "+12Hz", "-8%", 11.20),
    ("yukiya_1", "じゃあ、俺もゲームをやめる。", "ja-JP-KeitaNeural", "-4Hz", "-8%", 14.70),
    ("yukiya_2", "みくが好きだ。ずっと、本気だった。", "ja-JP-KeitaNeural", "-4Hz", "-8%", 18.15),
    ("miku_4", "やっと、同じ気持ちだね。", "ja-JP-NanamiNeural", "+10Hz", "-10%", 23.05),
]


def versioned_output(folder: Path, stem: str, suffix: str) -> Path:
    candidate = folder / f"{stem}{suffix}"
    if not candidate.exists():
        return candidate
    index = 2
    while True:
        candidate = folder / f"{stem}_v{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1


def desktop_folder() -> Path:
    desktop = Path(os.environ["USERPROFILE"]) / "Desktop"
    desktop.mkdir(parents=True, exist_ok=True)
    return desktop


async def generate_voices() -> None:
    VOICE_DIR.mkdir(parents=True, exist_ok=True)
    for name, text, voice, pitch, rate, _ in VOICE_LINES:
        destination = VOICE_DIR / f"{name}.mp3"
        if destination.exists() and destination.stat().st_size > 1024:
            continue
        communicator = edge_tts.Communicate(
            text=text,
            voice=voice,
            pitch=pitch,
            rate=rate,
            volume="+0%",
        )
        await communicator.save(str(destination))


def mix_audio(ffmpeg: Path) -> None:
    make_music(MUSIC)
    command = [str(ffmpeg), "-y", "-stream_loop", "-1", "-i", str(MUSIC)]
    for name, *_ in VOICE_LINES:
        command += ["-i", str(VOICE_DIR / f"{name}.mp3")]

    filters = ["[0:a]volume=0.26[bg]"]
    labels = ["[bg]"]
    for input_index, (_, _, _, _, _, start) in enumerate(VOICE_LINES, start=1):
        delay = int(start * 1000)
        label = f"v{input_index}"
        filters.append(f"[{input_index}:a]adelay={delay}|{delay},volume=1.18[{label}]")
        labels.append(f"[{label}]")
    filters.append(
        "".join(labels)
        + f"amix=inputs={len(labels)}:duration=longest:normalize=0,"
          "alimiter=limit=0.94:attack=5:release=80[a]"
    )
    command += [
        "-filter_complex", ";".join(filters),
        "-map", "[a]", "-t", str(DURATION),
        "-c:a", "aac", "-b:a", "224k", str(MIXED_AUDIO),
    ]
    subprocess.run(command, check=True)


def crop_keyframes(sheet: Image.Image) -> list[Image.Image]:
    width, height = sheet.size
    cell_w = width / 3
    cell_h = height / 3
    frames: list[Image.Image] = []
    for row in range(3):
        for col in range(3):
            left = int(round(col * cell_w)) + (4 if col > 0 else 2)
            right = int(round((col + 1) * cell_w)) - 4
            top = int(round(row * cell_h)) + (4 if row > 0 else 2)
            bottom = int(round((row + 1) * cell_h)) - 4
            panel = sheet.crop((left, top, right, bottom)).convert("RGB")
            target_ratio = WIDTH / HEIGHT
            if panel.width / panel.height < target_ratio:
                crop_h = int(panel.width / target_ratio)
                crop_top = max(0, (panel.height - crop_h) // 2)
                panel = panel.crop((0, crop_top, panel.width, crop_top + crop_h))
            else:
                crop_w = int(panel.height * target_ratio)
                crop_left = max(0, (panel.width - crop_w) // 2)
                panel = panel.crop((crop_left, 0, crop_left + crop_w, panel.height))
            frames.append(panel)
    return frames


def mouth_variant(closed: Image.Image, opened: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    result = closed.copy()
    patch = opened.crop(box)
    mask = Image.new("L", patch.size, 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((5, 5, patch.width - 5, patch.height - 5), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(radius=9))
    result.paste(patch, box[:2], mask)
    return result


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    choices = [
        Path(r"C:\Windows\Fonts\simhei.ttf") if bold else Path(r"C:\Windows\Fonts\simsun.ttc"),
        Path(r"C:\Windows\Fonts\simsunb.ttf"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ]
    for path in choices:
        if path.exists():
            return ImageFont.truetype(str(path), size=size)
    return ImageFont.load_default()


FONT_DIALOGUE = load_font(48, True)
FONT_SPEAKER = load_font(31, True)
FONT_TITLE = load_font(70, True)
FONT_SMALL = load_font(29)


def smooth(value: float) -> float:
    value = min(max(value, 0.0), 1.0)
    return value * value * (3.0 - 2.0 * value)


def lip_open(t: float, start: float, end: float, frequency: float = 5.2) -> bool:
    if not start <= t <= end:
        return False
    phase = int((t - start) * frequency * 2)
    return phase % 2 == 1


def render_panel(panel: Image.Image, t: float, zoom_start: float, zoom_end: float,
                 pan_x: float = 0.0, pan_y: float = 0.0) -> Image.Image:
    progress = (t % 4.0) / 4.0
    zoom = zoom_start + (zoom_end - zoom_start) * smooth(progress)
    # A two-pixel breathing motion plus the directed pan prevents frozen poses.
    breath = math.sin(t * math.pi * 0.92) * 0.0025
    zoom += breath
    scaled_w = int(WIDTH * zoom)
    scaled_h = int(HEIGHT * zoom)
    resized = panel.resize((scaled_w, scaled_h), Image.Resampling.BICUBIC)
    max_x = max(0, scaled_w - WIDTH)
    max_y = max(0, scaled_h - HEIGHT)
    left = int(max_x * min(max(0.5 + pan_x * (progress - 0.5), 0.0), 1.0))
    top = int(max_y * min(max(0.5 + pan_y * (progress - 0.5), 0.0), 1.0))
    frame = resized.crop((left, top, left + WIDTH, top + HEIGHT))
    return ImageEnhance.Color(frame).enhance(1.035)


def stage_frame(frames: list[Image.Image], female_open: Image.Image, male_open: Image.Image,
                stage: int, t: float) -> Image.Image:
    if stage == 0:
        return render_panel(frames[0], t, 1.025, 1.065, 0.10, -0.05)
    if stage == 1:
        source = female_open if lip_open(t, 1.80, 7.18) else frames[1]
        return render_panel(source, t, 1.035, 1.085, -0.05, -0.08)
    if stage == 2:
        return render_panel(frames[3], t, 1.025, 1.075, 0.13, 0.05)
    if stage == 3:
        source = male_open if lip_open(t, 14.70, 17.84) else frames[4]
        return render_panel(source, t, 1.035, 1.09, 0.06, -0.08)
    if stage == 4:
        # A short motion transition reads as a decisive reach without leaving
        # the two body poses double-exposed for the rest of the line.
        move = smooth((t - 18.0) / 0.34)
        closed = render_panel(frames[4], t, 1.04, 1.085, 0.04, -0.06)
        reaching = render_panel(frames[5], t, 1.03, 1.08, -0.08, -0.04)
        return Image.blend(closed, reaching, move)
    if stage == 5:
        return render_panel(frames[6], t, 1.02, 1.075, 0.08, 0.03)
    if stage == 6:
        return render_panel(frames[7], t, 1.03, 1.09, -0.05, -0.06)
    return render_panel(frames[8], t, 1.025, 1.075, 0.0, -0.08)


STAGES = [
    (0.00, 1.80),
    (1.80, 7.35),
    (7.35, 14.45),
    (14.45, 18.00),
    (18.00, 22.90),
    (22.90, 23.45),
    (23.45, 26.45),
    (26.45, 28.00),
]


def image_for_time(frames: list[Image.Image], female_open: Image.Image,
                   male_open: Image.Image, t: float) -> Image.Image:
    stage = next((idx for idx, (_, end) in enumerate(STAGES) if t < end), len(STAGES) - 1)
    current = stage_frame(frames, female_open, male_open, stage, t)
    start, _ = STAGES[stage]
    if stage > 0 and t - start < 0.32:
        previous = stage_frame(frames, female_open, male_open, stage - 1, t)
        current = Image.blend(previous, current, smooth((t - start) / 0.32))
    return current


def cue_for_time(t: float) -> tuple[str, str, str] | None:
    cues = [
        (1.80, 7.18, "みく", "优希也，今天先别说“爱你”。"),
        (7.45, 10.95, "みく", "别把它当游戏，认真听我说。"),
        (11.20, 14.27, "みく", "我喜欢你。"),
        (14.70, 17.84, "优希也", "那我也不玩这个游戏了。"),
        (18.15, 22.64, "优希也", "我喜欢你。一直都是认真的。"),
        (23.05, 26.31, "みく", "……终于，我们的心意一样了。"),
    ]
    for start, end, speaker, text in cues:
        if start <= t <= end:
            color = "miku" if speaker == "みく" else "yukiya"
            return speaker, text, color
    return None


def add_petals(frame: Image.Image, t: float) -> Image.Image:
    overlay = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for index in range(22):
        speed = 34 + (index % 7) * 8
        x = (index * 173 + t * speed * (0.7 + (index % 3) * 0.16)) % (WIDTH + 180) - 90
        y = (index * 97 + t * (28 + index % 5 * 8)) % (HEIGHT + 140) - 70
        wobble = math.sin(t * 1.8 + index * 0.9) * 20
        x += wobble
        size = 5 + index % 6
        alpha = 52 + (index % 4) * 20
        draw.ellipse((x - size * 1.4, y - size * 0.55, x + size * 1.4, y + size * 0.55),
                     fill=(255, 192 + index % 30, 218, alpha))
    return Image.alpha_composite(frame.convert("RGBA"), overlay).convert("RGB")


def centered(draw: ImageDraw.ImageDraw, y: int, text: str, font: ImageFont.FreeTypeFont,
             fill: tuple[int, int, int, int], stroke: int = 3) -> None:
    box = draw.textbbox((0, 0), text, font=font, stroke_width=stroke)
    x = (WIDTH - (box[2] - box[0])) // 2
    draw.text((x, y), text, font=font, fill=fill, stroke_width=stroke, stroke_fill=(20, 13, 23, 255))


def add_titles_and_subtitles(frame: Image.Image, t: float) -> Image.Image:
    overlay = Image.new("RGBA", frame.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    if 0.28 <= t <= 1.62:
        centered(draw, 100, "这一次，是认真的。", FONT_TITLE, (255, 247, 242, 255), 4)
        centered(draw, 187, "原创同人动画短片", FONT_SMALL, (255, 221, 227, 245), 2)

    cue = cue_for_time(t)
    if cue:
        speaker, text, color = cue
        for row in range(250):
            alpha = int(170 * (row / 249) ** 1.65)
            draw.line((0, HEIGHT - 250 + row, WIDTH, HEIGHT - 250 + row), fill=(12, 7, 16, alpha))
        tag_fill = (238, 119, 153, 245) if color == "miku" else (80, 152, 174, 245)
        tag_box = draw.textbbox((0, 0), speaker, font=FONT_SPEAKER)
        tag_width = tag_box[2] - tag_box[0] + 40
        tag_x = (WIDTH - tag_width) // 2
        draw.rounded_rectangle((tag_x, 868, tag_x + tag_width, 914), radius=20, fill=tag_fill)
        centered(draw, 871, speaker, FONT_SPEAKER, (255, 255, 255, 255), 1)
        centered(draw, 929, text, FONT_DIALOGUE, (255, 250, 246, 255), 3)

    if 26.55 <= t <= 27.72:
        draw.rounded_rectangle((510, 835, 1410, 970), radius=30, fill=(24, 12, 28, 140))
        centered(draw, 866, "游戏结束，恋爱开始。", FONT_TITLE, (255, 241, 239, 255), 3)

    draw.rectangle((0, 0, WIDTH, 22), fill=(8, 4, 8, 215))
    draw.rectangle((0, HEIGHT - 22, WIDTH, HEIGHT), fill=(8, 4, 8, 215))
    return Image.alpha_composite(frame.convert("RGBA"), overlay).convert("RGB")


def fade_ends(frame: Image.Image, t: float) -> Image.Image:
    black = Image.new("RGB", frame.size, (7, 4, 8))
    if t < 0.65:
        return Image.blend(black, frame, smooth(t / 0.65))
    if t > DURATION - 0.75:
        return Image.blend(black, frame, smooth((DURATION - t) / 0.75))
    return frame


def build_video(output: Path, ffmpeg: Path) -> None:
    sheet = Image.open(KEYFRAMES)
    frames = crop_keyframes(sheet)
    # The generated sheet deliberately includes near-identical lip-sync pairs.
    female_open = mouth_variant(frames[1], frames[2], (190, 115, 325, 225))
    male_open = mouth_variant(frames[4], frames[5], (185, 112, 330, 220))

    command = [
        str(ffmpeg), "-y",
        "-f", "rawvideo", "-vcodec", "rawvideo", "-pix_fmt", "rgb24",
        "-s", f"{WIDTH}x{HEIGHT}", "-r", str(FPS), "-i", "-",
        "-i", str(MIXED_AUDIO),
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-c:a", "copy", "-shortest",
        "-movflags", "+faststart",
        "-metadata", "title=这一次，是认真的。",
        "-metadata", "comment=Original fan-made animation with synthetic non-celebrity voices.",
        str(output),
    ]
    process = subprocess.Popen(command, stdin=subprocess.PIPE)
    assert process.stdin is not None
    try:
        for frame_index in range(int(DURATION * FPS)):
            t = frame_index / FPS
            frame = image_for_time(frames, female_open, male_open, t)
            frame = add_petals(frame, t)
            frame = add_titles_and_subtitles(frame, t)
            frame = fade_ends(frame, t)
            process.stdin.write(frame.tobytes())
    finally:
        process.stdin.close()
    code = process.wait()
    if code != 0:
        raise RuntimeError(f"ffmpeg exited with code {code}")


def main() -> None:
    if not KEYFRAMES.exists():
        raise FileNotFoundError(KEYFRAMES)
    ffmpeg = Path(imageio_ffmpeg.get_ffmpeg_exe())
    asyncio.run(generate_voices())
    mix_audio(ffmpeg)
    output = versioned_output(desktop_folder(), "想要结束我爱你游戏_告白动画_有配音", ".mp4")
    build_video(output, ffmpeg)
    print(output)


if __name__ == "__main__":
    main()
