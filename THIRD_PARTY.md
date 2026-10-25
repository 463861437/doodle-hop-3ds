# 许可与来源

项目源码、角色、图标、平台图像、界面及合成音效按 **GPL-3.0-or-later** 发布，完整文本见 `LICENSE`。原生基础代码参考或改编自同一工作区的 Many Bricks 3DS 与 Neon Snake 3DS，二者同为 GPL-3.0-or-later。

本项目重新编写跳跃、平台、道具与程序生成规则。1.3.0 的狐狸、平台、浮岛、天空城和晶谷由项目脚本原创建模，通过 Blender 离线渲染；现代界面和小型图标由 Pillow 生成。模型、图片与脚本均随源码提供，未使用商业原作或外部贴图库。未使用 Doodle Jump 的源码、安装包、角色图像或音乐。Doodle Jump 和 Lima Sky 的名称用于说明玩法参考，不表示关联或授权。Nintendo / 3DS 为其权利人的名称。

## 字体

游戏字形、图标和横幅文字来自 **Noto Sans SC**，采用 **SIL Open Font License 1.1**，见 `packaging/FONT-LICENSE.txt`。字库只包含用到的字符。来源：[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)。

## 运行库与工具

- libctru：zlib 许可证，见 `packaging/licenses/libctru-LICENSE.txt`；[官方源码](https://github.com/devkitPro/libctru)。
- devkitARM 的 libgcc / libstdc++：适用相应 GPL 及 GCC Runtime Library Exception 3.1，例外文本见 `packaging/licenses/GCC-RUNTIME-EXCEPTION.txt`。
- newlib：组成部分许可见 `packaging/licenses/newlib-COPYING.txt`。
- makerom / ctrtool：[Project_CTR](https://github.com/3DSGuy/Project_CTR)，仅为外部构建／检查工具，不随项目分发。
- bannertool：[官方源码](https://github.com/Steveice10/bannertool)，仅为外部构建工具。
- Pillow：只用于生成资源和预览，不参与 3DS 运行，不随项目分发。
- Blender / bpy 4.5.3：GPL-3.0，外部离线美术工具，不随项目分发、也不在 3DS 上运行；[官方 Python 模块](https://pypi.org/project/bpy/4.5.3/)。

发行时应同时提供对应源码、构建信息和上述许可文件；发行脚本会生成安装包与源码包。
