# 1.3 原创离线 3D 素材

这些 PNG 由 `packaging/render-modern-3d.py` 用 Blender 4.5.3 LTS 的 Python API 建模、Cycles CPU 渲染，再用 Pillow 排版到约定尺寸。全部人物、岛屿、树木、灯塔、房屋、玻璃塔、飞船、水晶、遗迹和平台均由脚本中的实际三维网格、曲线和材质构成；天空渐变和极光为离线合成。没有使用商业游戏贴图、下载模型或外部图片。

## 复现

在项目根目录运行，环境需 Python 3.11、`bpy==4.5.3`、Pillow：

```sh
python3.11 -m venv .venv-art
.venv-art/bin/python -m pip install -r packaging/requirements-art.txt
.venv-art/bin/python packaging/render-modern-3d.py --only all --samples 32
```

`--only preview` 只生成狐狸和森林早审；`--only hero`、`backgrounds`、`platforms` 可分别重建对应资源。`--width 800` 控制场景宽度，高度自动取 0.6 倍。脚本固定构图、材质与随机种子；不同系统的渲染器或图像库可能存在细微像素差异。

## 图像合同

- `hero_right/rise/fall/blink/jet.png`：160×176，straight RGBA，小狐狸投影脚中心对齐 x=80，实心脚底终止于 y=159，底部留透明区域。主整合脚本缩为 40×44；左朝向由镜像生成。
- `title_hero.png`：480×560，straight RGBA。
- `bg_day/bg_dusk/bg_night/bg_title.png`：800×480 RGB。主整合脚本缩为上屏 400×240。三场景分别为翠绿云海浮岛、珊瑚空中城市、极光水晶遗迹。
- 12 张 `platform_*.png`：320×96，straight RGBA。主整合脚本缩为 80×24；最上方中央落脚线贴齐 y=0，圆角与悬挂装饰位于其下。

渲染中间文件保存在 `build-host/modern-art-render/`，此目录只保留游戏最终输入素材与本说明。3DS 运行时无需 Blender、三维模型加载或动态 3D 渲染，最终图片将烘焙到现有静态像素资源头文件。
