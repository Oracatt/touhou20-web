"""Byte-level packaging checks using small temporary resource archives."""

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "prepare_github_site", Path(__file__).resolve().parents[1] / "tools" / "prepare_github_site.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class SitePackagingTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name) / "build"
        self.site = Path(self.temporary.name) / "site"
        (self.build / "game-data").mkdir(parents=True)
        for name in packager.SITE_FILES:
            (self.build / name).write_bytes(f"fixture:{name}".encode())
        (self.build / "game.html").write_text('<html><script src="game.js"></script></html>')
        self.resources = {"th20.dat": bytes(range(256)) * 4 + b"archive-tail",
                          "thbgm.dat": bytes(range(255, -1, -1)) * 8 + b"audio-tail"}
        for name, data in self.resources.items():
            (self.build / "game-data" / name).write_bytes(data)
        (self.build / "original.exe").write_bytes(b"must not be published")

    def verify_resources(self):
        manifest = json.loads((self.site / "game-data" / "manifest.json").read_text())
        self.assertEqual(manifest["version"], 1)
        for entry in manifest["files"]:
            restored = b""
            for chunk in entry["chunks"]:
                part = (self.site / "game-data" / chunk["path"]).read_bytes()
                self.assertEqual(len(part), chunk["size"])
                self.assertEqual(hashlib.sha256(part).hexdigest(), chunk["sha256"])
                restored += part
            self.assertEqual(restored, self.resources[entry["name"]])
            self.assertEqual(len(restored), entry["size"])
            self.assertEqual(hashlib.sha256(restored).hexdigest(), entry["sha256"])
            self.assertEqual((self.build / "game-data" / entry["name"]).read_bytes(), restored)

    def test_chunked_site_reconstructs_exactly_and_preserves_existing_files(self):
        self.site.mkdir()
        (self.site / "keep.txt").write_text("keep")
        result = packager.prepare_site(self.build, self.site, chunk_size=1024)
        self.verify_resources()
        self.assertEqual(result["resource_chunks"], 5)
        self.assertEqual((self.site / "index.html").read_bytes(), (self.site / "game.html").read_bytes())
        self.assertIn('data-resource-manifest="game-data/manifest.json"',
                      (self.site / "index.html").read_text())
        self.assertTrue((self.site / ".nojekyll").is_file())
        self.assertEqual((self.site / "keep.txt").read_text(), "keep")
        self.assertFalse((self.site / "original.exe").exists())

    def test_copy_and_resource_free_modes(self):
        packager.prepare_site(self.build, self.site, "copy")
        self.verify_resources()
        self.assertEqual((self.site / "game.html").read_bytes(), (self.build / "game.html").read_bytes())
        resource_free = self.site.parent / "resource-free"
        packager.prepare_site(self.build, resource_free, "omit")
        self.assertFalse((resource_free / "game-data").exists())
        self.assertTrue((resource_free / "th20_game.wasm").is_file())

    def test_invalid_inputs_leave_destination_untouched(self):
        with self.assertRaises(ValueError):
            packager.prepare_site(self.build, self.build / "site")
        self.assertFalse((self.build / "site").exists())
        with self.assertRaises(ValueError):
            packager.prepare_site(self.build, self.site, chunk_size=50 * 1024 * 1024)
        self.assertFalse(self.site.exists())
        (self.build / "game.js").unlink()
        with self.assertRaises(FileNotFoundError):
            packager.prepare_site(self.build, self.site)
        self.assertFalse(self.site.exists())


if __name__ == "__main__":
    unittest.main()
