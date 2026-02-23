#!/usr/bin/env python3
from PIL import Image, ImageDraw


def make_tile(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Rounded background
    pad = max(1, size // 24)
    r = max(2, size // 5)
    d.rounded_rectangle([pad, pad, size - pad - 1, size - pad - 1], radius=r,
                        fill=(20, 108, 86, 255), outline=(12, 66, 100, 255), width=max(1, size // 32))

    # Waveform
    wave_y = int(size * 0.68)
    amp = max(1, int(size * 0.10))
    left = int(size * 0.10)
    right = int(size * 0.90)
    points = []
    import math
    for i in range(49):
        t = i / 48.0
        x = int(left + (right - left) * t)
        y = int(wave_y - math.sin(t * 2.0 * math.pi * 1.6) * amp)
        points.append((x, y))
    d.line(points, fill=(223, 255, 244, 255), width=max(2, size // 16), joint="curve")

    # qc monogram (geometric approximation to avoid font dependency)
    fg = (245, 255, 252, 255)
    # q
    q_cx = int(size * 0.40)
    q_cy = int(size * 0.40)
    q_r = int(size * 0.12)
    d.ellipse([q_cx - q_r, q_cy - q_r, q_cx + q_r, q_cy + q_r], outline=fg, width=max(2, size // 18))
    d.line([(q_cx + q_r - 1, q_cy + q_r - 1), (q_cx + int(size * 0.12), q_cy + int(size * 0.16))],
           fill=fg, width=max(2, size // 18))
    # c
    c_cx = int(size * 0.58)
    c_cy = int(size * 0.40)
    c_r = int(size * 0.12)
    start = 35
    end = 325
    d.arc([c_cx - c_r, c_cy - c_r, c_cx + c_r, c_cy + c_r], start=start, end=end,
          fill=fg, width=max(2, size // 18))

    return img


def main() -> None:
    sizes = [16, 24, 32, 48, 64, 128, 256]
    base = make_tile(256)
    images = [base.resize((s, s), Image.Resampling.LANCZOS) for s in sizes]
    images[0].save("windows/app.ico", format="ICO", sizes=[(s, s) for s in sizes], append_images=images[1:])


if __name__ == "__main__":
    main()
