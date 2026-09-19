# Web regression checks

From the repository root, run:

```powershell
.\web\tests\run_regressions.ps1
```

The script uses `D:\AIWorkspace\emsdk\emsdk_env.ps1` and its `em++.exe` (or `em++.bat`) and Node. Override `-EmsdkRoot`, `-BuildDirectory`, or `-BgmArchive` when needed. It only builds the two small probes, without rebuilding the game.

- **HUD:** compiles the production text formatter to WASM and runs 14 checks in Node, including grouped scores and their foreground/shadow queue entries.
- **Audio:** runs `web/audio_stream_test.cjs` against the original PCM in `build_web/game-data/thbgm.dat`. This verifies scheduled samples, ring refills, track loops, pause/resume, seeking, and autoplay resume using a mock AudioContext. The detailed output is saved to `reports/web_audio_stream_validation.json`.
- **Graphics:** compiles the actual `web/src/web_d3d9.cpp` with `render_probe.cpp`. Start `./serve_web.ps1` and open [the pixel probe](http://127.0.0.1:8123/render_probe.html). It should display **27/27 pixel checks passed**. This browser step is manual; compilation alone does not validate GPU output.

The pixel checks use real `glReadPixels` values for fog, view-space depth, blending, render-texture orientation, default-backbuffer alpha, destination-alpha transitions, and the opaque presentation boundary. DOM `data-result`, `data-passed`, and `data-total` expose the summary; `#results` contains every actual/expected RGBA value and GL error code.

These checks cover specific browser-port regressions. They do not establish complete visual/audio parity or full-game equivalence to the original.

## GitHub Pages resource downloads

Run `node web/tests/chunk_loader.cjs` from the repository root. This executes the actual production `web/game.js` in a Node VM with browser and fetch mocks, using Node Web Crypto for SHA-256. It adds only an export hook; the loader functions are not duplicated in the test.

The 19 checks cover exact chunk reassembly, nested Pages paths, cached manifests, checksum and size rejection, three-attempt retries, invalid paths, missing files, declared-length mismatches, and manifest failures. They also exercise the production no-manifest branch with both streaming and `arrayBuffer` downloads, including missing content length and HTTP errors. This test needs Node with `node:crypto.webcrypto`, no game assets, and no running web server.

## Sustained gameplay input

[The gameplay input probe](http://127.0.0.1:8123/gameplay_probe.html) uses the production `game.js`, `game.css`, and WASM module. Its buttons toggle held Z, left, right, and Shift keys, or release all latched keys. It changes only keyboard input and releases held keys on window blur or tab hiding. It uses the same browser save storage as the normal game page.

`run_regressions.ps1` copies the page automatically. To update only this page without any compilation, run:

```powershell
Copy-Item .\web\tests\gameplay_probe.html .\build_web\gameplay_probe.html
```
