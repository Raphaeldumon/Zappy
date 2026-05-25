# Sprint 3 (W3) — Bonus + intégration

**Dates** : lundi 8 juin → vendredi 12 juin 2026
**Thème** : Bonus features (admin, prometheus, atmosphere, particles, curriculum, self-play). Training RL massif. Intégration cross-team renforcée.

## Goals du sprint

1. **Server bonus complets** : admin/spectator socket, Prometheus exporter, Grafana dashboard, hot-reload config CLI
2. **GUI bonus** : skybox étoiles + nebula propre, atmosphere scattering, water/lava biomes (si on garde), HUD complet (timeline, broadcasts), debug panel F3 complet, replay reader avec UI timeline scrub
3. **Audio engine** : musique ambient + SFX
4. **AI training intensif** : curriculum stages 1-3 traversés, self-play actif, ELO tracking opérationnel
5. **Broadcast codé team** intégré et utilisable
6. **Inférence libtorch C++** rapide et fiable
7. **Tests d'intégration** étendus (20+ scénarios YAML)
8. **Convergence preuve** : 1ère policy trainée bat la rule-based v1 sur >= 50% des matches

## Goals NON dans S3

- Ray tracing (S4)
- Post-FX final (bloom, TAA, tonemap, SSAO seulement basique)
- Polish visuel final

---

## D11 — Lundi 8 juin

### 9h30-10h30 : Sprint planning S3

### P1
- **Tâche** : Hot-reload config CLI : commande admin `reload-config <path>` applique nouvelles densités au prochain respawn
- Refactor pour découpler config statique vs runtime
- **Outputs** : hot-reload fonctionnel
- **Critère** : changer densité → vérifié au respawn suivant

### P2
- **Tâche** : Admin socket (port `-p+1000`), token auth `--admin-token`
- Commandes : pause, resume, set f, kill player, spawn res, snapshot
- Tests `test_admin_protocol.cpp`
- **Outputs** : admin opérationnel
- **Critère** : telnet localhost:5242 + auth + cmds OK

### P3
- **Tâche** : Atmosphere scattering shader (Rayleigh+Mie ray-march)
- Sun direction param + halo lumineux autour du torus
- **Outputs** : atmosphère visible quand on regarde le torus depuis l'espace
- **Critère** : visuellement convaincant, look "planète vue de l'espace"

### P4
- **Tâche** : Skybox étoiles + nebula propres (vrai shader procedural FBM 3D coloré)
- Timeline ImGui : log des events significatifs (incantations, morts, broadcasts)
- **Outputs** : ciel beau + timeline scroll dans HUD
- **Critère** : timeline réactive aux events serveur live

### P5
- **Tâche** : Curriculum stage 1 (small 20x20) lance training en SSH sur machine training, en parallèle laisse stage 0 finir
- ELO tracking commence (top 5 checkpoints)
- W&B custom charts pour ELO
- **Outputs** : training stage 0+1 en cours, ELO dashboard live
- **Critère** : 2 checkpoints stage 0 différents ELO scorent

### P6
- **Tâche** : Compléter `zappy_ai` : intégration broadcast codé team (encode quand émet, decode quand reçoit)
- Performance inférence libtorch : profile, viser < 2ms forward
- **Outputs** : `zappy_ai` complet avec broadcasts codés
- **Critère** : 2 AIs same team coordonnent visiblement (mesurable via test)

---

## D12 — Mardi 9 juin

### Standup

### P1
- **Tâche** : Tests stress server : 4 teams × 6 players, map 50x50, 10 min run
- Profile RSS et CPU, fix éventuels leaks/blocages
- **Outputs** : server stable sous charge
- **Critère** : RSS < 300 MB, 0 crash en 10 min

### P2
- **Tâche** : Prometheus exporter intégré (`prometheus-cpp` lib)
- Métriques : ticks_total, players_alive, actions_processed, action_latency, bytes_sent, active_clients, incantations_total
- **Outputs** : `/metrics` HTTP 9090 répond
- **Critère** : `curl localhost:9090/metrics` retourne plain text Prometheus format

### P3
- **Tâche** : SSAO pass basique (32 samples, blur cross 4x4)
- TAA pass basique (reproject prev frame + clip)
- **Outputs** : SSAO + TAA actifs
- **Critère** : screenshot before/after montre SSAO sous trantorians, anti-aliasing

### P4
- **Tâche** : Debug panel F3 complet : frametime graph 240 frames, GPU mem (VMA stats), packet count, draw call count, mesh count
- **Outputs** : F3 panel utile pour profiler
- **Critère** : peut diagnostiquer un drop FPS via le panel

### P5
- **Tâche** : Reward fix selon observations stage 0 (encourage co-location pour incantations team)
- Stage 2 (medium 50x50, 4 teams) commence training
- **Outputs** : training avance
- **Critère** : eval intermédiaire montre progression

### P6
- **Tâche** : Grafana dashboard JSON `grafana/zappy_overview.json` : panels pour chaque métrique
- `docker-compose.yml` : ajout services Prometheus + Grafana pour dev local
- **Outputs** : `docker compose up grafana` montre dashboard
- **Critère** : 8 panels remplis avec data live du serveur

---

## D13 — Mercredi 10 juin

### Standup

### P1
- **Tâche** : Death management : player meurt → libère slot, `pdi n` émis, connexion fermée propre
- Egg hatching : connexion sur slot libre → choose random egg → spawn at egg pos, orient random
- **Outputs** : lifecycle complet
- **Critère** : run partie complète, observations cohérentes côté GUI

### P2
- **Tâche** : Spectator mode : connexion `TEAM_NAME = SPECTATOR` reçoit le flux GUI (read-only)
- Tests
- **Outputs** : spectator opérationnel
- **Critère** : 2 GUIs spectator simultanés voient même état

### P3
- **Tâche** : Tonemap ACES + Bloom (compute downsample/upsample) basique
- **Outputs** : pipeline post-FX 80% complet
- **Critère** : highlight bloom visible sur particules brillantes

### P4
- **Tâche** : Replay reader UI : timeline ImGui scrub, play/pause, speed control
- Charge `.zrec`, parcourt events
- **Outputs** : replay fonctionnel
- **Critère** : peut scrub à n'importe quel moment d'une partie enregistrée

### P5
- **Tâche** : Self-play tournament : 10 matches/heure entre top 5 ELO, mise à jour ratings
- Dashboard Streamlit ELO leaderboard
- **Outputs** : self-play tourne 24/7
- **Critère** : ELO différencié émerge entre checkpoints

### P6
- **Tâche** : Audio engine (miniaudio) : load OGG music, WAV SFX (incantation, death, broadcast, fork)
- API simple `audio.play_sfx("incantation")`, `audio.play_music("ambient.ogg")`
- Volumes configurables
- **Outputs** : audio joue
- **Critère** : SFX joue sur events GUI, musique boucle

---

## D14 — Jeudi 11 juin

### Standup

### P1
- **Tâche** : Documentation API `server/core/` : doxygen complet
- Buffer / bug fixing
- **Outputs** : doxygen 80%+ commenté sur core
- **Critère** : `make docs` build sans warning

### P2
- **Tâche** : Tests d'intégration étendus : 20+ scénarios YAML (broadcast, eject, fork, incantation niveaux 1-3 success, death, food starvation, vision wrap)
- **Outputs** : suite intégration robuste
- **Critère** : 20 scénarios PASS en CI

### P3
- **Tâche** : LOD trantorian mesh (3 niveaux) + frustum culling
- Optimisation draw calls
- **Outputs** : perf scène large améliorée
- **Critère** : 60 FPS @ 1440p avec 100+ trantorians

### P4
- **Tâche** : Polish HUD : menu principal (Connect / Replay / Settings / Quit)
- Settings persistants (JSON)
- **Outputs** : menu fonctionnel
- **Critère** : retour menu sans crash, settings sauvegardés

### P5
- **Tâche** : Curriculum stage 3 (ref-size 50x50 f=100, eval vs ref-server)
- Premier eval réel vs zappy_ref : win rate baseline
- **Outputs** : win rate concret vs ref
- **Critère** : chiffre publié, plan ajustement réward si < 30%

### P6
- **Tâche** : Performance tuning libtorch : batch inference si possible (1 forward pour tous les agents d'une team)
- ASan run sur `zappy_ai` 30 min → 0 leaks
- **Outputs** : `zappy_ai` rapide et propre
- **Critère** : forward < 2 ms median, ASan clean

---

## D15 — Vendredi 12 juin (M4 : bonus done)

### Standup

### P1
- **Tâche** : Buffer + final tests + coverage push
- **Outputs** : `gcovr` ≥ 70% server core
- **Critère** : rapport coverage produit dans CI artifact

### P2
- **Tâche** : Buffer + Prometheus alerts (config Alertmanager basique)
- Doc admin protocol dans `docs/01_architecture/06_protocols.md`
- **Outputs** : observabilité complète
- **Critère** : Grafana montre 8 panels live

### P3
- **Tâche** : Buffer + investigation RT setup (pour S4)
- Doc shaders dans `docs/01_architecture/03_gui_vulkan.md`
- **Outputs** : prêt à attaquer RT lundi
- **Critère** : ADR-007 (RT fallback) décidé en retro vendredi

### P4
- **Tâche** : Buffer + animation simple trantorian (idle + walk)
- **Outputs** : trantorians vivants visuellement
- **Critère** : démo manuel "vivant"

### P5
- **Tâche** : Eval intensif weekend run (training 48h sur machine)
- Configure curriculum stage 4 (advanced)
- **Outputs** : training run weekend lancé
- **Critère** : 100k samples/sec atteint

### P6
- **Tâche** : Buffer + release internal `v0.3.0-rc1` packagé (tar.gz pour démos)
- **Outputs** : release candidate testable
- **Critère** : Yanis peut envoyer le tar.gz à un externe qui le run

### 16h-17h : Demo Friday S3 — **M4 : démo bonus**

Scénario :
1. Démarrage server avec record + admin socket + Prometheus
2. GUI live : scène 3D complète (atmosphère, étoiles, particles, SSAO, TAA, bloom basique)
3. Switch 2D planisphère / 3D / split / top-down
4. HUD complet : teams, inventaires, timeline, broadcasts
5. Show admin : pause, set f, spawn res
6. Show Grafana dashboard
7. Show training W&B + Streamlit ELO leaderboard
8. Load replay `.zrec`, scrub timeline
9. Show audio (music + SFX live)

### 17h-17h45 : Retro S3

### Checkpoint M4 — Definition of Done sprint
- [ ] Admin/spectator opérationnel **DONE**
- [ ] Prometheus + Grafana **DONE**
- [ ] Hot-reload config CLI **DONE**
- [ ] Atmosphere scattering, SSAO, TAA, Bloom basique **DONE**
- [ ] Skybox étoiles + nebula propre **DONE**
- [ ] HUD complet (timeline, broadcasts, menu) **DONE**
- [ ] Debug F3 panel complet **DONE**
- [ ] Replay UI timeline scrub **DONE**
- [ ] Audio engine (music + SFX) **DONE**
- [ ] Training curriculum stages 0-3 traversés **DONE**
- [ ] Self-play + ELO actif **DONE**
- [ ] Broadcast codé team intégré **DONE**
- [ ] `zappy_ai` libtorch < 2ms forward **DONE**
- [ ] 20+ scénarios YAML intégration PASS **DONE**
- [ ] Coverage ≥ 70% C++ core, ≥ 80% Python **DONE**
- [ ] Release candidate `v0.3.0-rc1` packagée **DONE**

Si tout ✅ → **M4 PASSÉ**, S4 peut commencer le polish + soutenance.
