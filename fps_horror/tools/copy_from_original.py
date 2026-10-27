"""Copy what we reuse from Bonkahe's FPSHorrorGDScript (MIT) into this project, sized for mobile.

    python3 tools/copy_from_original.py [path/to/FPSHorrorGDScript/CurrentProject]

  reference/original_code/   all their GDScript, untouched, to read while we rewrite it
                             (Godot ignores the folder: reference/.gdignore)
  assets/enemy/              Cultist.gltf + its textures
  assets/player/             arms (mesh + animations), bullet, shell, speed loader, flashlight + textures
  assets/env/                environment meshes + textures
  assets/effects/            effect meshes + textures

Textures are shrunk with macOS `sips`: characters to 1024 px max, environment and effects to
512 px max (their originals are mostly 4096 px). Godot then compresses them (ETC2/ASTC) on
import. Safe to run again; it overwrites.
"""
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/Users/Shared/Code/FPSHorrorGDScript/CurrentProject")

MESHES = {  # folder under Meshes/ -> destination
    "Enemies": "assets/enemy",
    "Arms": "assets/player",
    "Effects": "assets/effects",
    "Enviroment": "assets/env",
}
TEXTURES = {  # folder under Textures/ -> (destination, max size in px)
    "Enemies": ("assets/enemy/textures", 1024),
    "Arms": ("assets/player/textures", 1024),
    "Revolver": ("assets/player/textures", 1024),
    "Effects": ("assets/effects/textures", 512),
    "Enviroment": ("assets/env/textures", 512),
}
ROOT_TEXTURES = ("assets/env/textures/level", 512)  # the _albedo000.png ... set at the Textures/ root


def largest_side(png: Path) -> int:
    out = subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", str(png)], capture_output=True, text=True).stdout
    return max(int(line.split()[-1]) for line in out.splitlines() if "pixel" in line)


def copy_texture(src: Path, dst: Path, limit: int) -> None:
    """Copy, then shrink only if bigger than `limit` (sips -Z keeps the aspect ratio)."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    if largest_side(dst) > limit:
        subprocess.run(["sips", "-Z", str(limit), str(dst)], capture_output=True, check=True)


def main() -> None:
    code = HERE / "reference/original_code"
    shutil.rmtree(code, ignore_errors=True)
    shutil.copytree(SRC / "Code", code, ignore=shutil.ignore_patterns("*.uid"))
    (HERE / "reference/.gdignore").touch()
    shutil.copy2(SRC.parent / "LICENSE", HERE / "reference/LICENSE_original.txt")

    for folder, dest in MESHES.items():
        for gltf in (SRC / "Meshes" / folder).rglob("*.gltf"):
            out = HERE / dest / gltf.relative_to(SRC / "Meshes" / folder)
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(gltf, out)

    for folder, (dest, limit) in TEXTURES.items():
        for png in (SRC / "Textures" / folder).rglob("*.png"):
            copy_texture(png, HERE / dest / png.relative_to(SRC / "Textures" / folder), limit)
    for png in (SRC / "Textures").glob("_*.png"):
        copy_texture(png, HERE / ROOT_TEXTURES[0] / png.name, ROOT_TEXTURES[1])

    shutil.copy2(SRC.parent / "LICENSE", HERE / "assets/LICENSE_original.txt")
    print("done")


if __name__ == "__main__":
    main()
