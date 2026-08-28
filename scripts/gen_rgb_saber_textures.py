#!/usr/bin/env python3
"""
GalaxyRP: [Saber RGB] generator for the two tintable saber-blade textures.

The six palette blade shaders that ship with the base game each bake their colour into the texture
itself, so none of them can be recoloured at runtime. This script draws the same two pieces of
artwork -- the blade core and the glow sprite -- in neutral greyscale instead, which is what lets
assets/client/shaders/rgbsaber.shader declare them "rgbGen vertex" and have the renderer multiply
them by whatever per-entity colour the blade is submitted with (see CG_DoSaber in cg_players.c).

Both are procedural, so this file is the source of truth rather than the images: re-run it to
regenerate them, or to tweak the falloff without hand-editing a JPEG.

    python3 scripts/gen_rgb_saber_textures.py

Requires Pillow. Writes into assets/client/gfx/effects/sabers/, which is packed into the mod pk3.
"""

import math
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")

OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "assets", "client", "gfx", "effects", "sabers",
)

# --- blade core -----------------------------------------------------------------------------
# Drawn as an RT_LINE: a vertical bar stretched along the blade, so the horizontal axis is the
# across-the-blade brightness falloff and the vertical axis is the along-the-blade envelope. A flat
# saturated centre with a cosine shoulder is what reads as "hot core with a soft edge".
CORE_W, CORE_H = 64, 256
CORE_PLATEAU = 5.0		# half-width of the fully saturated centre, in pixels
CORE_FALLOFF = 22.0		# half-width at which the core has faded to nothing
CORE_TIP_FADE = 14		# rows at the muzzle end that ramp up, softening the emitter
CORE_TIP_FLOOR = 0.46	# brightness the ramp starts from
CORE_END_CUT = 6		# rows at the tip end left black, so the blade ends rather than smears

# --- glow sprite ----------------------------------------------------------------------------
# Drawn as an RT_SABER_GLOW sprite: a radial blob, additively blended many times over, so it peaks
# well below white -- pushing it to 255 blows out to a white haze the moment two blobs overlap.
GLOW_SIZE = 128
GLOW_PEAK = 176
GLOW_RADIUS = 54.0
GLOW_EXPONENT = 1.7	# shoulder shape; higher is a tighter, less hazy blob


def core_pixel(x, y):
    d = abs(x - (CORE_W - 1) / 2.0)
    if d <= CORE_PLATEAU:
        across = 1.0
    elif d >= CORE_FALLOFF:
        across = 0.0
    else:
        across = math.cos(math.pi / 2 * (d - CORE_PLATEAU) / (CORE_FALLOFF - CORE_PLATEAU))

    if y >= CORE_H - CORE_END_CUT:
        along = 0.0
    elif y < CORE_TIP_FADE:
        along = CORE_TIP_FLOOR + (1.0 - CORE_TIP_FLOOR) * (y / float(CORE_TIP_FADE))
    else:
        along = 1.0

    return int(round(255 * across * along))


def glow_pixel(x, y):
    c = (GLOW_SIZE - 1) / 2.0
    r = math.hypot(x - c, y - c)
    if r >= GLOW_RADIUS:
        return 0
    return int(round(GLOW_PEAK * math.cos(math.pi / 2 * r / GLOW_RADIUS) ** GLOW_EXPONENT))


def write(name, size, fn):
    w, h = size
    img = Image.new("L", size)
    img.putdata([fn(x, y) for y in range(h) for x in range(w)])
    path = os.path.join(OUT_DIR, name)
    # The base game's own saber textures are JPEGs; quality 95 keeps ringing off the hard centre
    # while staying in the same few-kilobyte range.
    img.convert("RGB").save(path, "JPEG", quality=95)
    print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    write("rgb_line.jpg", (CORE_W, CORE_H), core_pixel)
    write("rgb_glow.jpg", (GLOW_SIZE, GLOW_SIZE), glow_pixel)


if __name__ == "__main__":
    main()
