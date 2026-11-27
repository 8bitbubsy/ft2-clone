#!/usr/bin/env python3

import json
import shutil
import sys
import tempfile
from pathlib import Path

SUPPORTED_EXTENSIONS = {".bem", ".digi", ".ft", ".fst", ".it", ".mod", ".nst", ".s3m", ".stm", ".stk", ".xm"}


def generate_index(mods_dir: Path, output_file: Path) -> None:
	mods_dir.mkdir(parents=True, exist_ok=True)
	output_file.parent.mkdir(parents=True, exist_ok=True)

	mods = [
		{"name": path.name, "size": path.stat().st_size}
		for path in sorted(mods_dir.iterdir(), key=lambda item: item.name.casefold())
		if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS
	]
	output_file.write_text(
		json.dumps({"files": mods}, ensure_ascii=False, indent=2) + "\n",
		encoding="utf-8",
	)


def copy_library(source: Path, destination: Path) -> None:
	source.mkdir(parents=True, exist_ok=True)
	destination.parent.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix=".mods-copy-", dir=destination.parent) as temp:
		staging = Path(temp) / "mods"
		staging.mkdir()
		for path in source.iterdir():
			if path.is_file() and path.suffix.lower() in SUPPORTED_EXTENSIONS:
				shutil.copy2(path, staging / path.name)

		generate_index(staging, staging / "index.json")
		previous = Path(temp) / "previous"
		if destination.exists():
			destination.rename(previous)
		try:
			staging.rename(destination)
		except OSError:
			if previous.exists():
				previous.rename(destination)
			raise


def main() -> None:
	if len(sys.argv) == 4 and sys.argv[1] == "--copy":
		copy_library(Path(sys.argv[2]), Path(sys.argv[3]))
	elif len(sys.argv) == 3:
		generate_index(Path(sys.argv[1]), Path(sys.argv[2]))
	else:
		raise SystemExit("Usage: generate-mods-index.py <mods-directory> <output-file> | --copy <mods-directory> <output-directory>")


if __name__ == "__main__":
	main()
