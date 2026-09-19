# WebAssembly 移植状态

此目录是 `th20_cpp_source_20260919.zip` 的独立副本。Windows 源工程保留不动，浏览器构建位于 `web/`。

## 已接入 WASM 的代码

- THA1 归档头解析、目录解密、成员解密与 LZSS 解压使用 `source_reconstruction/archive/archive.cpp` 的恢复实现。
- 浏览器页面能够载入用户选择的 `th20.dat`，在 WASM 内解析真实资源目录并显示条目。
- 构建使用 wasm32、严格浮点选项、C++ 异常和可增长内存；不读取、映射或执行原版 EXE。

## 尚未完成的平台替换

- Direct3D 9 固定管线到 WebGL 2 的渲染后端。
- DirectSound 到 WebAudio 的音乐、音效和流式播放后端。
- DirectInput/XInput 到 KeyboardEvent/Gamepad API 的输入后端。
- Win32 窗口、计时、文件与 GDI 字形栅格化到浏览器 API 的适配。

当前 WASM 产物是可执行的资源运行时，不是完整可玩版本。只有上述平台后端接通并完成关卡与录像逐帧验证后，才能把它标记为可玩的完整移植。

## 构建

```powershell
.\build_web.ps1
.\run_web.ps1
```

打开页面后选择正版游戏目录中的 `th20.dat`。资源不会上传。
