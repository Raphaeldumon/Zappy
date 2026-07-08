# Refonte de la présentation Keynote — Design

**Date** : 2026-07-08
**Objectif** : remplacer `Keynote.pptx` (13 slides, images stock hétérogènes, slide IA vide)
par une présentation générée, visuellement cohérente et fondée sur le contenu réel du projet.

## Cadrage

- **Livrable** : un fichier `Keynote.pptx` à la racine (remplace l'actuel), 16:9.
- **Audience** : grand public, keynote Epitech ~10 minutes. Ton vulgarisé, métaphores
  assumées, mais fondé sur des faits vérifiés dans le code et les docs du repo.
- **Mode hybride** : fonds composés en HTML/CSS rendus en PNG (liberté graphique totale)
  + zones de texte natives PowerPoint par-dessus (éditables par l'équipe).
- **Direction artistique** : « Cosmos épuré » — nuit profonde, étoiles discrètes, glow
  réservé aux chiffres-clés. Pas de captures du jeu en fond (réservées aux slides GUI
  en encadrés).

## Plan des slides (~15, en 5 actes)

### Acte I — Le monde (accent : or Luan)
1. **Titre** — ZAPPY, tagline, les 5 noms (Raphael Dumon, Clément Fabre, Mathéo Emma,
   Léo Cazabat, Joachim Ismael).
2. **Zappy en une phrase** — une course entre équipes d'IA autonomes sur une
   planète-anneau, arbitrée par un serveur, observée en direct en 3D. Schéma des
   3 programmes (serveur / IA / GUI) et de qui parle à qui.
3. **Les règles** — manger pour survivre, 6 types de pierres, incantations à plusieurs ;
   la première équipe qui amène 6 joueurs au niveau 8 gagne. Tableau d'élévation
   simplifié en visuel.
4. **Un monde-anneau cadencé** — carte torique (sortir à droite = revenir à gauche) +
   ticks : chaque action a un prix (avancer = 7, incanter = 300), la fréquence `-f`
   accélère tout.

### Acte II — Le serveur, l'arbitre (accent : bleu froid)
5. **Le restaurant** — métaphore gardée : les clients commandent, le serveur tient
   l'unique vérité, accepte ou refuse selon le menu (`ok`/`ko`).
6. **Un seul serveur pour 800 clients, sans jamais courir** — poll() + agenda
   d'événements (`EventScheduler`) : le serveur dort entre deux échéances et se
   réveille exactement quand il faut (paquet réseau ou événement dû). Un thread,
   zéro attente active. Visuel : agenda/réveil.

### Acte III — Les drones, les habitants (accent : vert aurore)
7. **Le handicap** — un drone ne sait ni où il est (aucune commande ne renvoie la
   position), ni combien ils sont (`Connect_nbr` = slots libres), ni qui lui parle
   (broadcast anonyme + direction seule). Slide « tout ce que le drone n'a PAS ».
8. **S'organiser à l'oreille** — relèvement sonore sur 8 directions (boussole),
   recensement HELLO, élection du chef (plus petit id). Se compter sans se voir.
9. **ARRIVED : de 14 % à 92 %** — seul le relèvement 0 prouve la co-location ;
   le leader ne compte que les ARRIVED. Le chiffre en glow.
10. **Le Blitz** — 5 phases : collecter les 38 pierres du manifeste, banquer la
    nourriture (réserve 200 ≈ 25 s de maintien), converger, lâcher tout, enchaîner
    6 incantations → niveau 8 en ~30 s, 4 victoires sur 4.

### Acte IV — Le GUI, la fenêtre (accent : violet)
11. **Le spectateur branché en direct** — snapshot initial puis flux d'événements
    granulaires, traduits en 3D en temps réel.
12. **Showcase** — captures réelles en encadrés : cycle jour/nuit Luan & Palasse,
    saisons, ombres dynamiques, particules, aurores.
13. **Remonter le temps** — timeline scrub (le monde se reconstruit en rejouant
    l'historique) + replays `.zrec`.

### Acte V — Fin
14. **Et aussi…** — bonus en rafale : broadcasts d'équipe chiffrés (magic byte +
    payload binaire b64 — les adversaires entendent du bruit), console admin
    (pause, spawn, kill, set f en live), config JSON hot-reload (SIGUSR2).
15. **Ce que Zappy nous a appris + Merci** — réseau, architecture, coordination
    distribuée, 3D, équipe. Noms + invitation à la démo.

Chaque affirmation s'appuie sur `docs/server.md`, `docs/AI_OVERVIEW.md`,
`docs/protocols.md`, `docs/gui_raylib.md`.

## Design system

- **Format** : slides conçues en 1920×1080, rendues en 2× (3840×2160).
- **Palette** : base nuit (bleus-indigo très sombres, dégradés subtils, étoiles
  discrètes). Un accent par acte : or chaud (I), bleu froid (II), vert aurore (III),
  violet (IV), neutre (V). Glow doux réservé aux chiffres-clés.
- **Typo** : titres et décor dans les fonds en **Toxigenesis** (`gui/assets/toxigenesis
  bd.otf`, chargée par `@font-face` — aucun risque machine de soutenance). Textes
  natifs en **Liberation Sans/Arial** (présente partout, rendu identique).
- **Diagrammes** : SVG/CSS dans les fonds (restaurant, agenda du serveur, boussole
  8 directions, phases du Blitz, tableau d'élévation). Aucun clipart externe.
- **Captures GUI** : encadrés lumineux sur fond cosmos, slides 12–13 uniquement.

## Pipeline technique

```
presentation/
├── theme.css          # palette, typo, étoiles, layouts communs
├── slide01.html … slide15.html
├── build_pptx.py      # rend (Chrome headless) + assemble (python-pptx)
└── captures/          # screenshots GUI utilisés par les slides 12-13
```

1. Chaque `slideNN.html` est autonome (1920×1080), thème partagé via `theme.css`.
2. `google-chrome --headless --screenshot` capture chaque slide en PNG 3840×2160.
3. `build_pptx.py` (python-pptx, à installer) pose chaque PNG en fond plein écran
   puis superpose les zones de texte natives aux positions réservées, déclarées
   par slide dans le script (le HTML réserve visuellement la place).
4. `make keynote` régénère `Keynote.pptx` en une commande.
5. Captures GUI : lancer `zappy_server` + `zappy_gui` en démo et capturer les moments
   forts ; si le rendu automatisé est décevant, demander 3-4 screenshots à l'équipe.

## Gestion d'erreurs & risques

- **Police manquante le jour J** : impossible par construction — Toxigenesis vit dans
  les PNG, le texte natif utilise une police système universelle.
- **Chrome absent / rendu différent** : vérifié présent (`/usr/bin/google-chrome`).
  Le rendu est déterministe à taille de fenêtre fixée.
- **python-pptx absent** : installé par `build_pptx.py`/Makefile (pip, environnement
  utilisateur).
- **Captures GUI ratées** : repli explicite = screenshots manuels fournis par l'équipe.
- **Texte natif qui déborde** : chaque zone réservée est dimensionnée avec marge ;
  vérification visuelle slide par slide avant livraison (export PNG du pptx final
  via LibreOffice pour contrôle).

## Critères de réussite

- `Keynote.pptx` s'ouvre dans PowerPoint et LibreOffice, 15 slides 16:9, textes
  éditables.
- Identité visuelle unique sur toutes les slides (zéro image stock).
- Le contenu couvre les 5 actes validés, chiffres exacts conformes aux docs.
- Régénérable en une commande (`make keynote`).
