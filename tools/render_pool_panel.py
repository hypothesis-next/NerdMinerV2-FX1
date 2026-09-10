#!/usr/bin/env python3
"""Render deterministic UI mocks from the production RGB565 asset and layout."""

from io import BytesIO
from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont


PROJECT = Path(__file__).resolve().parents[1]
IMAGE_HEADER = PROJECT / "src" / "media" / "images_320_170.h"
FONT_HEADER = PROJECT / "src" / "media" / "myFonts.h"
OUTPUT = PROJECT / "docs" / "images"


def extract_array(path: Path, declaration: str) -> list[int]:
    source = path.read_text(encoding="utf-8")
    match = re.search(declaration + r"[^=]*=\s*\{(.*?)\};", source, re.S)
    if not match:
        raise RuntimeError(f"Unable to find array in {path.name}")
    body = re.sub(r"//.*", "", match.group(1))
    return [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]+", body)]


def rgb565(value: int) -> tuple[int, int, int]:
    return (
        ((value >> 11) & 0x1F) * 255 // 31,
        ((value >> 5) & 0x3F) * 255 // 63,
        (value & 0x1F) * 255 // 31,
    )


def centered(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str,
             font: ImageFont.ImageFont, fill: tuple[int, int, int]) -> None:
    box = draw.textbbox((0, 0), text, font=font)
    width = box[2] - box[0]
    height = box[3] - box[1]
    draw.text((xy[0] - width / 2, xy[1] - height / 2 - box[1]), text,
              font=font, fill=fill)


def font_from_header(size: int) -> ImageFont.FreeTypeFont:
    values = extract_array(
        FONT_HEADER, r"const\s+unsigned\s+char\s+DigitalNumbers\[\]")
    return ImageFont.truetype(BytesIO(bytes(values)), size=size)


def render(filename: str, pool: str, best: str, workers: str,
           hashrate: str, state: str, blue: bool) -> None:
    pixels = extract_array(
        IMAGE_HEADER, r"const\s+unsigned\s+short\s+MinerScreen\[[^\]]+\]")
    if len(pixels) != 320 * 170:
        raise RuntimeError("Unexpected MinerScreen dimensions")

    screen = Image.new("RGB", (320, 240))
    screen.putdata([rgb565(value) for value in pixels])
    draw = ImageDraw.Draw(screen)
    header = rgb565(0x4ACD)
    panel = rgb565(0x0E3E if blue else 0x9580)
    black = (0, 0, 0)
    white = (255, 255, 255)

    draw.rectangle((0, 170, 319, 189), fill=header)
    draw.rectangle((0, 190, 319, 239), fill=panel)
    draw.line((106, 190, 106, 239), fill=header)
    draw.line((210, 190, 210, 239), fill=header)

    label_font = ImageFont.truetype("arial.ttf", 10)
    name_font = ImageFont.truetype("arialbd.ttf", 14)
    small_name_font = ImageFont.truetype("arialbd.ttf", 10)
    state_font = ImageFont.truetype("consolab.ttf", 9)
    value_font = font_from_header(20)

    pool_right = 316 if not state else 270
    text_box = draw.textbbox((0, 0), pool, font=name_font)
    active_name_font = (small_name_font if text_box[2] - text_box[0] >
                        pool_right - 4 else name_font)
    centered(draw, ((pool_right + 4) // 2, 180), pool, active_name_font,
             white)
    if state:
        state_box = draw.textbbox((0, 0), state, font=state_font)
        draw.text((316 - (state_box[2] - state_box[0]), 180 -
                   (state_box[3] - state_box[1]) / 2 - state_box[1]),
                  state, font=state_font, fill=white)

    centered(draw, (53, 197), "Best Ever", label_font, black)
    centered(draw, (158, 197), "WORKERS", label_font, black)
    centered(draw, (265, 197), "Total Hash Rate", label_font, black)
    fallback_value_font = ImageFont.truetype("arialbd.ttf", 16)
    values = ((53, best), (158, workers), (265, hashrate))
    for x, value in values:
        active_value_font = (fallback_value_font if value in ("N/A", "TESTNET")
                             else value_font)
        centered(draw, (x, 220), value, active_value_font, black)

    canvas = Image.new("RGB", (344, 276), (42, 45, 50))
    caption_font = ImageFont.truetype("arialbd.ttf", 11)
    caption = "UI RENDER MOCK - NOT A HARDWARE EMULATOR"
    centered(ImageDraw.Draw(canvas), (172, 12), caption, caption_font,
             (235, 235, 235))
    canvas.paste(screen, (12, 24))
    ImageDraw.Draw(canvas).rectangle((11, 23, 332, 264), outline=(0, 0, 0))
    OUTPUT.mkdir(parents=True, exist_ok=True)
    canvas.save(OUTPUT / filename, optimize=True)


def main() -> None:
    render("pool-panel-heliospool.png", "HeliosPool", "7.65M", "2", "123K",
           "", True)
    render("pool-panel-public-pool.png", "Public Pool", "1.23K", "2",
           "3.50K", "", False)
    render("pool-panel-unsupported.png", "solo.example.com", "N/A", "N/A",
           "N/A", "N/A", True)
    render("pool-panel-stale.png", "HeliosPool", "7.65M", "2", "123K",
           "STALE", False)


if __name__ == "__main__":
    main()
