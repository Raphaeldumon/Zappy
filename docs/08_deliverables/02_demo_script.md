# 02 — Script démo live soutenance

**Durée cible** : 7-10 minutes pour la démo live (hors slides).
**Acteurs** :
- P6 (présentateur principal, transitions)
- P1 (lance serveur + AIs)
- P4 (lance GUI, interagit visuellement)
- P3 (commentaire Vulkan en arrière-plan)
- P5 (commentaire IA en arrière-plan)
- P2 (Q&A protocole prête)

**Setup** :
- Machine de démo Linux Ubuntu 22.04, écran 1920x1080 ou 4K externe
- `zappy-v1.0.0/` déployé, tar.gz extracté
- Terminal tmux pré-configuré avec 4 panes : serveur, gui, ais, monitoring
- Browser ouvert sur Grafana (http://localhost:3000) + Streamlit ELO (http://localhost:8501)
- Vidéo démo prête en fenêtre 2 (backup)

---

## Étape 0 — Setup pre-démo (30 sec, silencieux)

1. Killer processus existants : `pkill -f zappy_server || true; pkill -f zappy_ai || true; pkill -f zappy_gui || true`
2. `cd ~/zappy-v1.0.0`
3. Lancer Prometheus + Grafana : `docker compose -f docker/observability.yml up -d`
4. Vérifier Grafana accessible

## Étape 1 — Intro 30 sec (P6 parle)

> *"Bonjour, on est l'équipe Zappy. Notre projet implémente intégralement le sujet G-YEP-400 : un jeu réseau multi-équipes en C++ avec un serveur autoritatif, un client graphique 3D en Vulkan 1.3, et un client IA entraîné en RL multi-agent avec PyTorch et RLlib. On vous propose une démo live, puis on revient sur l'architecture et les choix techniques."*

## Étape 2 — Lancer le serveur (45 sec) — P1

P1 dans pane 1 du tmux :

```bash
./zappy_server \
    -p 4242 -x 50 -y 50 \
    -n red blue green yellow \
    -c 6 -f 500 \
    --record demo_$(date +%s).zrec \
    --admin-token "demo-token" \
    --metrics-port 9090
```

> *"On lance le serveur sur une map 50x50, 4 teams de 6 joueurs, fréquence 500 ticks/sec, avec enregistrement replay et endpoint admin."*

Vérifier dans Grafana (browser) : panel `zappy_active_clients` à 0, `zappy_tick_total` qui augmente.

## Étape 3 — Lancer le GUI 3D (1 min) — P4

P4 dans pane 2 :

```bash
./zappy_gui -p 4242 -h localhost
```

> *"Le client graphique se connecte, reçoit le snapshot complet de la map, et affiche la planète torus dans l'espace. Vous voyez l'atmosphère scattering qui crée ce halo lumineux, le ciel procedural avec ses étoiles et la nébuleuse, et les ressources réparties sur la surface."*

Actions UI live :
- **Tab** → basculer vue (3D / 2D planisphère / Split / Top-down) → *"Voici la vue 2D planisphère qui déroule le torus."*
- **F3** → debug panel → *"Le debug panel montre 60 FPS stables, l'occupation GPU, le nombre de paquets reçus, les draw calls."*
- Souris orbital, scroll zoom → *"Caméra free orbit en GLFW + GLM."*

## Étape 4 — Lancer les AIs (1 min) — P5

P5 dans pane 3 :

```bash
./tools/launch_demo_ais.sh
# qui fait :
# for team in red blue green yellow; do
#   for i in {1..6}; do
#     ./zappy_ai -p 4242 -n $team -h localhost --model models/current/model.pt &
#   done
# done
```

> *"On lance 24 AIs au total, 6 par team. Chacun charge le modèle PPO entraîné via libtorch C++, l'inférence prend moins de 2 ms par décision."*

Dans le GUI :
- **Click sur un trantorien** → panel droit Player Info → *"Voici l'inventaire et la position du joueur sélectionné."*
- **F suit player** → caméra follow → *"Caméra follow."*
- Observer les broadcasts en bas timeline → *"Les broadcasts sont codés intra-team (XOR magic byte). Les AIs adverses voient les paquets mais ne peuvent pas les décoder."*

## Étape 5 — Speed up + observations (1 min) — P4

> *"Le speed control permet d'observer l'évolution accélérée."*

- Slider speed → **4x**, puis **8x**, puis **16x**
- Observer dans la timeline : élévations qui se déclenchent, morts, naissances
- **Premier passage à level 8** → *"Et voilà la première élévation au niveau 8 par la team red. Le serveur va déclencher la victoire si une team atteint 6 joueurs au niveau 8."*

## Étape 6 — Mode admin (30 sec) — P1

P1 dans pane 4 :

```bash
nc localhost 5242
auth demo-token
set f 1000
spawn thystame 25 25 5
pause
resume
```

> *"L'admin socket permet de modifier le jeu en runtime : changer la fréquence, spawn des ressources, pause/resume. Utile pour debug et observation."*

## Étape 7 — Grafana monitoring (45 sec) — P6

Switch sur browser Grafana :

> *"Le serveur expose des métriques Prometheus. Voici le dashboard temps réel : ticks/sec, latence des actions, joueurs vivants par team, broadcasts/sec, incantations success/fail."*

Pointer panels live qui bougent.

## Étape 8 — Replay (45 sec) — P4

Stop AIs : `pkill zappy_ai`. Stop GUI.

```bash
./zappy_gui --replay demo_*.zrec
```

> *"Le format replay .zrec capture tout le flux protocole GUI. On peut rejouer hors-ligne avec scrub timeline."*

- Slider timeline scrub → *"On peut sauter à n'importe quel moment."*
- Speed control 16x → *"Replay accéléré."*

## Étape 9 — IA vs Référence (45 sec) — P5

Switch terminal, lancer :

```bash
python -m zappy_train.training.eval \
    --opponent ref \
    --model models/current/model.pt \
    --matches 10 \
    --report eval_demo.json
```

Pendant que ça tourne (~30s) :

> *"L'évaluation lance 10 matches contre le serveur de référence Epitech. Notre IA RL a un win rate de XX% sur 100 matches déjà mesurés, documenté dans models/current/eval_report.md."*

Lecture du output finale : `Win rate: X/10`.

## Étape 10 — Streamlit ELO (30 sec) — P5

Switch browser sur Streamlit :

> *"Pendant le training, le pipeline self-play maintient un classement ELO entre checkpoints. On voit ici la progression du modèle final qui domine progressivement les checkpoints intermédiaires."*

## Étape 11 — Conclusion démo (15 sec) — P6

> *"Voilà pour la démo live. On passe à la présentation pour parler de l'architecture et des choix techniques."*

→ Transition vers slides.

---

## En cas de problème

### Si le serveur crash au démarrage
→ P1 relance avec args minimum (`-x 10 -y 10 -n red -c 4 -f 100`). Garder le sourire.

### Si le GUI crash
→ P6 dit : *"Plot twist habituel des démos GPU live, on bascule sur la vidéo enregistrée"* → P4 lance la vidéo démo en plein écran.

### Si la connexion AI échoue
→ P5 : *"Le modèle RL est exigeant en CPU, on bascule sur l'IA rule-based"* :
```bash
./zappy_ai -p 4242 -n red -h localhost --no-model
```

### Si réseau / port occupé
→ P1 : `lsof -i :4242 | xargs kill`, relance sur port 4243 et adapte le GUI.

### Si total crash machine
→ P6 : *"On va vous montrer la vidéo démo et reprendre la présentation."* Vidéo plein écran depuis P4 laptop secondaire.

---

## Timing global

| Étape | Durée |
|-------|-------|
| 0 Setup silencieux | 30 s |
| 1 Intro | 30 s |
| 2 Lancer serveur | 45 s |
| 3 Lancer GUI 3D | 1 min |
| 4 Lancer AIs | 1 min |
| 5 Speed up | 1 min |
| 6 Admin | 30 s |
| 7 Grafana | 45 s |
| 8 Replay | 45 s |
| 9 IA vs Ref | 45 s |
| 10 Streamlit ELO | 30 s |
| 11 Conclusion démo | 15 s |
| **Total démo** | **~9 min** |

## Pré-vérifications le matin de la soutenance

- [ ] Tar.gz extracté, `make` re-run pour fraîcheur
- [ ] Grafana + Prometheus containers running
- [ ] Streamlit lancé en background
- [ ] `models/current/model.pt` présent
- [ ] Vidéo démo testée (fenêtre 2)
- [ ] Replay sample présent
- [ ] Pas de notifications Discord / Slack à l'écran
- [ ] Volume sonore réglé (musique GUI audible mais pas trop)
- [ ] Police terminal grande (16pt min, lisible jury)
