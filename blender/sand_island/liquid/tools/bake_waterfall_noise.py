#!/usr/bin/env python3
"""Bake the simple waterfall's noises into two tiling 3D textures (make waterfall-noise).

Every waterfall shader spends its time in the same few noises, worked out per pixel: voronoi
(27 neighbour cells a lookup) and fBM (3 sizes of noise, 8 corners each). They are worked out
here once, on a grid, and the shaders read them back with one texture() each (the baked steps).

  waterfall_simple/noise/noise_a.png   r  voronoi F1: distance to the nearest point, 0..1
                                       g  F1 minus Smooth F1 (the cells' borders, the lace), x 2
                                       b  value-noise fBM, roughness 0.5 (the sheet's fbm3, fbm)
                                       a  value-noise fBM, roughness 0.2 (the sheet's boil, blobs)
  waterfall_simple/noise/noise_b.png   r  Perlin fBM, roughness 0.5 (pool, froth, stones)
                                       g  Perlin fBM, roughness 0.375 (the pool's glints)
                                          (r, g: kept between 0.25 and 0.75, so stored 2 x as
                                          far from 0.5, to use all 256 levels)
                                       b  distance to the nearest cell border (stone cracks), x 2
                                       a  1 (unused)

Each is SIZE x SIZE x SIZE texels covering PERIOD noise units (cells, whole points of the noise)
on every side, and wraps: the grid's random numbers repeat every PERIOD units (every PERIOD x 2,
x 4 for fBM's smaller sizes), so the texture tiles with no seam and a shader reads it at
p / PERIOD with repeat on. Same maths as the shaders (Dave Hoskins' hash, the same smoothing),
so the patterns look the same; only the random layout differs (it wraps). The 3D texture is
saved as its SIZE slices side by side (ATLAS_X across, ATLAS_Y down: slice z = row x ATLAS_X +
column), imported by Godot as a Texture3D (noise_*.png.import).
"""
import os
import struct
import sys
import zlib

import numpy as np

SIZE = 96  # texels on each side (96 x 96 x 96 x 4 bytes: 3.4 MB of GPU memory a texture)
PERIOD = 6  # noise units on each side: 16 texels a unit
ATLAS_X, ATLAS_Y = 12, 8  # slices across and down the PNG (12 x 8 = SIZE)
OCTAVES = 3  # fBM sizes, as in the shaders

F32 = np.float32


def fract(x):
	return x - np.floor(x)


def smooth01(x):
	x = np.clip(x, 0.0, 1.0)
	return x * x * (3.0 - 2.0 * x)


def hash33(c):
	"""Dave Hoskins' hash33 on whole-number points c (..., 3): 3 random numbers 0..1."""
	p = fract(c * np.array([0.1031, 0.1030, 0.0973], F32))
	p = p + (p * (p[..., [1, 0, 2]] + F32(33.33))).sum(-1, keepdims=True)
	return fract((p[..., [0, 0, 1]] + p[..., [1, 0, 0]]) * p[..., [2, 1, 0]])


def grid_points():
	"""The texel centres in noise units, (z, y, x, 3) holding (x, y, z)."""
	t = ((np.arange(SIZE, dtype=F32) + 0.5) / SIZE * PERIOD).astype(F32)
	z, y, x = np.meshgrid(t, t, t, indexing="ij")
	return np.stack([x, y, z], -1)


def offsets():
	for k in (-1, 0, 1):
		for j in (-1, 0, 1):
			for i in (-1, 0, 1):
				yield np.array([i, j, k], F32)


def voronoi(p):
	"""F1, F1 minus Smooth F1 (smoothness 1) and the distance to the nearest border."""
	cell = np.floor(p)
	local = p - cell
	nearest = np.full(p.shape[:-1], 8.0, F32)
	smooth_nearest = np.full(p.shape[:-1], 8.0, F32)
	to_nearest = np.zeros(p.shape, F32)
	nearest_sq = np.full(p.shape[:-1], 64.0, F32)
	for offset in offsets():
		to_point = offset + hash33(np.mod(cell + offset, PERIOD)) - local
		d_sq = (to_point * to_point).sum(-1)
		d = np.sqrt(d_sq)
		nearest = np.minimum(nearest, d)
		h = smooth01(0.5 + (smooth_nearest - d))
		smooth_nearest = smooth_nearest + (d - smooth_nearest) * h - 0.5 * h * (1.0 - h)
		closer = d_sq < nearest_sq
		nearest_sq = np.where(closer, d_sq, nearest_sq)
		to_nearest = np.where(closer[..., None], to_point, to_nearest)
	border = np.full(p.shape[:-1], 8.0, F32)
	for offset in offsets():
		to_point = offset + hash33(np.mod(cell + offset, PERIOD)) - local
		across = to_point - to_nearest
		length = np.sqrt((across * across).sum(-1))
		far_enough = length > 0.01
		plane = ((to_nearest + to_point) * 0.5 * across).sum(-1) / np.maximum(length, 1e-6)
		border = np.where(far_enough, np.minimum(border, plane), border)
	return nearest, nearest - smooth_nearest, border


def corners():
	for c in ((0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)):
		yield np.array(c, F32)


def trilinear(values, u):
	"""Blend the 8 corner values (in corners() order) by u (..., 3)."""
	x0 = values[0] + (values[1] - values[0]) * u[..., 0]
	x1 = values[2] + (values[3] - values[2]) * u[..., 0]
	x2 = values[4] + (values[5] - values[4]) * u[..., 0]
	x3 = values[6] + (values[7] - values[6]) * u[..., 0]
	y0 = x0 + (x1 - x0) * u[..., 1]
	y1 = x2 + (x3 - x2) * u[..., 1]
	return y0 + (y1 - y0) * u[..., 2]


def value_noise(p, period):
	"""The sheet's noise3: random heights at whole points, smoothed between, 0..1."""
	i = np.floor(p)
	f = p - i
	u = f * f * (3.0 - 2.0 * f)
	return trilinear([hash33(np.mod(i + c, period))[..., 0] for c in corners()], u)


def perlin_noise(p, period):
	"""The pool's gnoise3: random slopes at whole points, about -1..1."""
	i = np.floor(p)
	f = p - i
	u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0)
	values = []
	for c in corners():
		gradient = hash33(np.mod(i + c, period)) * 2.0 - 1.0
		values.append((gradient * (f - c)).sum(-1))
	return trilinear(values, u)


def fbm(octaves, roughness):
	"""Sizes added, each smaller one weighted by roughness, divided back to the first's range."""
	total = sum(roughness ** k for k in range(len(octaves)))
	return sum(n * roughness ** k for k, n in enumerate(octaves)) / total


def to_bytes(v, scale=1.0):
	return np.clip(np.round(v * scale * 255.0), 0, 255).astype(np.uint8)


def atlas(channels):
	"""(z, y, x) channels -> the PNG's rows: slices laid ATLAS_X across, ATLAS_Y down."""
	rgba = np.stack(channels, -1)  # z, y, x, 4
	rgba = rgba.reshape(ATLAS_Y, ATLAS_X, SIZE, SIZE, 4).transpose(0, 2, 1, 3, 4)
	return rgba.reshape(ATLAS_Y * SIZE, ATLAS_X * SIZE, 4)


def write_png(path, rgba):
	height, width, _ = rgba.shape
	raw = b"".join(b"\x00" + rgba[row].tobytes() for row in range(height))
	def chunk(kind, data):
		return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
	with open(path, "wb") as f:
		f.write(b"\x89PNG\r\n\x1a\n")
		f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
		f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
		f.write(chunk(b"IEND", b""))


def report(name, v):
	print("  %-22s min %.3f  max %.3f  mean %.3f" % (name, v.min(), v.max(), v.mean()))


def main():
	out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "waterfall_simple", "noise")
	os.makedirs(out_dir, exist_ok=True)
	p = grid_points()
	print("voronoi ...")
	f1, edges, border = voronoi(p)
	print("value noise ...")
	value = [value_noise(p * 2 ** k, PERIOD * 2 ** k) for k in range(OCTAVES)]
	print("Perlin noise ...")
	perlin = [perlin_noise(p * 2 ** k, PERIOD * 2 ** k) for k in range(OCTAVES)]
	value_05, value_02 = fbm(value, 0.5), fbm(value, 0.2)
	perlin_05 = 0.5 + 0.5 * fbm(perlin, 0.5)
	perlin_0375 = 0.5 + 0.5 * fbm(perlin, 0.375)
	for name, v in (("F1", f1), ("F1 - smooth F1", edges), ("border distance", border), ("value fbm 0.5", value_05),
			("value fbm 0.2", value_02), ("Perlin fbm 0.5", perlin_05), ("Perlin fbm 0.375", perlin_0375)):
		report(name, v)
	one = np.full(f1.shape, 255, np.uint8)
	write_png(os.path.join(out_dir, "noise_a.png"), atlas(
			[to_bytes(f1), to_bytes(edges, 2.0), to_bytes(value_05), to_bytes(value_02)]))
	write_png(os.path.join(out_dir, "noise_b.png"), atlas(
			[to_bytes(0.5 + (perlin_05 - 0.5) * 2.0), to_bytes(0.5 + (perlin_0375 - 0.5) * 2.0), to_bytes(border, 2.0), one]))
	print("saved", os.path.normpath(out_dir), "noise_a.png, noise_b.png")


if __name__ == "__main__":
	sys.exit(main())
