#!/usr/bin/env python3
"""Copy the playable WASM build into a standalone GitHub Pages directory.

The original executable is never a packaging input. Resource archives are
split byte-for-byte (not compressed or transformed) to stay below GitHub's
per-file limit. Existing destination files are updated, but never deleted.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SITE_FILES = ("game.html", "game.js", "game.css", "th20_game.js", "th20_game.wasm")
RESOURCE_FILES = ("th20.dat", "thbgm.dat")
DEFAULT_CHUNK_SIZE = 32 * 1024 * 1024
MAX_CHUNK_SIZE = 49 * 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def package_resource(source: Path, destination: Path, chunk_size: int) -> dict:
    digest = hashlib.sha256()
    chunks = []
    size = 0
    with source.open("rb") as stream:
        while block := stream.read(chunk_size):
            name = f"{source.name}.part{len(chunks):04d}"
            output = destination / name
            if output.is_symlink():
                raise ValueError(f"Refusing symlinked output: {output}")
            output.write_bytes(block)
            digest.update(block)
            size += len(block)
            chunks.append({
                "path": name,
                "size": len(block),
                "sha256": hashlib.sha256(block).hexdigest(),
            })
    # Verify the exact bytes on disk, including concatenation order. Browsers
    # can then validate one bounded chunk at a time without making another
    # full-size copy of the 430 MB music archive just to hash it.
    published_digest = hashlib.sha256()
    published_size = 0
    for chunk in chunks:
        with (destination / chunk["path"]).open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                published_digest.update(block)
                published_size += len(block)
    if published_size != size or published_digest.hexdigest() != digest.hexdigest():
        raise OSError(f"Published resource verification failed: {source.name}")
    return {"name": source.name, "size": size,
            "sha256": digest.hexdigest(), "chunks": chunks}


def prepare_site(build_dir: Path, output_dir: Path, resources: str = "chunks",
                 chunk_size: int = DEFAULT_CHUNK_SIZE) -> dict:
    build_dir = build_dir.resolve()
    output_dir = output_dir.resolve()
    if build_dir == output_dir or build_dir in output_dir.parents or output_dir in build_dir.parents:
        raise ValueError("Build and output directories must not contain one another")
    if resources not in ("chunks", "copy", "omit"):
        raise ValueError("Resource mode must be chunks, copy, or omit")
    if not 0 < chunk_size <= MAX_CHUNK_SIZE:
        raise ValueError("Chunk size must be positive and at most 49 MiB")
    required = [build_dir / name for name in SITE_FILES]
    if resources != "omit":
        required.extend(build_dir / "game-data" / name for name in RESOURCE_FILES)
    for path in required:
        if not path.is_file():
            raise FileNotFoundError(f"Missing build input: {path}")
    game_html = (build_dir / "game.html").read_text(encoding="utf-8")
    if resources == "chunks":
        loader_tag = '<script src="game.js"></script>'
        if game_html.count(loader_tag) != 1:
            raise ValueError("game.html must contain one explicit game.js script tag")
        game_html = game_html.replace(loader_tag,
            '<script src="game.js" data-resource-manifest="game-data/manifest.json"></script>')
    if resources == "omit" and (output_dir / "game-data").exists():
        raise ValueError("Choose an output without game-data for resource-free packaging")
    # Symlinked outputs could overwrite files outside the requested directory.
    destination_names = [*SITE_FILES, "index.html", ".nojekyll", "game-data"]
    for name in destination_names:
        if (output_dir / name).is_symlink():
            raise ValueError(f"Refusing symlinked output: {output_dir / name}")
    output_dir.mkdir(parents=True, exist_ok=True)
    for name in SITE_FILES:
        shutil.copyfile(build_dir / name, output_dir / name)
    (output_dir / "game.html").write_text(game_html, encoding="utf-8", newline="\n")
    (output_dir / "index.html").write_text(game_html, encoding="utf-8", newline="\n")
    (output_dir / ".nojekyll").write_text("", encoding="utf-8")

    manifest = {"version": 1, "files": []}
    if resources != "omit":
        data_dir = output_dir / "game-data"
        data_dir.mkdir(parents=True, exist_ok=True)
        for name in RESOURCE_FILES:
            source = build_dir / "game-data" / name
            if resources == "chunks":
                entry = package_resource(source, data_dir, chunk_size)
            else:
                destination = data_dir / name
                if destination.is_symlink():
                    raise ValueError(f"Refusing symlinked output: {destination}")
                shutil.copyfile(source, destination)
                size = destination.stat().st_size
                digest = sha256_file(destination)
                entry = {"name": name, "size": size, "sha256": digest,
                         "chunks": [{"path": name, "size": size, "sha256": digest}]}
            manifest["files"].append(entry)
        manifest_path = data_dir / "manifest.json"
        if manifest_path.is_symlink():
            raise ValueError(f"Refusing symlinked output: {manifest_path}")
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return {"output_dir": str(output_dir), "resource_mode": resources,
            "resource_bytes": sum(entry["size"] for entry in manifest["files"]),
            "resource_chunks": sum(len(entry["chunks"]) for entry in manifest["files"]),
            "files": manifest["files"]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=PROJECT_ROOT / "build_web")
    parser.add_argument("--output-dir", type=Path, default=PROJECT_ROOT / "docs")
    parser.add_argument("--resources", choices=("chunks", "copy", "omit"), default="chunks",
                        help="chunks is suitable for Git; omit leaves game resources out")
    parser.add_argument("--chunk-mib", type=int, default=32,
                        help="Maximum part size in MiB (1-49; default: 32)")
    args = parser.parse_args()
    try:
        result = prepare_site(args.build_dir, args.output_dir, args.resources,
                              args.chunk_mib * 1024 * 1024)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Packaging failed: {error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
