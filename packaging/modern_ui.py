"""Original modern-cartoon UI for Doodle Hop 3DS, rendered offline at 4x.

build_ui(font_path) returns finished native-resolution Pillow images. Importing
this module never writes files or changes the generated game assets.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

S = 4
NAVY = '#172A43'
INK = '#253950'
WHITE = '#F8FCFF'


def _rgb(color):
    if isinstance(color, tuple):
        return color
    return tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))


def _mix(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(_rgb(a), _rgb(b)))


class Paint:
    def __init__(self, w, h, fill=(0, 0, 0, 0)):
        self.w, self.h = w, h
        self.im = Image.new('RGBA', (w * S, h * S), fill)

    @staticmethod
    def box(box):
        return tuple(round(v * S) for v in box)

    def round(self, box, radius, fill, outline=None, width=1):
        ImageDraw.Draw(self.im).rounded_rectangle(self.box(box), radius * S,
                                                  fill, outline, round(width * S))

    def line(self, points, fill, width=1):
        ImageDraw.Draw(self.im).line([(round(x * S), round(y * S)) for x, y in points],
                                    fill=fill, width=round(width * S), joint='curve')

    def ellipse(self, box, fill, outline=None, width=1):
        ImageDraw.Draw(self.im).ellipse(self.box(box), fill, outline, round(width * S))

    def gradient(self, box, radius, top, bottom):
        box = self.box(box)
        w, h = max(1, box[2] - box[0]), max(1, box[3] - box[1])
        layer = Image.new('RGBA', (w, h))
        draw = ImageDraw.Draw(layer)
        for y in range(h):
            draw.line((0, y, w, y), fill=_mix(top, bottom, y / max(1, h - 1)))
        mask = Image.new('L', (w, h))
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius * S, fill=255)
        self.im.paste(layer, box[:2], mask)

    def shadow(self, box, radius, alpha=70, blur=2, offset=2):
        shadow = Image.new('RGBA', self.im.size)
        shifted = (box[0], box[1] + offset, box[2], box[3] + offset)
        ImageDraw.Draw(shadow).rounded_rectangle(self.box(shifted), radius * S,
                                                 fill=(13, 31, 51, alpha))
        self.im = Image.alpha_composite(self.im, shadow.filter(ImageFilter.GaussianBlur(blur * S)))

    def card(self, box, radius=14, top='#FFFFFF', bottom='#EDF4F8', shadow=70):
        self.shadow(box, radius, shadow, 2, 2)
        self.gradient(box, radius, top, bottom)
        self.round((box[0] + .5, box[1] + .5, box[2] - .5, box[3] - .5),
                   max(1, radius - .5), None, (255, 255, 255, 210), .7)

    def finish(self, mode='RGBA'):
        return self.im.resize((self.w, self.h), Image.Resampling.LANCZOS).convert(mode)


def _arrow(p, x, y, direction):
    # Rounded, single-stroke icons remain legible at original 3DS resolution.
    col = '#2B8C86'
    p.line([(x - 12 * direction, y), (x + 11 * direction, y)], col, 4)
    p.line([(x + 3 * direction, y - 9), (x + 12 * direction, y),
            (x + 3 * direction, y + 9)], col, 4)


def _base():
    p = Paint(320, 240)
    p.gradient((0, 0, 320, 240), 0, '#203854', '#14283D')
    # Small edge accents support hierarchy without a full-screen pattern.
    p.round((278, 13, 302, 18), 2.5, '#49C9BE')
    p.round((286, 23, 302, 27), 2, '#EF8D89')
    p.line([(18, 53), (302, 53)], '#39546B', .7)
    return p


def _button(w, h, primary):
    p = Paint(w, h)
    if primary:
        p.card((1, 1, w - 1, h - 3), 14, '#60D8CA', '#24AFA9', 90)
        p.line([(20, 5), (w - 20, 5)], '#A2F0DD', .8)
        # Tiny navigation glyph supports the label without acting as a new hotzone.
        p.line([(w - 24, h / 2 - 4), (w - 20, h / 2), (w - 24, h / 2 + 4)], '#E9FFF7', 1.6)
    else:
        p.card((1, 1, w - 1, h - 3), 10, '#FFFFFF', '#DFEAF2', 65)
        p.line([(14, 4), (w - 14, 4)], '#FFFFFF', .8)
    return p.finish()


def _panel():
    p = Paint(280, 150)
    p.card((1, 1, 279, 147), 12, '#FFFFFF', '#EEF4F9', 80)
    return p.finish()


def _hud():
    p = Paint(120, 24)
    p.shadow((1, 1, 119, 21), 8, 80, 1, 1)
    p.gradient((1, 1, 119, 22), 8, '#354E68', '#20374F')
    p.round((1.5, 1.5, 118.5, 21.5), 7.5, None, '#58748A', .7)
    p.line([(12, 3), (108, 3)], '#6A879D', .6)
    return p.finish()


def _logo(font_path):
    p = Paint(260, 70)
    font = ImageFont.truetype(str(font_path), 51 * S)
    try:
        font.set_variation_by_axes([900])
    except (OSError, AttributeError):
        pass
    label = '涂鸦跃界'
    bbox = font.getbbox(label)
    x = (260 * S - (bbox[2] - bbox[0])) // 2 - bbox[0]
    y = 3 * S - bbox[1]
    # Layered extrusion rather than a heavy vintage stroke.
    draw = ImageDraw.Draw(p.im)
    for offset in range(5 * S, 0, -1):
        draw.text((x, y + offset), label, font=font, fill='#174F63',
                  stroke_width=2 * S, stroke_fill='#174F63')
    draw.text((x, y), label, font=font, fill='#F7FFFF', stroke_width=2 * S,
              stroke_fill='#35BDB7')
    draw.text((x, y), label, font=font, fill='#F7FFFF', stroke_width=0)
    # Small coral/teal confetti is separate from the letter silhouette.
    p.ellipse((8, 14, 13, 19), '#F49C92')
    p.line([(246, 17), (250, 21), (246, 25)], '#81E7CC', 2)
    p.round((106, 62, 154, 65), 1.5, '#5FD8C5')
    return p.finish()


def build_ui(font_path):
    """Return the five RGBA sprites and two RGB backgrounds in the UI contract."""
    font_path = Path(font_path)
    if not font_path.is_file():
        raise FileNotFoundError(font_path)
    menu = _base()
    menu.card((12, 57, 308, 216), 20, '#EEF5F9', '#DBE8F0', 90)
    controls = _base()
    controls.card((12, 66, 224, 216), 17)
    for x in (27, 131):
        controls.card((x, 107, x + 78, 172), 15, '#E3F4F1', '#CDE8E4', 40)
    _arrow(controls, 66, 138, -1)
    _arrow(controls, 170, 138, 1)
    controls.round((39, 203, 197, 207), 2, '#BDD2DA')
    controls.round((40, 203, 196, 204), .5, '#E8F2F7')
    controls.card((232, 66, 308, 160), 16, '#FFB09A', '#EE7E88', 80)
    # Fire icon: a simple launch marker, distinct from the left/right controls.
    controls.line([(270, 98), (270, 82)], '#FFF9F3', 3)
    controls.line([(263, 89), (270, 81), (277, 89)], '#FFF9F3', 3)
    controls.round((265, 100, 275, 102), 1, '#FAD0C6')
    controls.card((232, 172, 308, 216), 13, '#FFFFFF', '#E2EDF4', 70)
    controls.round((240, 188, 243, 199), 1, '#7894A8')
    controls.round((246, 188, 249, 199), 1, '#7894A8')
    return {'ui_primary': _button(272, 46, True),
            'ui_secondary': _button(130, 34, False),
            'ui_panel': _panel(), 'hud_plate': _hud(),
            'title_logo': _logo(font_path),
            'bg_bottom': menu.finish('RGB'), 'bg_controls': controls.finish('RGB')}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('font', type=Path)
    parser.add_argument('--out', type=Path, default=Path(__file__).resolve().parent.parent / 'art/ui-draft')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    assets = build_ui(args.font)
    for name, im in assets.items():
        im.save(args.out / f'{name}.png')
    # At-native-size overview: the same exact bitmap data used by the exporter.
    contact = Image.new('RGB', (720, 390), '#30445A')
    contact.paste(assets['bg_bottom'], (20, 120))
    contact.paste(assets['bg_controls'], (380, 120))
    for name, xy in [('title_logo', (20, 14)), ('ui_primary', (380, 20)),
                     ('ui_secondary', (380, 75)), ('hud_plate', (530, 80))]:
        contact.paste(assets[name], xy, assets[name])
    for name, xy in [('ui_primary', (44, 182)), ('ui_secondary', (44, 236)),
                     ('ui_secondary', (186, 236))]:
        contact.paste(assets[name], xy, assets[name])
    # Layout demonstration labels are intentionally absent from compiled sprites.
    body = ImageFont.truetype(str(args.font), 14)
    overlay = ImageDraw.Draw(contact)
    for text, xy, fill in [('幻境漫游', (38, 130), '#FFFFFF'),
                           ('向新世界出发', (38, 152), '#BDD1DF'),
                           ('冒险控制台', (398, 130), '#FFFFFF'),
                           ('滑动移动，按住 A 发射', (398, 152), '#BDD1DF')]:
        overlay.text(xy, text, font=body, fill=fill)
    for text, xy in [('开始冒险 A', (142, 195)), ('怎么玩 Y', (80, 243)),
                     ('音效：开 X', (213, 243)), ('滑动控制方向', (458, 197)),
                     ('向左', (429, 298)), ('向右', (533, 298)),
                     ('发射 A', (631, 232)), ('长按', (639, 255)), ('暂停', (638, 307))]:
        overlay.text(xy, text, font=body, fill=INK)
    contact.save(args.out / 'ui-contact.png')
    print(f'Wrote {len(assets)} UI assets and ui-contact.png to {args.out}')
