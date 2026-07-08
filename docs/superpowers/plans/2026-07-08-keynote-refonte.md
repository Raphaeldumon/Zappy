# Refonte Keynote — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Générer `Keynote.pptx` (15 slides 16:9, grand public, 10 min) depuis des sources HTML/CSS versionnées : fonds PNG composés + zones de texte natives éditables.

**Architecture:** Un dossier `presentation/` contient un HTML autonome par slide (1920×1080) partageant `theme.css`. `render.sh` capture chaque HTML en PNG 3840×2160 via Chrome headless (les blocs `.native` sont masqués au rendu final). `build_pptx.py` assemble le PPTX : PNG en fond plein écran + zones de texte natives (mêmes coordonnées que les `.native` du HTML, source de vérité = le manifeste `SLIDES` du script).

**Tech Stack:** HTML/CSS/SVG inline, Chrome headless (`/usr/bin/google-chrome`), Python 3.12 + python-pptx (venv local `presentation/.venv`), GNU Make.

## Global Constraints

- Spec : `docs/superpowers/specs/2026-07-08-keynote-refonte-design.md` — la copie (textes) du plan en est la déclinaison exacte.
- Slides conçues en **1920×1080 px**, rendues en **3840×2160** (`--force-device-scale-factor=2`).
- PPTX en **12192000×6858000 EMU** (16:9). Conversions : `1 px design = 6350 EMU` ; `pt natif = px design / 2` (144 px/inch).
- Titres et décor **dans les fonds** en Toxigenesis (`@font-face` → `../gui/assets/toxigenesis bd.otf`, attention à l'espace dans le nom). Texte natif en **Arial** (rendu Liberation Sans sous Linux, métriquement identique).
- Palette de base nuit : fond `#070B18` → `#0D1430` (dégradé), texte `#EAF0FF`, texte secondaire `#8E9BC0`.
- Accent par acte : Acte I or `#F5C866` · Acte II bleu `#6FB7FF` · Acte III vert aurore `#5CE8B5` · Acte IV violet `#B58CFF` · Acte V neutre `#EAF0FF`.
- Aucune image stock ; uniquement compositions CSS/SVG et captures du GUI (slides 12–13).
- Chaque slide HTML contient un bloc `<div class="native">…</div>` par zone de texte native : visible en préversion (design), masqué au rendu final (`?final`). Les rectangles px de ces divs et du manifeste `SLIDES` doivent rester identiques.
- Français partout, chiffres conformes aux docs (`docs/server.md`, `docs/AI_OVERVIEW.md`, `docs/protocols.md`, `docs/gui_raylib.md`).
- Vérification visuelle : après chaque rendu, inspecter le PNG produit (outil Read). Pas de LibreOffice sur la machine : le PPTX se vérifie par introspection python-pptx + ouverture manuelle par l'utilisateur en fin de parcours.
- Commits fréquents, préfixe `feat(keynote):` / `chore(keynote):`.

---

### Task 1: Infrastructure de rendu + slide 01 (titre)

**Files:**
- Create: `presentation/theme.css`
- Create: `presentation/slide01.html`
- Create: `presentation/render.sh`
- Create: `presentation/out/` (généré, gitignoré)
- Modify: `.gitignore`

**Interfaces:**
- Produces: `presentation/render.sh [NN…]` — rend `slideNN.html` → `presentation/out/slideNN.png` (3840×2160, `.native` masqués) et `presentation/out/preview/slideNN.png` (1920×1080, `.native` visibles). Sans argument : toutes les slides présentes.
- Produces: `theme.css` — classes utilisées par toutes les slides : `.slide`, `.stars`, `.act-gold|.act-blue|.act-green|.act-violet|.act-neutral` (définissent `--accent`), `.kicker`, `.title`, `.glow-number`, `.panel`, `.native`, `.footnote`.

- [ ] **Step 1: Écrire `presentation/theme.css`**

```css
/* Thème « Cosmos épuré » — partagé par toutes les slides. 1920×1080. */
@font-face {
  font-family: "Toxigenesis";
  src: url("../gui/assets/toxigenesis bd.otf") format("opentype");
}

* { margin: 0; padding: 0; box-sizing: border-box; }

html, body { width: 1920px; height: 1080px; overflow: hidden; }

.slide {
  position: relative;
  width: 1920px; height: 1080px;
  background:
    radial-gradient(1200px 800px at 75% -10%, #16204A 0%, transparent 60%),
    linear-gradient(160deg, #0D1430 0%, #070B18 65%, #05070F 100%);
  color: #EAF0FF;
  font-family: "Liberation Sans", Arial, sans-serif;
}

/* Accents par acte : la slide porte une de ces classes sur .slide */
.act-gold    { --accent: #F5C866; }
.act-blue    { --accent: #6FB7FF; }
.act-green   { --accent: #5CE8B5; }
.act-violet  { --accent: #B58CFF; }
.act-neutral { --accent: #EAF0FF; }

/* Champ d'étoiles : trois calques de points en box-shadow, opacités décroissantes */
.stars, .stars::before, .stars::after {
  content: ""; position: absolute; inset: 0; pointer-events: none;
  background-repeat: repeat;
}
.stars {
  background-image: radial-gradient(1.6px 1.6px at 12% 22%, #FFFFFFCC 50%, transparent 51%),
    radial-gradient(1.3px 1.3px at 43% 8%,  #FFFFFF99 50%, transparent 51%),
    radial-gradient(1.2px 1.2px at 71% 31%, #FFFFFFB0 50%, transparent 51%),
    radial-gradient(1.5px 1.5px at 88% 12%, #FFFFFF88 50%, transparent 51%),
    radial-gradient(1.1px 1.1px at 25% 47%, #FFFFFF77 50%, transparent 51%),
    radial-gradient(1.4px 1.4px at 58% 55%, #FFFFFF66 50%, transparent 51%),
    radial-gradient(1.2px 1.2px at 93% 64%, #FFFFFF99 50%, transparent 51%),
    radial-gradient(1.0px 1.0px at 7% 78%,  #FFFFFF55 50%, transparent 51%),
    radial-gradient(1.3px 1.3px at 36% 88%, #FFFFFF88 50%, transparent 51%),
    radial-gradient(1.1px 1.1px at 66% 93%, #FFFFFF66 50%, transparent 51%);
  background-size: 640px 540px;
}
.stars::before { transform: translate(210px, 140px) scale(1.15); opacity: .5; }
.stars::after  { transform: translate(-160px, 300px) scale(0.85); opacity: .3; }

/* Étiquette d'acte, au-dessus du titre */
.kicker {
  position: absolute; left: 110px; top: 96px;
  font-family: "Liberation Sans", Arial, sans-serif;
  font-size: 26px; font-weight: bold; letter-spacing: 8px;
  text-transform: uppercase; color: var(--accent);
}

/* Titre de slide — Toxigenesis, dans le fond uniquement */
.title {
  position: absolute; left: 106px; top: 140px;
  font-family: "Toxigenesis", sans-serif;
  font-size: 84px; line-height: 1.05; color: #FFFFFF;
  text-shadow: 0 0 42px color-mix(in srgb, var(--accent) 45%, transparent);
}

/* Chiffre-clé en glow (92 %, ~30 s, 806…) */
.glow-number {
  font-family: "Toxigenesis", sans-serif;
  color: var(--accent);
  text-shadow: 0 0 30px color-mix(in srgb, var(--accent) 80%, transparent),
               0 0 90px color-mix(in srgb, var(--accent) 45%, transparent);
}

/* Encadré lumineux (captures, tableaux) */
.panel {
  border: 1px solid color-mix(in srgb, var(--accent) 55%, transparent);
  border-radius: 14px;
  background: #FFFFFF08;
  box-shadow: 0 0 34px color-mix(in srgb, var(--accent) 22%, transparent);
}

/* Zone de texte NATIVE : visible en préversion, masquée au rendu final.
   Position/tailles inline sur chaque div — les mêmes valeurs vivent dans
   build_pptx.py (manifeste SLIDES). pt natif = px/2. */
.native {
  position: absolute;
  font-family: "Liberation Sans", Arial, sans-serif;
  color: #EAF0FF; line-height: 1.3; white-space: pre-line;
}
body.final .native { visibility: hidden; }

.footnote {
  position: absolute; left: 110px; bottom: 48px;
  font-size: 22px; color: #8E9BC0;
}
```

- [ ] **Step 2: Écrire `presentation/slide01.html`** (slide de titre, acte I)

```html
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<link rel="stylesheet" href="theme.css">
<style>
  /* Anneau stylisé du monde-tore, derrière le titre */
  .ring {
    position: absolute; left: 960px; top: 480px; width: 980px; height: 420px;
    transform: translate(-50%, -50%) rotate(-14deg);
    border-radius: 50%;
    border: 3px solid #F5C86655;
    box-shadow: 0 0 60px #F5C86633, inset 0 0 60px #F5C86622;
  }
  .ring::before {
    content: ""; position: absolute; inset: 54px 120px;
    border-radius: 50%;
    border: 2px solid #6FB7FF33;
  }
  .logo {
    position: absolute; left: 0; right: 0; top: 360px;
    text-align: center;
    font-family: "Toxigenesis", sans-serif;
    font-size: 220px; letter-spacing: 26px; color: #FFFFFF;
    text-shadow: 0 0 60px #F5C86688, 0 0 160px #F5C86644;
  }
  .luan {
    position: absolute; left: 1490px; top: 220px; width: 90px; height: 90px;
    border-radius: 50%;
    background: radial-gradient(circle at 35% 35%, #FFE9B8, #F5C866 60%, #B8862F);
    box-shadow: 0 0 50px #F5C86699;
  }
  .palasse {
    position: absolute; left: 330px; top: 700px; width: 56px; height: 56px;
    border-radius: 50%;
    background: radial-gradient(circle at 40% 35%, #D8E4FF, #8FA8D8 65%, #4A5C85);
    box-shadow: 0 0 34px #8FA8D866;
  }
</style>
</head>
<body>
<div class="slide act-gold">
  <div class="stars"></div>
  <div class="ring"></div>
  <div class="luan"></div>
  <div class="palasse"></div>
  <div class="logo">ZAPPY</div>
  <!-- Zones natives (éditables dans le PPTX) -->
  <div class="native" style="left:310px; top:640px; width:1300px; height:60px;
       font-size:40px; text-align:center; color:#EAF0FF;">Une course d’intelligences artificielles sur un monde-anneau</div>
  <div class="native" style="left:210px; top:950px; width:1500px; height:44px;
       font-size:28px; text-align:center; color:#8E9BC0;">Raphael Dumon · Clément Fabre · Mathéo Emma · Léo Cazabat · Joachim Ismael</div>
</div>
<script>
  if (location.search.includes("final")) document.body.classList.add("final");
</script>
</body>
</html>
```

- [ ] **Step 3: Écrire `presentation/render.sh`**

```bash
#!/usr/bin/env bash
# Rend les slides HTML en PNG.
#   ./render.sh          → toutes les slides présentes
#   ./render.sh 01 04    → slides choisies
# Sorties : out/slideNN.png (3840×2160, fonds finaux, .native masqués)
#           out/preview/slideNN.png (1920×1080, .native visibles)
set -euo pipefail
cd "$(dirname "$0")"
CHROME=${CHROME:-google-chrome}
mkdir -p out/preview

slides=("$@")
if [ ${#slides[@]} -eq 0 ]; then
  for f in slide??.html; do slides+=("${f:5:2}"); done
fi

for nn in "${slides[@]}"; do
  f="slide${nn}.html"
  [ -f "$f" ] || { echo "skip: $f absent" >&2; continue; }
  "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
    --force-device-scale-factor=2 --window-size=1920,1080 \
    --virtual-time-budget=3000 \
    --screenshot="out/slide${nn}.png" "file://$PWD/${f}?final" 2>/dev/null
  "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
    --force-device-scale-factor=1 --window-size=1920,1080 \
    --virtual-time-budget=3000 \
    --screenshot="out/preview/slide${nn}.png" "file://$PWD/${f}" 2>/dev/null
  echo "rendu: slide${nn}"
done
```

- [ ] **Step 4: Gitignorer les sorties** — ajouter à `.gitignore` :

```
presentation/out/
presentation/.venv/
```

- [ ] **Step 5: Rendre et vérifier**

Run: `chmod +x presentation/render.sh && presentation/render.sh 01`
Expected: `rendu: slide01`, fichiers `presentation/out/slide01.png` et `presentation/out/preview/slide01.png`.

Run: `python3 -c "from PIL import Image; print(Image.open('presentation/out/slide01.png').size)"`
Expected: `(3840, 2160)`

Inspection visuelle (Read) de `out/preview/slide01.png` : logo ZAPPY en Toxigenesis (lettres anguleuses — si la police a un rendu générique, le chemin `@font-face` est cassé), anneau doré, Luan/Palasse, tagline et noms visibles. Puis Read de `out/slide01.png` : mêmes fonds mais **sans** tagline ni noms (zones natives masquées).

- [ ] **Step 6: Commit**

```bash
git add presentation/theme.css presentation/slide01.html presentation/render.sh .gitignore
git commit -m "feat(keynote): infrastructure de rendu + slide titre"
```

---

### Task 2: `build_pptx.py` + cible `make keynote`

**Files:**
- Create: `presentation/build_pptx.py`
- Modify: `Makefile` (nouvelle cible `keynote`, à ajouter après la cible `demo`, et `keynote` dans `help`)

**Interfaces:**
- Consumes: `presentation/out/slideNN.png` produits par `render.sh` (Task 1).
- Produces: `SLIDES` — manifeste Python (liste de dicts) dans `build_pptx.py` ; chaque tâche de slide suivante y ajoute son entrée : `{"n": "01", "zones": [{"px": (x, y, w, h), "text": "…", "pt": 20, "color": "EAF0FF", "bold": False, "align": "left"}]}`.
- Produces: `make keynote` — rend toutes les slides puis écrit `Keynote.pptx` à la racine.

- [ ] **Step 1: Écrire `presentation/build_pptx.py`** (avec la slide 01 seule dans le manifeste)

```python
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
```

- [ ] **Step 2: Ajouter la cible `keynote` au `Makefile`** (après la cible `demo`)

```make
PRESENTATION_PY := presentation/.venv/bin/python

keynote:
	@test -x $(PRESENTATION_PY) || { \
		python3 -m venv presentation/.venv && \
		presentation/.venv/bin/pip install --quiet python-pptx pillow; }
	@./presentation/render.sh
	@$(PRESENTATION_PY) presentation/build_pptx.py
```

Ajouter aussi une ligne dans la cible `help` existante : `keynote — régénère Keynote.pptx depuis presentation/`.

- [ ] **Step 3: Construire et vérifier structurellement**

Run: `make keynote`
Expected: `rendu: slide01` puis `écrit : …/Keynote.pptx (1 slides)`.

Run:
```bash
presentation/.venv/bin/python - <<'EOF'
from pptx import Presentation
from pptx.util import Emu
p = Presentation("Keynote.pptx")
assert (p.slide_width, p.slide_height) == (12192000, 6858000)
s = p.slides[0]
kinds = [sh.shape_type for sh in s.shapes]
texts = [sh.text_frame.text for sh in s.shapes if sh.has_text_frame]
print(len(p.slides.__iter__.__self__._sldIdLst), kinds, texts)
EOF
```
Expected: 1 slide, une PICTURE + 2 TEXT_BOX, textes = tagline et noms.

- [ ] **Step 4: Commit**

```bash
git add presentation/build_pptx.py Makefile
git commit -m "feat(keynote): assemblage pptx (fonds + texte natif) et cible make"
```

---

### Task 3: Acte I — slides 02, 03, 04

**Files:**
- Create: `presentation/slide02.html`, `presentation/slide03.html`, `presentation/slide04.html`
- Modify: `presentation/build_pptx.py` (entrées manifeste)

**Interfaces:**
- Consumes: `theme.css` (Task 1), manifeste `SLIDES` (Task 2).
- Produces: rien pour les tâches suivantes (slides indépendantes).

Toutes trois : `<div class="slide act-gold">`, `.stars`, kicker `ACTE I — LE MONDE`, titre Toxigenesis dans le fond, corps de texte en zones natives à gauche, diagramme SVG/CSS à droite (x ≥ 1000 px).

- [ ] **Step 1: `slide02.html` — « Trois programmes, un monde »**

Titre (fond) : `Trois programmes, un monde`.
Diagramme (fond, droite) : trois nœuds `.panel` disposés en triangle — SERVEUR (haut, accent or, étiquette « la vérité du monde »), IA ×n (bas-gauche, « les habitants »), GUI (bas-droite, « la fenêtre »). Flèches SVG étiquetées `TCP` reliant IA→SERVEUR (double sens) et SERVEUR→GUI (sens unique). Style flèches : trait `#8E9BC0`, pointes triangulaires, étiquettes 24px.
Zones natives (HTML + manifeste, mêmes rectangles) :

| px (x, y, w, h) | pt | texte |
|---|---|---|
| (110, 400, 780, 300) | 20 | `Un serveur arbitre la partie et détient la seule vérité du monde.\nDes drones autonomes, chacun piloté par une IA.\nUne interface 3D qui montre tout, en direct.` |
| (110, 760, 780, 90) | 16, color 8E9BC0 | `Trois programmes séparés, écrits par nous, qui ne se parlent qu’en s’envoyant du texte sur le réseau.` |

- [ ] **Step 2: `slide03.html` — « Les règles du jeu »**

Titre (fond) : `Les règles du jeu`.
Diagramme (fond, droite) : colonne `.panel` « échelle d'élévation » — 8 barreaux étiquetés 1 → 8, le 8 en glow or ; à côté de chaque palier 2→8, pastilles indiquant joueurs requis (1, 2, 2, 4, 4, 6, 6) ; légende `joueurs requis` en 22px `#8E9BC0`. Sous l'échelle, rangée de 6 gemmes stylisées (losanges CSS de couleurs distinctes) + une pastille « nourriture ».
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 800, 340) | 20 | `Manger pour survivre — la nourriture s’épuise en continu.\nCollecter six types de pierres précieuses.\nS’élever par des incantations — parfois à plusieurs.` |
| (110, 790, 800, 120) | 22, bold, color F5C866 | `Victoire : la première équipe qui amène 6 joueurs au niveau 8.` |

- [ ] **Step 3: `slide04.html` — « Un monde-anneau cadencé »**

Titre (fond) : `Un monde-anneau cadencé`.
Diagramme (fond, droite) : tore vu de dessus — deux ellipses concentriques dorées, grille suggérée par arcs ; une flèche qui sort du bord droit et revient par le bord gauche (chemin SVG pointillé, pointe accent). En dessous, mini-tableau `.panel` des coûts : `avancer 7 · regarder 7 · pondre 42 · incanter 300` avec `ticks` en exposant, en Toxigenesis 26px.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 800, 300) | 20 | `Sortir à droite, c’est revenir par la gauche : la carte est un tore.\nLe temps s’écoule en ticks : chaque action a un prix.\nLa fréquence f accélère le monde entier — de la balade au sprint.` |
| (110, 760, 800, 90) | 16, color 8E9BC0 | `f = 1 → une unité de temps par seconde. f = 1000 → mille fois plus vite.` |

- [ ] **Step 4: Ajouter les 3 entrées au manifeste `SLIDES`** de `build_pptx.py`, rectangles/pt/textes strictement identiques aux tableaux ci-dessus.

- [ ] **Step 5: Rendre et vérifier**

Run: `presentation/render.sh 02 03 04`
Inspection visuelle (Read) des trois `out/preview/slideNN.png` : hiérarchie lisible, diagrammes équilibrés, rien ne déborde, accents or cohérents. Corriger et re-rendre jusqu'à satisfaction.

- [ ] **Step 6: Commit**

```bash
git add presentation/slide02.html presentation/slide03.html presentation/slide04.html presentation/build_pptx.py
git commit -m "feat(keynote): acte I — le monde (slides 02-04)"
```

---

### Task 4: Acte II — slides 05, 06

**Files:**
- Create: `presentation/slide05.html`, `presentation/slide06.html`
- Modify: `presentation/build_pptx.py`

Classe d'acte : `act-blue`, kicker `ACTE II — LE SERVEUR`.

- [ ] **Step 1: `slide05.html` — « Le serveur : un restaurant »**

Titre (fond) : `Le serveur : un restaurant`.
Diagramme (fond, droite) : une carte « MENU » `.panel` (bord accent bleu) listant en Toxigenesis 28px : `Forward · Look · Take · Broadcast · Incantation…` ; au-dessus, bulle de dialogue arrondie contenant en italique 30px : `« Bonjour, un Forward et une grande frite »` ; sous le menu deux tampons : `ok` (vert `#5CE8B5`) et `ko` (rouge `#FF7A7A`), légèrement inclinés.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 800, 300) | 20 | `Les IA et la GUI sont les clients : ils passent commande.\nLe serveur vérifie que c’est au menu, répond ok ou ko…\n…puis sert chaque plat au bon moment — pas avant.` |
| (110, 760, 800, 90) | 16, color 8E9BC0 | `Hors menu ? « ko », et on passe à la commande suivante.` |

- [ ] **Step 2: `slide06.html` — « 800 clients, un seul serveur »**

Titre (fond) : `800 clients, un seul serveur`.
Grand chiffre (fond) : `806` en `.glow-number` 150px, coin haut-droit, sous-titre 24px `clients simultanés au maximum`.
Diagramme (fond, droite, sous le chiffre) : un agenda vertical `.panel` — lignes horaires avec pastilles `repas`, `fin d'incantation`, `éclosion`, et un curseur « maintenant » ; à gauche de l'agenda, pictogramme de lune + « zzz » entre deux échéances.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 800, 320) | 20 | `Un seul programme, un seul fil d’exécution — et aucune attente active.\npoll() surveille toutes les tables à la fois.\nUn agenda d’événements planifie chaque échéance : le serveur dort, puis se réveille exactement quand il faut.` |
| (110, 790, 800, 90) | 16, color 8E9BC0 | `Réveillé par un paquet réseau ou par l’échéance suivante — jamais pour rien.` |

- [ ] **Step 3: Ajouter les entrées au manifeste, rendre, vérifier, committer**

Run: `presentation/render.sh 05 06` puis inspection visuelle des previews.

```bash
git add presentation/slide05.html presentation/slide06.html presentation/build_pptx.py
git commit -m "feat(keynote): acte II — le serveur (slides 05-06)"
```

---

### Task 5: Acte III — slides 07, 08, 09, 10

**Files:**
- Create: `presentation/slide07.html` … `presentation/slide10.html`
- Modify: `presentation/build_pptx.py`

Classe d'acte : `act-green`, kicker `ACTE III — LES DRONES`.

- [ ] **Step 1: `slide07.html` — « Un drone naît aveugle »**

Titre (fond) : `Un drone naît aveugle`.
Fond plus sombre que les autres (overlay `#00000055` sur le dégradé). Diagramme (fond, droite) : trois pictogrammes barrés d'un trait accent — épingle de carte (`position`), annuaire/liste (`équipe`), étiquette de nom (`émetteur`) ; chaque picto dans un cercle `#FFFFFF0A`, croix diagonale `#FF7A7A`.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 820, 320) | 20 | `Il ne sait pas où il est — aucune commande ne donne sa position.\nIl ne sait pas combien ils sont — pas d’annuaire d’équipe.\nIl ne sait pas qui parle — un message entendu est anonyme.` |
| (110, 790, 820, 90) | 18, bold | `Tout ce qui suit existe pour contourner ces trois handicaps.` |

- [ ] **Step 2: `slide08.html` — « S'organiser à l'oreille »**

Titre (fond) : `S’organiser à l’oreille`.
Diagramme (fond, droite) : boussole SVG — cercle accent, 8 rayons numérotés 1–8 dans le sens horaire (1 en haut), drone au centre (triangle orienté vers 1), secteur 1 surligné ; légende 22px : `0 = même case`. En dessous, trois petites bulles `HELLO` convergeant vers un compteur `équipe : 6`.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 820, 320) | 20 | `Chaque son entendu arrive avec une direction — huit possibles.\nHELLO : chacun se présente en boucle, l’équipe se compte toute seule.\nLe plus petit identifiant devient le chef — élection sans dispute.` |
| (110, 790, 820, 90) | 16, color 8E9BC0 | `Pour rejoindre quelqu’un : marcher vers son son, encore et encore.` |

- [ ] **Step 3: `slide09.html` — « ARRIVED : la présence prouvée »**

Titre (fond) : `ARRIVED : la présence prouvée`.
Grand visuel (fond, droite) : `14 % → 92 %` en `.glow-number` 130px (le `92 %` plus grand que le `14 %`), sous-titre 26px `de rituels réussis`. En dessous, mini-scène : deux drones sur une même case surlignée, onde `0` entre eux.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 820, 320) | 20 | `Vu d’un chef, un « joueur » sur sa case n’a pas de niveau : impossible de savoir si c’est le bon coéquipier.\nMais un son reçu « sur ma case » (direction 0), lui, ne ment pas.\nChacun annonce ARRIVED en entendant le chef à direction 0 — le rituel ne part que sur les présences confirmées.` |
| (110, 800, 820, 80) | 16, color 8E9BC0 | `Une idée simple, trouvée après beaucoup de rituels ratés.` |

- [ ] **Step 4: `slide10.html` — « Le Blitz »**

Titre (fond) : `Le Blitz — gagner en 30 secondes`.
Diagramme (fond, pleine largeur sous le texte natif) : frise horizontale de 5 étapes `.panel` reliées par des chevrons accent : `COLLECTER — 38 pierres chacun` → `BANQUER — 200 nourritures` → `CONVERGER — au son du chef` → `TOUT LÂCHER — sur une case` → `INCANTER ×6 — d'affilée`. Au bout de la frise, `NIVEAU 8` en `.glow-number` 64px. Coin haut-droit : `4 / 4` en `.glow-number` 110px, sous-titre `parties gagnées`.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 1100, 220) | 20 | `Une seule stratégie, réglée comme une horlogerie : chacun amasse tout le nécessaire, l’équipe fait le plein de nourriture, six convergent vers le chef…\n…qui dépose les 38 pierres et enchaîne les six rituels d’un coup.` |
| (110, 950, 900, 70) | 18, bold, color 5CE8B5 | `Du niveau 2 au niveau 8 : environ 3 secondes. Partie gagnée en ~30.` |

- [ ] **Step 5: Ajouter les entrées au manifeste, rendre, vérifier, committer**

Run: `presentation/render.sh 07 08 09 10` puis inspection visuelle des previews.

```bash
git add presentation/slide07.html presentation/slide08.html presentation/slide09.html presentation/slide10.html presentation/build_pptx.py
git commit -m "feat(keynote): acte III — les drones (slides 07-10)"
```

---

### Task 6: Acte IV — slides 11 et 13 (le 12 attend les captures)

**Files:**
- Create: `presentation/slide11.html`, `presentation/slide13.html`
- Modify: `presentation/build_pptx.py`

Classe d'acte : `act-violet`, kicker `ACTE IV — LA FENÊTRE`.

- [ ] **Step 1: `slide11.html` — « Le spectateur branché en direct »**

Titre (fond) : `Le spectateur branché en direct`.
Diagramme (fond, droite) : à gauche un nœud `SERVEUR`, à droite un cadre d'écran `.panel` contenant une mini-grille 3D stylisée (losanges) ; entre les deux, un flux de petites étiquettes wire qui « coulent » vers l'écran : `pnw` `ppo` `pic` `pdi` `seg` en monospace 22px, opacités décroissantes ; première étiquette plus grosse : `instantané complet`.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 800, 300) | 20 | `Au branchement : le serveur envoie un instantané complet du monde.\nEnsuite : chaque changement arrive en temps réel, message par message…\n…et la 3D le traduit aussitôt — déplacements, rituels, naissances, morts.` |
| (110, 760, 800, 90) | 16, color 8E9BC0 | `La GUI observe tout mais ne joue jamais : spectatrice, pas arbitre.` |

- [ ] **Step 2: `slide13.html` — « Remonter le temps »**

Titre (fond) : `Remonter le temps`.
Diagramme (fond, bas, pleine largeur) : barre de timeline `.panel` avec graduation, curseur accent tiré vers la gauche (flèche « rembobiner »), trois vignettes fantômes du monde au-dessus (états passés, opacité croissante vers « maintenant »). Coin droit : badge fichier `partie.zrec`.
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 400, 900, 300) | 20 | `Chaque message reçu est daté et archivé.\nTirer la timeline en arrière reconstruit le monde à cet instant précis — en rejouant son histoire.\nUne partie entière s’enregistre dans un fichier .zrec, et se revoit plus tard.` |
| (110, 760, 900, 80) | 16, color 8E9BC0 | `Idéal pour comprendre une défaite… ou savourer une victoire image par image.` |

- [ ] **Step 3: Ajouter les entrées au manifeste, rendre, vérifier, committer**

Run: `presentation/render.sh 11 13` puis inspection visuelle des previews.

```bash
git add presentation/slide11.html presentation/slide13.html presentation/build_pptx.py
git commit -m "feat(keynote): acte IV — la fenêtre (slides 11 et 13)"
```

---

### Task 7: Captures GUI + slide 12 (showcase)

**Files:**
- Create: `presentation/captures/` (PNG committés — ce sont des sources de la présentation)
- Create: `presentation/slide12.html`
- Modify: `presentation/build_pptx.py`

**Interfaces:**
- Consumes: `make demo` (lance serveur + IA + GUI, cf. Makefile), session X11, ImageMagick `import`.

- [ ] **Step 1: Lancer la démo et capturer 3–4 moments forts**

```bash
cd /home/peppermint/Epitech/Zappy
make demo &                       # serveur + bots + GUI (FREQ 300 par défaut)
sleep 12                          # laisser la partie s'installer
WID=$(xwininfo -root -tree | grep -i zappy | head -1 | awk '{print $1}')
mkdir -p presentation/captures
import -window "$WID" presentation/captures/gui_nuit.png
# attendre / observer, capturer d'autres instants :
#   gui_jour.png (Luan haut), gui_ombres.png (ombres longues), gui_rituel.png (incantation)
```

Espacer les captures de 20–40 s pour couvrir des états différents (jour/nuit, saison, rituel). Vérifier chaque PNG (Read) : net, fenêtre entière, moment intéressant. **Repli si capture impossible ou moche** (fenêtre masquée, tearing…) : s'arrêter et demander à l'utilisateur 3–4 screenshots manuels — c'est le repli prévu par la spec. Tuer la démo ensuite (`kill %1` / `pkill -f zappy_server`).

- [ ] **Step 2: `slide12.html` — « Le monde, en vrai »** (act-violet)

Titre (fond) : `Le monde, en vrai`.
Composition (fond) : mosaïque 2×2 de `.panel` contenant les captures (`<img src="captures/gui_nuit.png">`, etc., `object-fit: cover`), légères rotations alternées (−1.2° / +0.8°) pour un effet planche photo ; étiquette 22px sous chaque vignette : `nuit sous Palasse` · `plein jour sous Luan` · `ombres dynamiques` · `rituel en cours` (adapter aux captures réellement obtenues).
Zones natives :

| px | pt | texte |
|---|---|---|
| (110, 950, 1700, 70) | 18, align center | `Cycle jour/nuit sous nos deux astres, saisons, ombres portées, particules, aurores — tout est calculé en direct.` |

- [ ] **Step 3: Ajouter l'entrée au manifeste, rendre, vérifier, committer**

Run: `presentation/render.sh 12` puis inspection visuelle du preview.

```bash
git add presentation/captures presentation/slide12.html presentation/build_pptx.py
git commit -m "feat(keynote): slide showcase avec captures du GUI"
```

---

### Task 8: Acte V — slides 14, 15

**Files:**
- Create: `presentation/slide14.html`, `presentation/slide15.html`
- Modify: `presentation/build_pptx.py`

Classe d'acte : `act-neutral`, kicker `ACTE V — POUR FINIR`.

- [ ] **Step 1: `slide14.html` — « Et aussi… »**

Titre (fond) : `Et aussi…`.
Composition (fond) : trois cartes `.panel` verticales côte à côte, chacune avec un pictogramme CSS et un titre Toxigenesis 34px : `MESSAGES CHIFFRÉS` (cadenas + suite de caractères brouillés), `CONSOLE ADMIN` (chevron de terminal `>_`), `RÉGLAGES À CHAUD` (curseurs). Le corps de chaque carte reste vide : le texte est natif.
Zones natives (une par carte, alignées sur les cartes à ~y=560) :

| px | pt | texte |
|---|---|---|
| (140, 560, 500, 300) | 16 | `Les équipes parlent en code : un octet magique + un contenu illisible.\nLes adversaires n’entendent que du bruit.` |
| (710, 560, 500, 300) | 16 | `Pause, reprise, changement de vitesse, apparition d’objets, en pleine partie.\nSur un port séparé, protégé par jeton.` |
| (1280, 560, 500, 300) | 16 | `Densités, coûts, durées : un fichier de configuration rechargeable sans redémarrer le serveur.` |

- [ ] **Step 2: `slide15.html` — « Merci »**

Titre (fond, centré, 120px) : `MERCI`.
Composition (fond) : reprise sobre de l'anneau du slide 01 (plus discret, opacité réduite), Luan et Palasse aux coins opposés. Bandeau bas : cinq points accent espacés (un par membre).
Zones natives :

| px | pt | texte |
|---|---|---|
| (260, 560, 1400, 120) | 18, align center | `Réseau, architecture, coordination distribuée, 3D temps réel — et beaucoup de travail d’équipe.\nUne démo tourne juste à côté : venez voir le monde vivre.` |
| (210, 930, 1500, 44) | 14, align center, color 8E9BC0 | `Raphael Dumon · Clément Fabre · Mathéo Emma · Léo Cazabat · Joachim Ismael` |

- [ ] **Step 3: Ajouter les entrées au manifeste, rendre, vérifier, committer**

Run: `presentation/render.sh 14 15` puis inspection visuelle des previews.

```bash
git add presentation/slide14.html presentation/slide15.html presentation/build_pptx.py
git commit -m "feat(keynote): acte V — bonus et merci (slides 14-15)"
```

---

### Task 9: Assemblage final, revue complète, livraison

**Files:**
- Modify: `Keynote.pptx` (régénéré, remplace l'ancien)

- [ ] **Step 1: Régénérer tout**

Run: `make keynote`
Expected: 15 lignes `rendu: slideNN` puis `écrit : …/Keynote.pptx (15 slides)`.

- [ ] **Step 2: Revue visuelle complète**

Inspecter (Read) les 15 `presentation/out/preview/slideNN.png` un par un, en vérifiant contre la spec : hiérarchie, débordements, cohérence des accents par acte, chiffres exacts (806, 38, 200, 14 %→92 %, ~30 s, 4/4, niveau 8, coûts 7/42/300). Corriger toute slide fautive (HTML + manifeste si zones modifiées), re-rendre, re-générer.

- [ ] **Step 3: Vérification structurelle du PPTX**

```bash
presentation/.venv/bin/python - <<'EOF'
from pptx import Presentation
p = Presentation("Keynote.pptx")
slides = list(p.slides)
assert len(slides) == 15, len(slides)
for i, s in enumerate(slides, 1):
    shapes = list(s.shapes)
    assert any(sh.shape_type == 13 for sh in shapes), f"slide {i}: pas d'image de fond"
    n_txt = sum(1 for sh in shapes if sh.has_text_frame and sh.text_frame.text.strip())
    print(f"slide {i:02d}: {len(shapes)} shapes, {n_txt} zones de texte")
EOF
```
Expected: 15 slides, chacune avec image de fond + le nombre de zones du manifeste.

- [ ] **Step 4: Remise à l'utilisateur**

Envoyer `Keynote.pptx` (SendUserFile) pour ouverture dans LibreOffice/PowerPoint — c'est le contrôle final que la machine ne peut pas faire ici (rendu des zones natives). Demander en particulier : lisibilité au projecteur, textes bien éditables, aucun chevauchement fond/texte.

- [ ] **Step 5: Commit final**

```bash
git add presentation/
git commit -m "feat(keynote): présentation complète régénérable (make keynote)"
```

Note : `Keynote.pptx` est un artefact généré — décision utilisateur de le committer ou non (l'ancien n'était pas suivi par git).

---

## Self-review (fait à l'écriture)

- **Couverture spec** : 15 slides ✓, 5 actes ✓, hybride fond/texte natif ✓, palette par acte ✓, Toxigenesis dans les fonds ✓, pipeline render/build/make ✓, captures + repli manuel ✓, critères de réussite couverts par Task 9 ✓.
- **Placeholders** : aucun TBD ; chaque slide a sa copie exacte et sa spec de diagramme ; le code d'infrastructure est complet.
- **Cohérence** : rectangles px identiques entre divs `.native` et manifeste (règle énoncée en contrainte globale, vérifiée slide par slide en Task 9) ; `pt = px/2` partout ; noms de fichiers `slideNN.html` / `out/slideNN.png` cohérents entre `render.sh` et `build_pptx.py`.
