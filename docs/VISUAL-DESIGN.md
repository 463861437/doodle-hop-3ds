# 1.3.0 现代卡通美术

本版保留狐狸与三个幻想世界，将画风改为圆润的现代卡通。角色穿青蓝外套、珊瑚围巾和运动鞋，场景具有真实模型体积、面积光、材质高光和柔阴影。界面改用深蓝底、奶白卡片、青绿主按钮与珊瑚发射键。

## 离线三维到 3DS 图片

`packaging/render-modern-3d.py` 使用 Blender/bpy 4.5.3 创建原创网格、倒角、曲线、材质、灯光与正交相机，以 Cycles 32 samples 渲染。没有外部模型或商业贴图依赖。

`art/modern-3d/` 包含渲染后的原始 PNG：狐狸五姿态、标题角色、四张场景及十二种平台。`packaging/generate-assets.py` 将它们缩到实际分辨率，加入原创小型物件与 `modern_ui.py` 生成的界面，输出头文件和 `ASSET-MANIFEST.json`。

这是预先渲染的三维美术。3DS 运行时仍绘制 2D 图片，不运行 Blender、实时模型或路径追踪，也没有裸眼立体输出。所有主要图片随源码提供，普通编译无需安装美术工具。

## 画面结构

| 高度 | 主题 | 视觉构成 |
| --- | --- | --- |
| 0～1999 | 翡翠浮岛 | 圆冠树、灯塔、连接桥、水溪与蘑菇植物 |
| 2000～4999 | 珊瑚天空城 | 圆顶建筑、玻璃高塔、悬浮地块与空中轨道 |
| 5000 及以上 | 极光晶谷 | 晶簇、环形遗迹、月球、远岛与极光 |

大块景观分布在两侧与远处，中部留给角色、道具和可落脚平台。三种主题分别建模；平台表面用不同材质区分类型，并保持清楚的水平落脚边界。

## 保持手感与运行开销

- 物理世界仍宽 280 单位，上屏统一投影至 400 像素；高度坐标和碰撞不变。
- 狐狸图片 40×44，脚底锚点 (20,40)；平台图片 80×24，中央落脚线 y0。
- 背景预编码为原生 BGR8 列排列。运行时复制背景和透明精灵，不逐帧计算复杂光照。
- 下屏保留静态缓存，只叠加触摸反馈；热区、按键与存档路径不变。
- 落地短光圈与喷气尾光只使用少量绘制，不参与物理计算。

文字与按钮底色已检查对比度，并保留清晰的主次操作。完成渲染安全、像素参考和桌面性能测试后再打包；这些检查不代表真机帧率，实测边界见 [VALIDATION.md](VALIDATION.md)。

## 复现

```sh
python3.11 -m venv .venv-art
.venv-art/bin/python -m pip install -r packaging/requirements-art.txt
.venv-art/bin/python packaging/render-modern-3d.py --only all --samples 32
.venv-art/bin/python packaging/generate-assets.py /absolute/path/to/NotoSansSC.ttf
.venv-art/bin/python packaging/generate-font.py /absolute/path/to/NotoSansSC.ttf
python3 tests/make-preview.py
python3 tests/make-motion.py
```

预览由实际 C++ 渲染器生成，静态画面为摆放好的演示场景；动态预览运行真实游戏逻辑与自动转向。两者都不是设备截图。

[与 1.2.0 对比](visual-comparison.png) · [上一版设计记录](VISUAL-DESIGN-1.2.0.md)
