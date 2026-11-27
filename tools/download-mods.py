#!/usr/bin/env python3

import html.parser
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path


SOURCE_URL = "https://mod.crh.ovh/mods/"
SUPPORTED_EXTENSIONS = {".bem", ".digi", ".ft", ".fst", ".it", ".mod", ".nst", ".s3m", ".stm", ".stk", ".xm"}


class DirectoryIndex(html.parser.HTMLParser):
	def __init__(self):
		super().__init__()
		self.links = []
		self.title = ""
		self.in_title = False

	def handle_starttag(self, tag, attrs):
		if tag == "title":
			self.in_title = True
		elif tag == "a":
			self.links.extend(value for name, value in attrs if name == "href" and value)

	def handle_endtag(self, tag):
		if tag == "title":
			self.in_title = False

	def handle_data(self, data):
		if self.in_title:
			self.title += data


class NoRedirect(urllib.request.HTTPRedirectHandler):
	def redirect_request(self, req, fp, code, msg, headers, newurl):
		return None


def module_names(index):
	names = set()
	for href in index.links:
		link = urllib.parse.urlsplit(href)
		if link.scheme or link.netloc or link.query or link.fragment:
			continue
		name = urllib.parse.unquote(link.path)
		if not name or name in (".", "..") or "/" in name or "\\" in name or "\x00" in name:
			continue
		if Path(name).suffix.lower() in SUPPORTED_EXTENSIONS:
			names.add(name)
	return sorted(names, key=str.casefold)


def sync_library(source_url, output_dir):
	opener = urllib.request.build_opener(NoRedirect)
	with opener.open(source_url, timeout=30) as response:
		content = response.read(4 * 1024 * 1024 + 1)
		if len(content) > 4 * 1024 * 1024:
			raise ValueError("Module directory listing is too large")
		index = DirectoryIndex()
		index.feed(content.decode("utf-8"))
		if not index.title.strip().startswith("Index of "):
			raise ValueError("Module directory did not return an Apache directory listing")

	names = module_names(index)
	output_dir.parent.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix=".mods-download-", dir=output_dir.parent) as temp:
		temp_dir = Path(temp)
		staging = temp_dir / "mods"
		staging.mkdir()
		for name in names:
			url = urllib.parse.urljoin(source_url, urllib.parse.quote(name, safe=""))
			with opener.open(url, timeout=30) as response:
				if response.headers.get_content_type() == "text/html":
					raise ValueError(f"Module request returned HTML: {name}")
				with (staging / name).open("wb") as file:
					shutil.copyfileobj(response, file)
				length = response.headers.get("Content-Length")
				if length is not None and (staging / name).stat().st_size != int(length):
					raise ValueError(f"Incomplete module download: {name}")
			print(f"Downloaded {name}", flush=True)

		subprocess.run(
			[sys.executable, str(Path(__file__).with_name("generate-mods-index.py")), str(staging), str(staging / "index.json")],
			check=True,
		)

		previous = temp_dir / "previous"
		if output_dir.exists():
			output_dir.rename(previous)
		try:
			staging.rename(output_dir)
		except OSError:
			if previous.exists():
				previous.rename(output_dir)
			raise

	print(f"Downloaded {len(names)} modules to {output_dir}")


def main():
	if len(sys.argv) != 2:
		raise SystemExit("Usage: download-mods.py <output-directory>")
	try:
		sync_library(SOURCE_URL, Path(sys.argv[1]))
	except (OSError, ValueError, UnicodeError, subprocess.CalledProcessError) as error:
		raise SystemExit(f"Could not download module library: {error}") from error


if __name__ == "__main__":
	main()
