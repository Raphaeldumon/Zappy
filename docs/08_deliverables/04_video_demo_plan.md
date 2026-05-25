# 04 — Storyboard vidéo démo (5 min)

**Cible** : 5 minutes, 1080p ou 4K, h264/h265, MP4
**Outil capture** : OBS Studio
**Montage** : DaVinci Resolve / Kdenlive / ffmpeg
**Voiceover** : optionnel (sinon musique + texte sur écran)
**Format publication** : MP4, YouTube unlisted (lien partageable)

## Vue d'ensemble

| Section | Durée | Contenu |
|---------|-------|---------|
| Intro | 0:00 - 0:15 | Logo Zappy, contexte projet |
| Architecture overview | 0:15 - 0:45 | Diagramme animé + texte (les 3 composants) |
| GUI 3D | 0:45 - 2:00 | Cinematic caméra autour torus, atmosphère, particles, post-FX |
| Gameplay AI | 2:00 - 3:30 | Time-lapse partie complète (24 AIs RL), élévations, broadcasts |
| Bonus | 3:30 - 4:30 | Admin, Grafana, Replay scrub, 2D planisphère, F3 debug |
| AI training | 4:30 - 4:50 | W&B charts, ELO leaderboard, eval |
| Outro | 4:50 - 5:00 | Logo team, GitHub URL, merci |

## Détail par section

### 0:00 - 0:15 — Intro

- Fade-in logo `{ZAPPY}` style des PDFs Epitech (couleur bleue)
- Texte en bas : `G-YEP-400 · Epitech · 2026`
- Musique : ambient sci-fi commençant doucement
- Voiceover (optionnel) : *"Zappy. Un jeu réseau de stratégie multi-agents, implémenté à 100% en C++17 avec Vulkan 1.3 et IA RL multi-agent en PyTorch."*

### 0:15 - 0:45 — Architecture overview

- Animation slide : 3 boîtes apparaissent
  - Server (C++) en haut
  - GUI (C++/Vulkan) en bas gauche
  - AI (C++/libtorch) en bas droite
- Flèches TCP entre boîtes
- En bas de l'écran : encart "Offline training : libzappy_sim → pybind11 → RLlib PPO multi-agent → TorchScript export"
- Voiceover : *"Trois binaires : un serveur autoritatif single-thread avec poll, un client graphique Vulkan 1.3, un client IA libtorch. Et un pipeline d'entraînement RL qui réutilise la logique du serveur via pybind11."*

### 0:45 - 2:00 — GUI 3D (sequence cinematic)

Pré-tourné avec mode caméra `CINEMATIC` activé dans le GUI.

Plan 1 (0:45 - 1:10) :
- Zoom out depuis la surface du torus → vue large dans l'espace
- Skybox étoiles + nebula visible
- Atmosphère scattering halo autour
- Voiceover : *"La planète Trantor — un monde toroïdal sphérique — flotte dans l'espace. Le ciel et la nébuleuse sont entièrement procédurals via FBM noise."*

Plan 2 (1:10 - 1:35) :
- Orbital tour autour du torus
- Trantorians se baladent
- Particles : on déclenche manuellement une incantation, glow effect spectaculaire
- Voiceover : *"Les particules sont calculées sur GPU via compute shader. Voici une incantation, le ritual qui élève les trantoriens de niveau."*

Plan 3 (1:35 - 2:00) :
- Vue rapprochée trantorian + ressources
- Quick toggle SSAO on/off pour montrer effet (split screen)
- Bloom et tonemap visibles
- Voiceover : *"SSAO, TAA, Bloom, tonemap ACES — pipeline post-FX complet via bindless descriptor indexing pour zéro rebind de textures."*

### 2:00 - 3:30 — Gameplay AI

Plan 1 (2:00 - 2:30) :
- Lancement de 24 AIs RL (4 teams × 6)
- Vue top-down speed 8x
- Voiceover : *"24 agents RL, 4 teams. Chaque agent charge le même modèle PPO trainé, l'inférence libtorch C++ prend moins de 2 ms par décision."*

Plan 2 (2:30 - 3:00) :
- Speed 16x, time-lapse
- Élévations qui se déclenchent (visible dans timeline)
- Broadcasts (lines colored from emetteur)
- Voiceover : *"Les agents s'organisent via le broadcast codé intra-team. Magic byte XOR + payload base64. Les agents adverses voient les paquets mais ne peuvent pas les décoder."*

Plan 3 (3:00 - 3:30) :
- Zoom sur un trantorian qui atteint level 8 → confetti / effet spécial
- Compteur teams updates
- Voiceover : *"Premier joueur niveau 8 ! Quand 6 joueurs d'une même team atteignent ce niveau, la team gagne."*

### 3:30 - 4:30 — Bonus

Plan 1 (3:30 - 3:50) :
- Switch vue 2D planisphère
- Click sur trantorian → panel droit Player Info animé
- Voiceover : *"La vue 2D déroule le torus en planisphère. HUD ImGui dockable avec teams, inventaires, timeline."*

Plan 2 (3:50 - 4:10) :
- Switch sur Grafana browser
- Panels qui bougent live
- Voiceover : *"Le serveur expose des métriques Prometheus. Dashboard Grafana avec ticks/sec, latences, broadcasts, incantations."*

Plan 3 (4:10 - 4:30) :
- Replay reader, timeline scrub
- Speed 16x → 0.25x slow-motion d'un moment intense
- F3 debug panel apparaît
- Voiceover : *"Replay binaire `.zrec`. Timeline scrub, slow-motion, debug panel pour profiler en live."*

### 4:30 - 4:50 — AI training

- W&B charts qui évoluent (loss, win rate vs ref)
- Streamlit ELO leaderboard
- Texte sur écran : `Win rate vs zappy_ref: 73%` (chiffre réel)
- Voiceover : *"Le training utilise RLlib PPO multi-agent avec curriculum learning et self-play. ELO tracking entre checkpoints. Notre modèle final atteint 73% de win rate contre le serveur de référence Epitech."*

### 4:50 - 5:00 — Outro

- Photo / logo équipe
- Texte : "github.com/<org>/G-YEP-400-RUN-4-1-zappy-3"
- "Documentation : <docs.url>"
- "Merci !"
- Fade-out musique

## Production checklist

### Pre-production (S3-S4)
- [ ] Story-board validé par P6
- [ ] Script voiceover rédigé (si applicable)
- [ ] Musique licensable choisie (royalty-free, ex: incompetech.com)
- [ ] Scenes cinematic camera mode tournées (P4 + P3, jeudi 18 juin AM)

### Capture (J-2 mercredi 17 juin, après répet)
- [ ] OBS Studio configuré (1080p+, 60 FPS, h264, qualité haute)
- [ ] Microphone testé (si voiceover live)
- [ ] Pas de notifications / Discord visible

### Montage (J-1 jeudi 18 juin AM)
- [ ] Imports OBS clips
- [ ] Découpe + montage selon plan ci-dessus
- [ ] Voiceover enregistré + ajusté
- [ ] Musique en arrière-plan (volume bas)
- [ ] Texts sur écran (timestamps clés)
- [ ] Color grading léger
- [ ] Export MP4

### Validation (J-1 jeudi 18 juin AM)
- [ ] Vue collective par les 6
- [ ] Re-tournage si quelque chose ne va pas
- [ ] Backup sur 2 USB + cloud

### Sécurisation jour J
- [ ] Vidéo sur laptop principal
- [ ] Vidéo sur laptop secondaire (P4)
- [ ] Vidéo sur USB
- [ ] Vidéo sur cloud (lien partageable)

## Versions

- `assets/demo_v1.0.0.mp4` : version officielle pour la soutenance
- `assets/demo_v1.0.0_short.mp4` : version 1-min teaser (pour LinkedIn / Twitter)
- `assets/demo_v1.0.0_silent.mp4` : version sans audio (utile si bug audio jour J)

## Licensing / créditing

- Musique : préciser source dans description vidéo + credits écran
- Modèles 3D : license (Kenney CC0, etc.) dans `gui/assets/LICENSES.md`
- Outils utilisés : OBS, DaVinci Resolve, etc. en credits
