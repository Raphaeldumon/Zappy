# P2 — Marc, Server Dev — Réseau & Ops — Calendrier 20 jours

**Mission rappel** : couche réseau (poll/asio), parser protocole AI+GUI, mode admin/spectator, recorder `.zrec`, observabilité Prometheus+Grafana.

**Zones owned** : `server/runtime/network_layer.{hpp,cpp}`, `server/runtime/protocol_*.{hpp,cpp}`, `server/runtime/recorder.{hpp,cpp}`, `server/runtime/metrics.{hpp,cpp}`, `grafana/**`, `prometheus/**`, `docs/01_architecture/06_protocols.md`.

---

## S1 — Foundations

### D1 — lun 25/05
- `.github/CODEOWNERS`, `PULL_REQUEST_TEMPLATE.md`, ISSUE templates
- `ci.yml` minimal : commitlint + checkout
- Tester commit invalide → CI rouge ; commit valide → vert
- **DoD** : PR template visible, commitlint actif
- Dépendances : aucune
- Bloque : tous les devs (PR rules)

### D2 — mar 26/05
- `ci.yml` matrix Ubuntu+Fedora × gcc+clang, build + ctest skeleton
- ADR-019 (CI strategy)
- **DoD** : 4 jobs CI verts sur fake PR
- Bloque : tous les devs

### D3 — mer 27/05 — M1
- Sync protocole 10h-11h
- Spec complète `docs/01_architecture/06_protocols.md`
- Parser AI : split lignes, dispatch table commande → handler stub
- Test `test_protocol_ai_parser.cpp`
- ADR-010 (Prometheus labels)
- **DoD** : parser 100% commandes sujet
- Dépendances : stubs ops de P1
- Bloque : aucun

### D4 — jeu 28/05
- NetworkLayer asio init + accept loop minimal (echo)
- Test `test_network_echo.cpp`
- ADR-009 (asio standalone)
- **DoD** : `nc localhost 4242` reçoit echo
- Dépendances : aucune
- Bloque : P5 (eval pipeline pourrait avoir besoin)

### D5 — ven 29/05 — M2
- Handshake AI complet (WELCOME → TEAM_NAME → CLIENT_NUM → X Y)
- Test `test_handshake.cpp`
- Demo Friday + retro
- **DoD** : handshake conforme sujet
- Bloque : P5 (eval AI)

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Complete `protocol_ai::dispatch` (Forward, Right, Left, Look, Inventory)
- Command queue par client (max 10 sujet)
- Test `test_protocol_ai_dispatch.cpp`
- **DoD** : 5 commandes E2E
- Dépendances : ops `WorldState` de P1
- Bloque : P5 (env Python utilise serveur réel pour test)

### D7 — mar 2/06
- Compléter dispatch (Broadcast, Connect_nbr, Fork, Eject, Take, Set, Incantation)
- Implémenter émission GUI : pnw, ppo, plv, pin, pgt, pdr, pic, pie, pfk, pdi, enw, ebo, edi, pex, pbc
- Tests
- **DoD** : 100% commands AI + 100% emit GUI
- Bloque : P4 (gui_client parse les émissions)

### D8 — mer 3/06
- Recorder `.zrec` : write header + frames
- Outil CLI `tools/zrec_inspect.cpp`
- Tests `test_recorder_roundtrip.cpp`
- **DoD** : `./zappy_server --record game.zrec` produit fichier valide

### D9 — jeu 4/06
- Broadcast direction sphérique (shortest path, 8 dir)
- `pbc` émis avec direction
- Tests directions cardinales + torus wrap
- **DoD** : broadcast directionality conforme

### D10 — ven 5/06 — M3
- 5 scénarios YAML integration PASS dans CI
- Demo Friday + retro
- **DoD** : `tools/run_integration.sh` PASS

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Admin socket (port `-p+1000`), token auth `--admin-token`
- Commandes : pause, resume, set f, kill player, spawn res, snapshot
- Tests `test_admin_protocol.cpp`
- **DoD** : telnet + auth + cmds OK
- Bloque : P6 (Grafana panels)

### D12 — mar 9/06
- Prometheus exporter (`prometheus-cpp`)
- Métriques : ticks_total, players_alive, actions_processed, action_latency, bytes_sent, active_clients, incantations_total
- **DoD** : `curl localhost:9090/metrics` Prometheus format
- Bloque : P6 (Grafana dashboard)

### D13 — mer 10/06
- Spectator mode : `TEAM_NAME = SPECTATOR` reçoit flux GUI read-only
- Tests `test_spectator.cpp`
- **DoD** : 2 GUIs spectator simultanés voient même état

### D14 — jeu 11/06
- Tests d'intégration étendus : 20+ scénarios YAML
- Catégories : broadcast, eject, fork, incantation success, death, food, vision wrap
- **DoD** : 20 scénarios PASS en CI

### D15 — ven 12/06 — M4
- Buffer + Prometheus Alertmanager config basique
- Doc admin protocol
- Demo Friday + retro
- **DoD** : M4 PASS, observabilité complète

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Stress test réseau (100 GUI spectators)
- Investigate latence bytes/sec
- **DoD** : pas de dégradation

### D17 — mar 16/06
- Bug fixing protocole + intégration
- Tests régression
- **DoD** : 0 P0/P1 protocole

### D18 — mer 17/06 — Code freeze
- Matin : bug fixes
- 14h-17h : Répétition complète
- Rôle soutenance : Q&A protocole / réseau

### D19 — jeu 18/06
- Doc finale `06_protocols.md`
- README network section
- Slides "Réseau + Protocoles" avec P6
- 2ème répétition

### D20 — ven 19/06 — Soutenance
- 9h-11h : préparation
- Soutenance : Q&A protocole, support démo serveur de P1
- **DoD** : soutenance livrée 🎉

---

## Sync points clés

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D1 | Tous | PR rules / CI baseline |
| D3 | Tech-leads + P1 | Protocole figé |
| D6-D7 | P1 | Dispatch ops |
| D6-D7 | P4 | GUI events format |
| D8 | P4 | `.zrec` format |
| D11 | P1, P6 | Admin reload-config |
| D12 | P6 | Métriques noms / labels |
| Chaque PR `protocol_*` | P1, P3, P4, P5 | Coordination contrats |

## Outils / setup spécifiques

- `tcpdump`, `wireshark` pour debug réseau
- `prometheus` + `grafana` Docker pour dev local
- `ab` (Apache Bench) ou `wrk` pour stress test
- IDE : VS Code + clangd

## Auto-évaluation

| Sprint | Score (1-5) | Notes |
|--------|-------------|-------|
| S1 | __ | |
| S2 | __ | |
| S3 | __ | |
| S4 | __ | |
