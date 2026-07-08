#!/usr/bin/env python3
"""Assemble Keynote.pptx : fonds PNG plein écran + zones de texte natives.

Source de vérité des zones : le manifeste SLIDES ci-dessous. Les rectangles
"px" doivent rester identiques aux divs .native des slideNN.html.
Conversions : 1 px design = 6350 EMU ; pt natif = px / 2.
"""
from pathlib import Path

from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.text import MSO_ANCHOR, PP_ALIGN
from pptx.util import Emu, Pt

HERE = Path(__file__).parent
OUT = HERE / "out"
TARGET = HERE.parent / "Keynote.pptx"

EMU_PER_PX = 6350          # 12192000 EMU / 1920 px
SLIDE_W = 12192000         # 16:9
SLIDE_H = 6858000

ALIGN = {"left": PP_ALIGN.LEFT, "center": PP_ALIGN.CENTER, "right": PP_ALIGN.RIGHT}

SLIDES = [
    {
        "n": "01",
        "zones": [
            {"px": (310, 640, 1300, 60), "pt": 20, "align": "center",
             "text": "Une course d’intelligences artificielles sur un monde-anneau"},
            {"px": (210, 950, 1500, 44), "pt": 14, "align": "center", "color": "8E9BC0",
             "text": "Raphael Dumon · Clément Fabre · Mathéo Emma · Léo Cazabat · Joachim Ismael"},
        ],
    },
]


def add_zone(slide, zone):
    x, y, w, h = (Emu(v * EMU_PER_PX) for v in zone["px"])
    box = slide.shapes.add_textbox(x, y, w, h)
    tf = box.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = MSO_ANCHOR.TOP
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    lines = zone["text"].split("\n")
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = ALIGN[zone.get("align", "left")]
        if i > 0:
            p.space_before = Pt(zone.get("gap_pt", 8))
        run = p.add_run()
        run.text = line
        f = run.font
        f.name = "Arial"
        f.size = Pt(zone["pt"])
        f.bold = zone.get("bold", False)
        f.color.rgb = RGBColor.from_string(zone.get("color", "EAF0FF"))


def main():
    prs = Presentation()
    prs.slide_width = Emu(SLIDE_W)
    prs.slide_height = Emu(SLIDE_H)
    blank = prs.slide_layouts[6]
    for spec in SLIDES:
        png = OUT / f"slide{spec['n']}.png"
        if not png.exists():
            raise SystemExit(f"fond manquant : {png} (lancer render.sh)")
        slide = prs.slides.add_slide(blank)
        slide.shapes.add_picture(str(png), 0, 0, width=Emu(SLIDE_W), height=Emu(SLIDE_H))
        for zone in spec["zones"]:
            add_zone(slide, zone)
    prs.save(TARGET)
    print(f"écrit : {TARGET} ({len(SLIDES)} slides)")


if __name__ == "__main__":
    main()
