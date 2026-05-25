# Sprint 4 (W4) — Polish + soutenance

**Dates** : lundi 15 juin → vendredi 19 juin 2026
**Thème** : finalisation. Ray tracing (si feasible), post-FX final, training final, **code freeze mercredi 17**, répétition, soutenance vendredi.

## Goals du sprint

1. **Ray tracing** : pipeline RT reflets + soft shadows (si ADR-007 GO en fin S3) — sinon polish raster
2. **Post-FX final** : TAA stable, Bloom raffiné, tonemap ACES propre
3. **Training final** : meilleur checkpoint sélectionné, exporté, packagé dans release
4. **Eval finale** : score win rate vs ref-server publié (cible 70%+)
5. **Polish** : bugfixes, visuels finaux, audio mix, copies UX
6. **Documentation finale** : MkDocs complet, ADRs, README, CHANGELOG
7. **Release `v1.0.0`** taggée, packagée
8. **Vidéo démo 5 min** enregistrée et montée
9. **Slides soutenance** prêts
10. **Répétition complète J-2** (mercredi)
11. **Soutenance vendredi 19 juin**

## Code freeze : mercredi 17 juin 18h

Après code freeze : **uniquement bugfix critiques** mergés. Tout autre changement bloqué.

---

## D16 — Lundi 15 juin

### 9h30-10h30 : Sprint planning S4
- Priorité absolue : tout ce qui finit le projet
- Travail en pair pour réduire risque
- Rappel : code freeze mercredi 18h

### P1
- **Tâche** : Stress test final server (4 teams × 6 players, map 200x200, f=500, 30 min)
- Profile + optimize cold spots (hot path)
- **Outputs** : 500 ticks/sec atteint, RSS < 300 MB
- **Critère** : benchmark documenté

### P2
- **Tâche** : Stress test réseau (100 GUI spectators connectés)
- Investigate latence bytes/sec
- **Outputs** : pas de dégradation
- **Critère** : 4 teams × 6 + 10 spectators stable

### P3
- **Tâche** : RT pipeline implementation (si GO) : reflection sur torus surface metallic
- TLAS/BLAS construction
- ray_trace shader (rgen/rmiss/rchit)
- **Outputs** : reflets visibles sur torus
- **Critère** : RenderDoc capture montre RT pipeline OK

### P4
- **Tâche** : Animation skinned trantorian (walk, attack, death) si glTF rigging dispo
- Particles refinement : incantation glow effect, ejection wave
- **Outputs** : visuels finaux
- **Critère** : look "production"

### P5
- **Tâche** : Sélection best checkpoint via ELO leaderboard + manual eval
- Lance final training run 24h
- **Outputs** : modèle candidat pour release
- **Critère** : win rate vs ref baseline mesurable

### P6
- **Tâche** : Release prep : version bump, CHANGELOG draft, validation `make` aux normes Epitech
- Trial Docker image release `ghcr.io/.../zappy:1.0.0-rc1`
- **Outputs** : packaging prêt
- **Critère** : externe peut `docker pull` + run

---

## D17 — Mardi 16 juin

### Standup

### P1
- **Tâche** : Bug fixing prio P0/P1 (issues GitHub labels)
- Coverage push > 75% C++ core
- **Outputs** : 0 P0/P1 ouverts en fin de journée
- **Critère** : board Kanban clean

### P2
- **Tâche** : Bug fixing protocole + intégration
- Tests d'intégration ajoutés pour bugs trouvés
- **Outputs** : 0 P0/P1 protocole
- **Critère** : 100% integration scénarios PASS

### P3
- **Tâche** : TAA final tuning (history reject, neighborhood clip)
- Bloom final tuning (downsample threshold, upsample tint)
- Tonemap ACES propre
- **Outputs** : post-FX visuels excellents
- **Critère** : reference screenshots final

### P4
- **Tâche** : Polish HUD final : icons polished, fonts définis, layout final docking
- Audio mix final (volumes, ducking quand event important)
- **Outputs** : UX léchée
- **Critère** : démo manuelle "ça respire la qualité"

### P5
- **Tâche** : Export best checkpoint en TorchScript `model.pt`, versionné git-lfs
- Eval final exhaustive : 100 matches vs `zappy_ref`, statistiques publiées
- **Outputs** : modèle final + rapport eval
- **Critère** : `models/current/model.pt` + `models/current/eval_report.md`

### P6
- **Tâche** : Préparation slides soutenance (plan voir `docs/08_deliverables/03_soutenance_slides_plan.md`)
- Script démo vidéo
- **Outputs** : slides draft + storyboard vidéo
- **Critère** : slides reviewed par tous

---

## D18 — Mercredi 17 juin (M5 : code freeze 18h)

### Standup

### P1, P2, P3, P4, P5, P6
- Matin : **dernier sprint bugfixing** (8h-12h)
- Midi : déjeuner équipe (célébration intermédiaire)
- Après-midi (14h-17h) : **répétition complète E2E** sur machine de démo
  - Démontage envrionnement, install propre du tar.gz release
  - Suit le script `docs/08_deliverables/02_demo_script.md` à la lettre
  - Identifier les bugs jour-J potentiels
  - Backup : enregistrement vidéo de la répétition (sera notre vidéo démo)
- 17h-18h : **derniers commits** autorisés (uniquement P0 bugfix)
- **18h00 : CODE FREEZE** — tag `v1.0.0-rc1`, branche `release/1.0.0` créée

### Tâches en parallèle
- P3, P4 : tournage vidéo démo additionnelle (séquences cinematic caméra)
- P5 : eval supplémentaire si temps (training continue en background)
- P6 : finalize slides, repérer salle soutenance, vérifier matos

### 18h-19h : Retro mid-week
- Évaluation préparation soutenance
- Que reste-t-il à faire jeudi (poli, doc, slides) ?

---

## D19 — Jeudi 18 juin

### Standup

Aucune feature, aucun bugfix non-critique. **Polish + préparation soutenance uniquement**.

### P1, P2
- **Tâche** : Documentation finale `docs/01_architecture/02_server.md`, `06_protocols.md`
- README serveur, exemples CLI, usage Prometheus
- Préparer slides "Architecture serveur" (avec P6)

### P3, P4
- **Tâche** : Documentation finale `docs/01_architecture/03_gui_vulkan.md`
- README GUI, captures d'écran finales
- Préparer slides "GUI Vulkan + Bonus" (avec P6)
- Montage vidéo démo final 5 min (voiceover si désiré)

### P5
- **Tâche** : Documentation finale `docs/01_architecture/04_ai_rl.md`
- README AI + training reproductibilité instructions
- Préparer slides "AI + Training pipeline" (avec P6)
- Rapport eval final propre

### P6
- **Tâche** : Mise à jour MkDocs final, déploiement GitHub Pages
- `CHANGELOG.md` finalisé
- Tag `v1.0.0` officiel
- Release GitHub packagée
- Vérification CI verte sur le tag
- Slides global compilées
- **Liste matériel soutenance** (laptops, backup, clé USB, adaptateurs, cable hdmi)

### 14h-17h : 2ème répétition complète
- Suivre script soutenance
- Mesurer le timing exact (cible 8 min démo + 2 min Q&A préview)
- Identifier les phrases / transitions

### 17h-18h : Préparation matérielle
- Tar.gz copié sur 2 USB (P6 + P3)
- Slides copiées sur 2 USB + cloud Drive
- Vidéo démo copiée sur 2 USB + cloud
- Backup : screenshots prints en cas de panne totale

---

## D20 — Vendredi 19 juin (M6 : soutenance)

### 8h30 : arrivée équipe sur lieu soutenance

### 9h-11h : Préparation finale
- Setup matériel
- Test live démo (1x)
- Test slides + vidéo
- Test fallback (replay `.zrec`)
- Refresh slides si besoin (correction typo, etc.)

### Soutenance — horaire confirmé Epitech

Plan détaillé : voir [`docs/08_deliverables/02_demo_script.md`](../../08_deliverables/02_demo_script.md) et [`docs/08_deliverables/03_soutenance_slides_plan.md`](../../08_deliverables/03_soutenance_slides_plan.md).

**Format type** :
- 1-2 min : introduction
- 5-7 min : démo live (server + GUI + AI)
- 8-15 min : présentation slides (architecture, choix techniques, bonus, perf, RL)
- 5-10 min : Q&A jury

### Rôles soutenance

| Rôle | Personne |
|------|----------|
| Présentateur principal | P6 (coordinateur, transitions) |
| Démo serveur + AI | P1 |
| Démo GUI live | P4 |
| Slides driver | P3 |
| Slides AI/RL spécifique | P5 |
| Q&A protocole + réseau | P2 |
| Q&A Vulkan rendering | P3 |
| Q&A AI/RL training | P5 |
| Q&A infra/DevOps | P6 |
| Fallback vidéo si crash | P4 (montre depuis laptop secondaire) |
| Backup tech qui gère hardware | P6 |

### Checklist 5 min avant entrée

- [ ] Serveur démo prêt à lancer (cmd en clipboard)
- [ ] GUI démo prêt
- [ ] 12 AIs démo prêts (script bash launch_demo_ais.sh)
- [ ] Replay `.zrec` chargé en backup window
- [ ] Vidéo démo prête à play
- [ ] Slides ouverts en plein écran
- [ ] Discord muté
- [ ] Téléphones en silencieux
- [ ] Bouteille d'eau
- [ ] Smile

### Après soutenance : célébration équipe 🎉

---

## Code après soutenance (D20+ optionnel weekend)

- Faire le point ensemble sur ce qui marché / pas marché
- Si jury demande modifs : régler
- Push back `develop` → `main` final
- Archive du repo + tag `v1.0.0-final`
- Vidéo démo publiée publiquement (si autorisé)

## Definition of Done sprint S4

- [ ] Ray tracing OK ou drop documenté **DONE**
- [ ] Post-FX final (TAA, Bloom, ACES) **DONE**
- [ ] Training final, best checkpoint exporté + eval report **DONE**
- [ ] `models/current/model.pt` packagé dans release **DONE**
- [ ] CHANGELOG `v1.0.0` complet **DONE**
- [ ] Release tar.gz `v1.0.0` GitHub Releases **DONE**
- [ ] MkDocs déployé, à jour **DONE**
- [ ] Vidéo démo 5 min montée **DONE**
- [ ] Slides soutenance finaux **DONE**
- [ ] 2 répétitions complètes passées **DONE**
- [ ] Soutenance livrée **DONE**

## Bilan attendu

Voir [`docs/08_deliverables/01_final_deliverables.md`](../../08_deliverables/01_final_deliverables.md) pour la checklist "projet à 100%".
