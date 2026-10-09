"""Write textures/noise_<size>.png: one random grey value per pixel (white noise), one channel.

Shaders read it two ways (shader_lib/texture_noise.gdshaderinc):
  tex_noise  bilinear filtering + the smoothstep trick → smooth value noise, 1 read instead of 8 hashes
  tex_hash   texelFetch of one exact pixel → one random number per grid cell (grain, sparkles)
One channel is enough: both uses read the same pixels at unrelated coordinates. A second channel
would only help if a shader needed two independent random values at the SAME coordinate.
Usage: python3 make_noise_texture.py [size=128]   (128 → 16 KB on the GPU, repeats every 128 cells)
"""
import random
import sys
from pathlib import Path

from PIL import Image

size = int(sys.argv[1]) if len(sys.argv) > 1 else 128
rng = random.Random(7)
img = Image.new("L", (size, size))
img.putdata([rng.randrange(256) for _ in range(size * size)])
out = Path(__file__).resolve().parent.parent / "textures" / f"noise_{size}.png"
img.save(out)
print(out, out.stat().st_size, "bytes")
