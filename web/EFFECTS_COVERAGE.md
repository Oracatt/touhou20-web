# WebGL 特效覆盖审计

审计日期：2026-09-19。范围为当前独立 C++/WASM 工程的资源清单、生产渲染调用和 WebGL 兼容层。此文件记录**源码与 API 覆盖**；不把编译成功、进入第一关或少量截图当作全部关卡视觉一致性的证明。

## 原版效果如何进入浏览器

原资源清单 [assets_recovery.json](../reports/assets_recovery.json) 有 283 个不同名称的文件，扩展名是 ANM、ECL、FMT、MSG、PNG、RPY、SHT、STD、TXT、VER、WAV；未发现单独的 HLSL、FX、CSO 或其他 shader 文件。已恢复的生产渲染源码没有 `CreatePixelShader`、`CreateVertexShader` 或 `SetPixelShader` 调用；[controller.cpp](../source_reconstruction/sprite_renderer/controller.cpp) 设置的是 `SetVertexShader(nullptr)`。

因此当前有证据的原效果路径是 D3D9 固定管线：ANM/ECL/STD 驱动精灵、顶点颜色、几何体、混合、雾和离屏纹理。本移植在 [web_d3d9.cpp](src/web_d3d9.cpp) 使用 GLSL 实现这些状态。这个结论限定于已审计的资源及恢复源码，不能据此宣称原 EXE 的全部路径已验证。

## API 和效果路径

| 原调用或效果 | 浏览器实现及证据 | 静态覆盖状态 |
| --- | --- | --- |
| FVF `0x44` | XYZRHW + 顶点颜色；彩色线条、环、矩形、遮罩 | 已处理 |
| FVF `0x144` | XYZRHW + 顶点颜色 + UV；精灵批次、扭曲条带 | 已处理 |
| FVF `0x102` | XYZ + UV；world/view/projection 和纹理坐标矩阵 | 已处理 |
| FVF `0x142` | XYZ + 顶点颜色 + UV；三维彩色条带 | 已处理 |
| 图元 | 生产调用出现 POINTLIST、LINESTRIP、TRIANGLELIST、TRIANGLESTRIP、TRIANGLEFAN；兼容层另支持 LINELIST | 拓扑及顶点数均有分支 |
| 纹理组合 | 实际使用 SELECTARG1、SELECTARG2、MODULATE，输入为纹理、DIFFUSE 或 TFACTOR；RGB 与 alpha 分开组合 | 已处理 |
| 纹理坐标 | TEXTURETRANSFORMFLAGS=COUNT2，XYZ 顶点应用纹理矩阵；已变换的 XYZRHW 顶点保留原 UV | 已处理 |
| 采样 | point/linear；wrap/mirror/clamp | 已处理 |
| 混合 | 原动画模式 0—9 的源/目标因子和 ADD、REVSUBTRACT、MIN、MAX；独立 alpha 因子与方程 | 已处理，模式表见下文 |
| alpha 测试 | enable/ref/function，按 8 位 alpha 比较 | 已处理 |
| 深度 | enable/write/function、viewport MinZ/MaxZ、深度清除 | 已处理；精度与跨目标深度行为未做原版逐像素对照 |
| 离屏合成 | 两张表面纹理、遮罩 alpha、阶段合成、最终游戏区拷贝；切目标时重置 viewport | 已处理 |
| 雾 | FOGENABLE/COLOR/START/END/DENSITY/VERTEXMODE/TABLEMODE/RANGEFOGENABLE | 本轮补齐，详见下文 |
| 屏幕渐变/闪光 | [screen_effect/draw.cpp](../source_reconstruction/screen_effect/draw.cpp) 的纯色四边形、指定范围、顶点 alpha | 源码路径及所用 API 已覆盖 |
| 圆环/粒子/尾迹 | [effect_system](../source_reconstruction/effect_system/README.md) 的 15 个有效 descriptor 初始化器及各自 update/draw | 源码存在且在完整游戏目标中编译；并非全部实机触发验证 |
| 标题、石头阶段、敌方画面扭曲 | `title_system/background.cpp`、`overlay_system/visuals.cpp`、`gameplay/enemy_mesh_adapter.cpp` 修改网格后发布条带，再用真实离屏纹理绘制 | 源码路径及所用 API 已覆盖；全部效果阶段待逐项视觉对照 |

动画绘制的完整分发在 [sprite_renderer/draw.cpp](../source_reconstruction/sprite_renderer/draw.cpp)。模式 10 和 23 保留原分支直接返回行为，不应凭“没有画东西”添加猜测效果。其他 0—48 范围内的实际分支分派到已恢复的精灵、投影、彩色几何和条带函数。

以下是 [render_state.cpp](../source_reconstruction/sprite_renderer/render_state.cpp) 实际配置的动画混合模式，兼容层有对应因子和方程：

| 模式 | 源 RGB 因子 | 目标 RGB 因子 | 方程 |
| --- | --- | --- | --- |
| 0 | SRCALPHA | INVSRCALPHA | ADD |
| 1 | SRCALPHA | ONE | ADD |
| 2 | SRCALPHA | ONE | REVSUBTRACT |
| 3 | ONE | ZERO | ADD；关闭 alpha 测试 |
| 4 | INVDESTCOLOR | INVSRCCOLOR | ADD |
| 5 | DESTCOLOR | ZERO | ADD |
| 6 | INVSRCCOLOR | INVSRCALPHA | ADD |
| 7 | DESTALPHA | INVDESTALPHA | ADD |
| 8 | SRCALPHA | ONE | MIN |
| 9 | SRCALPHA | ONE | MAX |

## 本轮确定的缺失：雾

此前 `SetRenderState` 静默忽略所有雾状态，而原版初始化启用线性顶点雾（FOGVERTEXMODE=3，FOGTABLEMODE=0）。[stage_background/draw.cpp](../source_reconstruction/stage_background/draw.cpp) 每次背景绘制更新颜色、起点和终点，背景与动画模式 15/48 会启用雾。因而这是实际效果缺项，而不是资源没有载入。

本轮兼容层从 world/view 变换后的相机空间深度计算顶点雾系数，再由 GLSL 插值，在片元阶段只混合 RGB，保留 alpha。XYZRHW 精灵不按屏幕深度重新计算顶点雾。实现也接收指数/平方指数和 range 参数；当前生产初始化使用的是线性顶点雾，不能把其他分支存在当作已在全部关卡实测。

语义依据：[Microsoft Vertex Fog](https://learn.microsoft.com/en-us/windows/win32/direct3d9/vertex-fog)、[Fog Parameters](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-parameters)。官方说明顶点雾在顶点计算并插值，起止参数使用相机空间单位；已变换顶点的雾由应用提供。

清晰的复测位置是 [st01.std.txt](../scripts/recovered/std/st01.std.txt) 的 `SCRIPT`：初始 `ins_8` 设置颜色 `#a0c0c0ff`、起点 200、终点 800，随后 740 个背景脚本帧渐变到 `#607010ff`、起点 400、终点 1400。可在相同游戏帧、相同输入/种子下比较远景颜色与轮廓消失范围。

## 进关转场与后备缓冲 alpha

原转场并没有被省略为一个瞬间场景切换。[title_system/loadout.cpp](../source_reconstruction/title_system/loadout.cpp) 在阶段 3 的第 10 帧调用加载转场，第 40 帧才请求进入游戏。`loadout_environment.cpp` 创建 EffectInf descriptor 0，并发送事件 7。它绑定原 [screenswitch.anm](../scripts/recovered/anm/screenswitch.anm.txt) 的面板脚本 3—6 和遮罩脚本 11；[gameplay/activation.cpp](../source_reconstruction/gameplay/activation.cpp) 在游戏首帧发事件 1，改用退场脚本 7—10。面板的位置、角度、延时和插值仍来自这些脚本。

本轮发现另一项确定的渲染错误：WebGL 原来以 `alpha=false` 创建后备缓冲，却向游戏报告 `D3DFMT_A8R8G8B8`。转场根动画是 secondary 列表中的 layer 52，经 [dispatch.cpp](../source_reconstruction/sprite_renderer/dispatch.cpp) 映射为 layer 47，在绘制优先级 77 执行；这时优先级 66/67 已经恢复并合成后备缓冲。故转场遮罩实际需要**后备缓冲自己的目标 alpha**。原 `alpha=false` 使目标 alpha 无法保存，原脚本 11 的 DESTALPHA/INVDESTALPHA 混合失效。

修复要求是保留绘制过程中的 RGBA 后备缓冲，采用 `alpha=true`、`premultipliedAlpha=false`，并单独处理最终页面展示的不透明性；不能在转场绘制中提前把 alpha 固定为 1。此问题的像素回归由 GPU 测试覆盖，不能仅靠回调执行次数判定画面修复。

## Boss 符卡和局部扭曲触发

- [st01bs.ecl.txt](../scripts/recovered/ecl/st01bs.ecl.txt) 的 `Boss` 在第 1 帧执行 `ins_621(160.0f, 15732608)`；第一关中 Boss 相应半径是 128。`enemy_shot.cpp` 的 opcode 621 经 `enemy_shot_adapter.cpp` 创建真实 17×17 网格，`enemy_update.cpp` 每帧调用 `enemy_mesh.cpp`。该函数按原算法改动颜色、位置和 UV，最后发布条带。这里不是待补的屏幕 shader，也不应替换成自制正弦滤镜。
- 符卡启动由 `enemy_opcode_state.cpp` 调用 [card_system/start.cpp](../source_reconstruction/card_system/start.cpp)：生成符卡名称/信息动画、effect.anm 脚本 6 和 13，并按原 StageDefinition 选择背景与人物动画。游戏激活时在 `activation.cpp` 启用 CardInf 回调。当前加载协程在启动关卡更新之前等待阶段 ANM 模板完成；本轮未发现需要绕过原脚本或延长原符卡时间的依据。
- 发现并修复了额外的 WASM 间接调用签名错误：`enemy_shot_adapter.cpp` 中矩形时缓与恢复时缓回调的第二个原生 fastcall 参数仅是 EDX 占位，但 `enemy_update.cpp` 实际按一个 EnemyState 参数调用。WASM 分支现在使用一个参数，与调用点一致；原 x86 分支保留原适配签名。原资源 `st07bs.ecl.txt` 确实使用 `ins_624(1)` 和 `ins_624(2)`，所以这不是只会出现在假测试里的路径。修复保留回调原来的时缓与颜色行为。

为实际验证加入了 `TH20_WEB` 专用、只读的低频 DOM 记录，不增加调试 UI，也不修改原对象布局或游戏状态：

| `document.documentElement.dataset` 字段 | 记录内容 |
| --- | --- |
| `th20TransitionStarts/Frame/Closing/Status` | 原转场创建、更新、退出状态；更新每 8 帧采样 |
| `th20TransitionDraws/LastDrawFrame` | 原转场 draw 路径的累计次数与采样帧 |
| `th20StageFrame/GameFlags` | 关卡时钟和已有标志，每 60 帧采样 |
| `th20SpellStarts/Id/Active/BackgroundHandle/EffectHandle` | 符卡原入口与背景/效果句柄 |
| `th20SpellAge/Flags` | 活跃符卡的原计时与标志，每 30 帧采样 |
| `th20EnemyMeshUpdates/Radius/Strips` | 原敌方扭曲网格的更新、半径与条带数，每 60 次更新采样 |

这些值证明对应运行路径是否执行，不能代替像素截图或原版一致性比较。

## 审计中没有贸然改动的项目

`stage_background/mesh_distortion.cpp` 在初始化条带之后修改 `mesh.vertices`，末尾没有再次发布条带。原函数 `0x004722e0` 的反编译证据也没有 `0x0049ddc0` 调用，当前资源的 STD 文本未出现 `ins_17`。单凭源码观感不能把此处改成新效果；应先确定原调用链和触发场景。

兼容层的若干不影响已观察调用的状态由默认行为满足：LIGHTING=0、CULLMODE=NONE、SHADEMODE=GOURAUD、MULTISAMPLEANTIALIAS=0，以及单张二维纹理的 TEXCOORDINDEX=0、无 mipmap。本文不宣称兼容层实现了完整 D3D9 标准。

## 后续视觉验收清单

以下是验收范围，**不是已通过记录**：

1. 同输入/种子检查第一关初始、740、2652、4132 等背景帧，核对雾渐变、前景层和背景层遮挡。
2. 标题背景波动、开始游戏与返回标题时的转场遮罩，核对 alpha 累积与离屏取样边界。
3. 各角色和石头配置的射击、符卡、阶段开始/结束、死亡/复活，核对尾迹、环、粒子、画面扭曲及闪光的范围和持续帧数。
4. 各关中 Boss、Boss、符卡切换与全屏效果，核对混合模式，尤其反向减法、MIN/MAX 和目标 alpha。
5. 不同窗口倍率与全屏，核对 atlas 边缘、单像素点线、纹理过滤、深度精度以及文本图层。

完整通关视觉对照、音画同步、相同输入/种子的逐帧状态与截图对照尚未完成。源码/API 静态覆盖只能缩小遗漏范围，不能证明所有特效已经与原版 1:1。

### 本轮已执行的浏览器检查

重新编译完整游戏后，实际捕捉到进入第一关的面板转场、远景雾与分数文字；自然运行到中 Boss 和正式 Boss 后，背景扭曲网格分别以半径 128 和 160 持续更新。正式 Boss 第一张符卡（ID 0）的原背景、名称、倒计时、弹幕与圆环可见，符卡年龄推进至 1,920 帧，期间未观察到浏览器 error / warning。实际 WebGL 像素回归为 27/27 通过。详情与数值见 [WEB_VISUAL_VALIDATION.md](../WEB_VISUAL_VALIDATION.md)；本段不将这些结果扩展为其余关卡的逐帧验收。
