# WebAssembly 移植版

本目录是 `D:\AIWorkspace\Touhou20Reconstruction` 的独立副本，网页构建和移植修改保存在 `D:\AIWorkspace\Touhou20WebWasm`。游戏由恢复的 C++ 和浏览器平台代码编译为 WASM，读取原版 `th20.dat`、`thbgm.dat` 资源；网页运行不需要原版 EXE。

## 构建

在本目录打开 PowerShell，运行：

```powershell
.\build_web.ps1
```

默认使用 `D:\AIWorkspace\emsdk` 中已激活的 Emscripten，另需 CMake 和 Ninja。脚本兼容 SDK 的 `emcmake.exe` 和旧版 `emcmake.bat`。可通过 `-EmsdkRoot`、`-GameDirectory`、`-Configuration`、`-Parallel` 修改设置；默认设置 `BINARYEN_CORES=1`，控制最终链接时的内存占用，可用 `-BinaryenCores` 调整。脚本显式构建游戏和资源浏览器两个目标，并使用 EMSDK 自带的 Node 对真实资源档案进行 WASM 解包测试。

原资源只读取，复制到 `build_web\game-data`。同大小副本会跳过复制；若要强制替换资源，先移走该副本。资源已经复制到构建目录后，即使原资源路径不可用，仍可重新构建和测试。

## 启动

```powershell
.\serve_web.ps1
```

然后打开 <http://127.0.0.1:8123/game.html>。需 Python 3；可用 `-Python` 指定解释器。服务只监听本机，关闭终端或按 Ctrl+C 停止。`-Port` 可修改端口，`-OpenBrowser` 可自动打开浏览器。不能通过双击 HTML 以 `file://` 方式运行。

首次载入约 554 MiB 原版资源，页面会显示进度。建议使用支持 WebGL 2 的桌面 Edge 或 Chrome。点击画面后使用方向键移动、Z 确认/射击、X 取消/符卡、Shift 低速、Esc 暂停；Alt+Enter 切换全屏。浏览器存档和配置位于该站点的本地存储中。

主要产物：

- `build_web\game.html`、`th20_game.js`、`th20_game.wasm`：游戏运行页和编译产物。
- `build_web\index.html`：原资源档案浏览页。
- `build_web\WEB_BUILD_VERIFICATION.json`：构建目标、产物哈希、真实资源测试结果及验证范围。

## 当前验证范围

人工浏览器检查已确认画布为 1280×960、画面以 4:3 比例铺满，标题背景恢复；标题、难度、角色、石头选择菜单可进入。进关加载按帧推进，游戏逻辑实测约 60 帧/秒。

音频与特效修复后重新编译，实际观察到进关面板转场、第一关雾效、中 Boss 背景扭曲、正式 Boss 第一张符卡背景和右上角分数。标题 BGM 在浏览器中连续推进超过 26 秒，关卡 BGM 超过 44 秒，诊断未见欠载。独立回归包含 14 项分数检查、27 项实际 WebGL 像素检查，以及跨原曲循环点的 PCM 逐采样校验。具体结果和验证边界见 [WEB_VISUAL_VALIDATION.md](WEB_VISUAL_VALIDATION.md)，效果调用审计见 [web/EFFECTS_COVERAGE.md](web/EFFECTS_COVERAGE.md)，测试复现见 [web/tests/README.md](web/tests/README.md)。

以上检查不代表完整通关或 1:1 验收。构建脚本自动验证的是编译、链接和真实资源解析，不会自动验证菜单或战斗画面。

原版同输入、同种子的逐帧状态对照、全部关卡、视觉与音频一致性尚未完成。因此本目录是可继续开发的 WASM 移植工程，不能当作已完成全部源码恢复和 1:1 还原的证明。
