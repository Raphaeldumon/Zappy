# P5 — Théo, AI Lead — RL training — Calendrier 20 jours

**Mission rappel** : env multi-agent PettingZoo, RLlib PPO pipeline, reward shaping, curriculum, self-play + ELO, export TorchScript.

**Zones owned** : `ai_python/zappy_train/**`, `ai_python/tests/**`, `ai_python/notebooks/**`, `docs/01_architecture/04_ai_rl.md`.

---

## S1 — Foundations

### D1 — lun 25/05
- `ai_python/pyproject.toml` deps (torch, ray, pettingzoo, gymnasium, numpy, wandb, tensorboard)
- `ai_python/zappy_train/__init__.py` + structure modules
- Stub `pettingzoo_env.py` (méthodes vides)
- Setup compte W&B, projet créé
- **DoD** : `pip install -e ai_python` OK, `python -c "import zappy_train"` OK, pytest vide PASS
- Dépendances : aucune
- Bloque : aucun

### D2 — mar 26/05
- Architecture `zappy_train/env/` : encode obs (placeholder shape), action space discret (30 actions)
- Stub `reset()/step()` retournent random tensors bons shapes
- Test `test_env_shapes.py`
- **DoD** : env stub fonctionne avec rllib rollout
- Bloque : P6 (binding sim utilise les shapes obs/action)

### D3 — mer 27/05 — M1
- Sync protocole 10h-11h
- Encodage obs/action complet (selon `docs/01_architecture/04_ai_rl.md`)
- `reward.py` reward shaping function avec tests
- ADR-004 (RLlib vs SB3) + ADR-016 (curriculum) + ADR-017 (reward)
- **DoD** : pytest `test_reward.py` 100% PASS
- Bloque : aucun

### D4 — jeu 28/05
- RLlib config PPO multi-agent draft + script `train_ppo.py`
- Run 60sec PPO sur env stub : check pas de crash, samples_per_sec
- Démo W&B
- **DoD** : run 60sec OK, W&B dashboard accessible
- Bloque : aucun

### D5 — ven 29/05 — M2
- Eval pipeline stub : `eval --opponent rule-based-stub` → win rate fake
- W&B + Streamlit dashboard ELO stub
- Demo Friday + retro
- **DoD** : pipeline eval fonctionnel sur stub
- Bloque : aucun

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Connecter `pettingzoo_env` au vrai `zappy_sim` (pour l'instant stub de P6)
- Reward V0 testé avec mock env
- Lancer 1er PPO 30 min sur env stub : baseline win rate
- **DoD** : training tourne, W&B logs visibles, 1k samples/sec debug
- Dépendances : sim stub de P6
- Bloque : aucun

### D7 — mar 2/06
- Lancement PPO sur `zappy_sim` réel (10x10, 4 agents, rule-based opponents bouchon)
- Eval pipeline : tournament mini 10 matches
- **DoD** : training E2E fonctionne, loss chart W&B
- Dépendances : sim de P6 (D6-D7)

### D8 — mer 3/06
- Reward shaping V1 (potential-based shaping)
- Curriculum stage 0 (10x10) lance training nightly
- ELO bootstrap (1500 init)
- **DoD** : curriculum opérationnel, 1ère policy stage 0 > 50% vs random

### D9 — jeu 4/06
- Continuer training, monitor W&B
- Ajouter eval contre `zappy_ref` : démarre ref-server + N AI rule-based, mesure win rate
- **DoD** : 1er eval vs ref publié, chiffre concret

### D10 — ven 5/06 — M3
- 2nd training run avec reward fixes + obs validation
- Vérifier `zappy_ai` (P6) charge model.pt + joue sur server
- Demo Friday + retro
- **DoD** : E2E AI RL → inférence C++ marche (même si mauvais)

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Sprint planning
- Curriculum stage 1 (small 20x20) lance training SSH machine
- ELO tracking (top 5 checkpoints)
- W&B custom charts ELO
- **DoD** : training stages 0+1 en cours, ELO dashboard live

### D12 — mar 9/06
- Reward fix selon obs stage 0
- Stage 2 (medium 50x50, 4 teams) training
- **DoD** : training avance, eval intermédiaire montre progression

### D13 — mer 10/06
- Self-play tournament : 10 matches/h entre top 5 ELO
- Streamlit ELO leaderboard
- **DoD** : self-play tourne 24/7, ELO différencié émerge

### D14 — jeu 11/06
- Curriculum stage 3 (ref-size 50x50 f=100, eval vs ref)
- 1er eval réel vs zappy_ref : win rate baseline
- **DoD** : chiffre publié, plan ajustement reward si < 30%

### D15 — ven 12/06 — M4
- Eval intensif weekend run (training 48h)
- Configure curriculum stage 4
- Demo Friday + retro
- **DoD** : weekend run lancé, 100k samples/sec

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Sprint planning S4
- Sélection best checkpoint via ELO + manual eval
- Lance final training run 24h
- **DoD** : modèle candidat pour release

### D17 — mar 16/06
- Export best checkpoint TorchScript `model.pt`, versionné git-lfs
- Eval final exhaustive : 100 matches vs `zappy_ref`, stats publiées
- **DoD** : `models/current/model.pt` + `eval_report.md`

### D18 — mer 17/06 — Code freeze
- Matin : eval supplémentaire si temps
- 14h-17h : Répétition complète
- **DoD** : tag `v1.0.0-rc1`

### D19 — jeu 18/06
- Doc finale `04_ai_rl.md`
- README AI + reproductibilité instructions
- Rapport eval final propre
- Slides "AI + Training pipeline" avec P6
- 2ème répétition
- **DoD** : doc et slides reviewed

### D20 — ven 19/06 — Soutenance
- 9h-11h : préparation
- Soutenance : slides spécifiques AI/RL + Q&A training
- **DoD** : soutenance livrée 🎉

---

## Sync points clés

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D2-D3 | P6 | Schéma obs/action |
| D3 | Tech-leads | M1 protocole |
| D6-D7 | P6 | Sim bindings ready |
| D9 | P6 | Eval vs ref-server setup |
| D10 | P6 | libtorch loading model |
| D14-D17 | P6 | Final eval pipeline |
| Chaque PR `ai_python/` | P6 | Co-review |

## Outils / setup

- Python 3.11 venv ou poetry
- PyTorch 2.x + Ray 2.x + RLlib
- W&B account
- Tensorboard local
- Streamlit pour ELO dashboard
- Jupyter notebooks pour exploration
- VS Code + Pylance + ruff

## Auto-évaluation

| Sprint | Score (1-5) | Notes |
|--------|-------------|-------|
| S1 | __ | |
| S2 | __ | |
| S3 | __ | |
| S4 | __ | |
