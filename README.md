# Touhou 20 WebAssembly

由恢复的 C++ 游戏逻辑和浏览器平台适配代码构建的 WebAssembly 移植工程。

**网页版：<https://oracatt.github.io/touhou20-web/>**

网页使用键盘：方向键移动，Z 确认/射击，X 取消/符卡，Shift 低速，Esc 暂停，Alt+Enter 全屏。首次启动需下载约 554 MiB 游戏数据。点击画面或按键后浏览器才能播放音频。建议在桌面浏览器运行。

## 工程结构

- `source_reconstruction/`、`native_recovered/`：恢复的 C++ 模块。
- `src/`、`include/`：独立资源及脚本解析工具。
- `web/`：WebGL、WebAudio、输入及其他浏览器适配。
- `scripts/recovered/`：恢复的游戏脚本。
- `docs/`：GitHub Pages 发布产物，包括 WASM 和游戏数据分块。
- `tools/prepare_github_site.py`：静态发布打包工具。

网页运行不需要原游戏 EXE。原游戏内容的权利归各自权利人；本仓库没有另行授予这些内容的使用许可。

## 本地构建与发布

完整本地环境说明见 [WEB_README.md](WEB_README.md)。在 Windows PowerShell 中：

```powershell
.\build_web.ps1 -EmsdkRoot D:\AIWorkspace\emsdk -GameDirectory '原游戏资源目录'
python tools/prepare_github_site.py
.\serve_web.ps1
```

发布目录以 `game.html` 作为 `index.html`。两份大资源按 32 MiB 分块，浏览器逐块下载并进行 SHA-256 校验，再恢复原字节。`docs/` 提交到 `main` 后，GitHub Pages 从 `main /docs` 自动更新。更新 WASM 前重新运行构建和发布脚本，保持 JS/WASM 与清单匹配。

## 验证范围

已在浏览器中验证标题与选择菜单、第一关、进关转场、背景雾、分数、BGM 连续播放，以及首关 Boss 符卡背景和扭曲效果。回归检查覆盖 27 项 WebGL 像素、14 项分数和跨原曲循环点的音频 PCM。

全部关卡与同输入、同种子的逐帧一致性尚未完成。详见 [验证记录](WEB_VISUAL_VALIDATION.md)、[特效覆盖](web/EFFECTS_COVERAGE.md) 和 [测试运行方式](web/tests/README.md)。
