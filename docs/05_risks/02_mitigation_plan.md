# 02 — Plan de mitigation par risque

## R01 — Ray tracing Vulkan : peu de RTX

**Détail** : seuls 2-3 sur 6 devs ont un GPU RTX. P4 (GUI Dev UX) pourrait être affecté pour le test du rendu final.

**Mitigation** :
- **Feature flag CMake** `ZAPPY_HAS_RT=ON/OFF`, détection runtime des extensions `VK_KHR_ray_tracing_pipeline` et `VK_KHR_acceleration_structure`
- **Fallback obligatoire** SSR (screen-space reflections) pour les non-RTX, livré en **S2** (avant que RT soit tenté en S3)
- Pipeline RT travaillé **uniquement** par P3 (qui a RTX)
- Validation du rendu sur machine partagée RTX hebdomadaire (P3 fait tourner sur la machine de training en SSH)
- ADR-007 : si RT pas convergent à fin S3, **DROP** la feature, garder le fallback comme path principal

**Owner** : P3
**Indicateur** : ADR-007 en `Accepted` à fin S2, RT shipping en S4 ou drop décidé en retro fin S3.

## R02 — RL multi-agent ne converge pas

**Détail** : C'est le risque le plus probable et le plus impactant. La convergence d'un PPO multi-agent sur un jeu complexe en 3 semaines de training réel est ambitieuse.

**Mitigation** :
- **IA rule-based v1** garantie pour fin S2. Si RL échoue, on livre la rule-based, qui passe largement la barre Epitech.
- **Curriculum learning** dès S2 : démarrer petites maps (10x10) avec peu d'agents (4) pour signal d'apprentissage rapide
- **Reward shaping dense** (potential-based) plutôt que sparse pour accélérer
- **Self-play + ELO** dès qu'on a un baseline qui survit
- **Hyper-parameter sweep** parallèle avec RLlib `tune`
- **Critère go/no-go** : si à J+15 (fin S3) win rate < 30% contre rule-based simple → on drop le RL pour la soutenance, on présente la rule-based + le pipeline RL comme bonus "in progress"
- **Logs continus W&B** : tout est tracké, on peut investiguer.
- **Baseline simple PPO single-agent** comme premier check (avant le multi-agent self-play)

**Owner** : P5
**Indicateur** : à J+10 (fin S2), training tourne et progresse (loss baisse, win rate vs rule-based augmente).

## R03 — poll/asio bug Fedora

**Détail** : asio sur Fedora peut différer subtilement (epoll vs kqueue est non-issue ici, mais glibc versions diffèrent).

**Mitigation** :
- CI matrix `ubuntu-22.04` + `fedora-39` dès le **jour 1**
- Tests d'intégration tournent sur les deux OS
- ASan / UBSan / TSan activés en CI debug
- Si bug obscur : escalation P2 + P6, possible refactor vers `epoll` direct (pas de poll → simpler)

**Owner** : P2
**Indicateur** : CI verte sur les 2 OS dès fin S1.

## R04 — Vulkan 1.3 + dynamic rendering + bindless : apprentissage

**Détail** : Ces features sont récentes, doc parfois incomplète, drivers parfois capricieux.

**Mitigation** :
- **Jour 1-3** : P3 et P4 font un sample "triangle Vulkan" → "cube texturé" → "scène 3 objets" en suivant les samples Sascha Willems
- **vk-bootstrap** pour init (gros gain de temps, élimine boilerplate)
- **Validation layers TOUJOURS ON** en debug
- Pair-programming P3+P4 systématique sur les passes complexes (frame graph, bindless)
- Reference repo : github.com/SaschaWillems/Vulkan + vkguide.dev
- Hot-reload shaders dès S2 pour itérer vite

**Owner** : P3 (GUI Lead)
**Indicateur** : fin S1 sample triangle + cube rotatif rend.

## R05 — pybind11 GIL / multi-thread RLlib

**Détail** : RLlib lance des workers parallèles, chacun appelle `Sim::step` Python qui fait du C++. Mauvaise gestion GIL → leaks ou crashes.

**Mitigation** :
- **`py::call_guard<py::gil_scoped_release>`** sur tous les bindings hot
- `Sim` est **thread-local** : 1 instance par worker, pas de partage
- Pas de `static`/global state dans `libzappy_sim`
- Test stress en S2 : 32 workers × 8 envs en parallèle pendant 1h → vérifier RSS stable, pas de segfault
- ASan/TSan dans CI debug pour catch races

**Owner** : P6
**Indicateur** : test stress 32 workers/1h pass en fin S2.

## R06 — Conformance sim vs runtime drift

**Détail** : si la lib core évolue dans le serveur runtime mais pas dans la sim (ou vice-versa), les comportements divergent silencieusement → modèle entraîné sur sim sous-performant en prod.

**Mitigation** :
- **Refactor server core ↔ runtime en S1** : la lib `zappy_core` est partagée
- **Conformance test bit-à-bit** : `tests/conformance_sim_vs_runtime/` lance les MÊMES scénarios sur les deux, exige snapshot JSON égal
- Ce test est dans **`ci.yml`** (bloquant chaque PR)
- Si KO sur une PR : on n'avance pas → root cause + fix
- Review obligatoire de P1 ET P6 sur toute modif de `server/core/`

**Owner** : P1 + P6 (co-owners)
**Indicateur** : conformance test PASS sur 100% des PR mergées.

## R07 — Conflit contrat protocole

**Détail** : un changement breaking du protocole (ex: ajout d'un nouveau message GUI) peut casser 3 composants.

**Mitigation** :
- **Contrats figés à fin S1** : `docs/01_architecture/06_protocols.md` est gelé, toute modif passe par ADR
- **ADR + vote 3 tech-leads** obligatoires
- **Versioning** : `.zrec` versionné, protocole versionné (header `WELCOME v1.0\n` futur si rupture)
- **Tests d'intégration AI↔server** rouge dès qu'il y a divergence
- Channel Discord `#protocol` dédié pour les discussions

**Owner** : Tech-leads collégial
**Indicateur** : 0 modif protocole post-S1 sans ADR Accepted.

## R08 — Machine training down

**Détail** : seule machine 64GB+RTX disponible → si elle plante, plus de training.

**Mitigation** :
- **Snapshots checkpoints** toutes les 30 min vers cloud (S3 ou GitHub Artifacts via `actions-runner` periodic upload)
- **Tmux + autosave** : tmux-resurrect ou screen log
- **Backup secondaire** : un dev a RTX local capable de runs courts (P3 ou P5)
- **Monitoring** : Discord webhook si la machine ne ping plus 5 min
- **Diagnostic** : P6 a accès SSH + IPMI / sortie graphique distante si root cause hardware

**Owner** : P6
**Indicateur** : <1h downtime cumulé sur le mois.

## R09 — vcpkg / pip deps cassent en CI Fedora

**Détail** : vcpkg parfois pas testé sur Fedora, certaines deps Python ont des wheels Linux-only.

**Mitigation** :
- **Versions épinglées** dans `vcpkg.json` et `pyproject.toml`
- **vcpkg cache** dans CI pour stabilité
- **Dockerfile.ci** dédié, image taguée par hash deps → reproductible
- **Dependabot weekly** pour upgrades contrôlés (PR séparées)
- Si bump deps casse CI : revert immédiat + investigate

**Owner** : P6
**Indicateur** : 0 downtime CI > 4h dû aux deps.

## R10 — Crash live demo soutenance

**Détail** : démos GPU + réseau + IA = beaucoup de risque jour-J. Driver Vulkan, OS update, port occupé, IA qui se bloque, etc.

**Mitigation** :
- **Répétition complète J-2 (mercredi 17 juin)** sur la machine de démo, conditions réelles
- **Build release immutable** packagée sur tar.gz, copiée sur la machine de démo, **pas de rebuild dernière minute**
- **Backup plan** : vidéo démo pré-enregistrée 5 min en plus de la démo live (si crash → "on va vous montrer la vidéo et expliquer ce qui s'est passé")
- **Replay `.zrec`** prêt : si serveur crashe, on charge un replay enregistré (montre le rendu sans dépendance live au serveur)
- **2 machines** présentes : machine principale + backup (laptops + USB-C HDMI)
- **Network local only** : pas d'internet nécessaire pour la démo
- **Réveil machine 1h avant** : tout est warm, drivers chargés, swap purgé
- **Process kill explicite** au début du script de démo
- **Slides en backup** sur clé USB + cloud

**Owner** : Tous (responsabilité collective)
**Indicateur** : répétition J-2 PASS sans intervention.

## Sur-suivi des risques

Vérification hebdomadaire à la retro vendredi :
1. Chaque risque : score a-t-il changé ?
2. Nouveau risque émergent ?
3. Mitigation efficace ?
4. Risque mitigé → on retire

Document mis à jour dans le repo (commit signé par retro animator du vendredi).
