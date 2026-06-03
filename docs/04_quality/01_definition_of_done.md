# 01 — Definition of Done (DoD)

Une tâche/PR est **Done** quand elle satisfait **toutes** les conditions de sa catégorie.

## DoD générique (s'applique à TOUTE PR)

- [ ] Conventional Commits respectés sur tous les commits
- [ ] CI **verte** (build + lint + tests + commitlint + docs build)
- [ ] Pas de TODO/FIXME sans issue GitHub référencée
- [ ] Pas de code commenté laissé
- [ ] PR template rempli (description, type, lien issue, checklist, comment tester)
- [ ] 2 reviews approuvées dont 1 du CODEOWNERS du domaine
- [ ] Toutes les conversations review marquées resolved
- [ ] Branche à jour avec `develop` (pas de "behind by N commits")

## DoD `feat` — nouvelle feature

En plus du DoD générique :
- [ ] Tests **unitaires** ajoutés (Catch2 ou pytest selon stack)
- [ ] Si feature visible utilisateur → test d'**intégration** ou smoke test ajouté
- [ ] Si modification API publique → doxygen / docstrings à jour
- [ ] Si feature config/CLI → documentation utilisateur mise à jour (`docs/09_appendix/01_useful_commands.md`)
- [ ] Si feature impacte les perfs → benchmark exécuté avant/après, résultat dans la PR
- [ ] Coverage du code modifié >= 70% (C++) / >= 80% (Python)
- [ ] Pas de warning compilateur (treated as error)
- [ ] Si feature impacte le protocole / format `.zrec` → **ADR créée + lien dans la PR**

## DoD `fix` — bug fix

En plus du DoD générique :
- [ ] Test de **régression** ajouté qui reproduit le bug avant le fix et passe après
- [ ] Root cause documentée dans le body de la PR (pas juste "fixed crash")
- [ ] Si bug critique → entry dans `CHANGELOG.md`
- [ ] Si bug avait été détecté en prod via metric / log → metric/log enrichi pour faciliter la prochaine détection

## DoD `perf` — optimisation

En plus du DoD générique :
- [ ] Benchmark avant/après dans la PR (chiffres, conditions de bench, hw utilisé)
- [ ] Pas de régression fonctionnelle (tests existants verts)
- [ ] Si gain < 10% → justifier pourquoi mergé quand même (souvent simplicité de code)
- [ ] Si change l'algorithmique (complexité) → expliquer dans le body

## DoD `refactor`

En plus du DoD générique :
- [ ] Aucun changement de comportement (tests existants verts sans modif)
- [ ] Si découpe de fichiers → CODEOWNERS mis à jour si nécessaire
- [ ] Coverage maintenue (pas en baisse)

## DoD `docs`

En plus du DoD générique :
- [ ] MkDocs build local OK (`mkdocs serve` sans erreur)
- [ ] Pas de lien cassé (markdown-link-check job en CI)
- [ ] Mermaid diagrams rendent correctement (preview avec mkdocs-mermaid2-plugin)
- [ ] En français (sauf code blocks et identifiants)

## DoD `test`

En plus du DoD générique :
- [ ] Le test failed sans la modification de code associée (sinon : test inutile)
- [ ] Pas de flake (run 10x localement, 100% pass)
- [ ] Pas de sleep arbitraire (utiliser polling + timeout)
- [ ] Si test long > 30s → mettre dans `[!benchmark]` ou `[long]` tag, pas dans suite unit

## DoD `build` / `ci`

En plus du DoD générique :
- [ ] Build local OK sur Ubuntu **et** Fedora (vérifié via Docker)
- [ ] CI nightly verte aussi (pas que `ci.yml`)
- [ ] Si add dependency → bump dans `vcpkg.json` ou `pyproject.toml` avec version épinglée
- [ ] Si bump deps majeur → ADR + entry dans `docs/09_appendix/02_dependencies.md`

## DoD `chore`

En plus du DoD générique :
- [ ] Body de la PR explique pourquoi c'est nécessaire (souvent les chores ont l'air gratuits)

## DoD spécifique : **modification protocole**

- [ ] **ADR Accepted** avant le merge
- [ ] Documentation `docs/01_architecture/06_protocols.md` mise à jour
- [ ] Server **et** GUI **et** AI mis à jour dans le même PR (ou PRs liées merged ensemble)
- [ ] Test d'intégration AI↔server YAML ajusté
- [ ] Compatibilité de version `.zrec` : si breaking, bump VERSION u32 dans le header

## DoD spécifique : **nouveau shader Vulkan**

- [ ] glslangValidator OK (en pre-commit)
- [ ] Compile sans warning
- [ ] Test smoke screenshot OK (ou skip explicite documenté)
- [ ] Coût GPU mesuré (RenderDoc capture ou GPU timestamps)
- [ ] Fallback documenté si feature optionnelle (ex: RT)

## DoD spécifique : **nouveau checkpoint RL**

- [ ] Eval vs `zappy_ref` : >=70% win rate sur 100 matches (sinon : pas mergé en `models/current/`)
- [ ] ELO calculé et publié dans le dashboard
- [ ] Config training inclus dans `models/runs/<id>/config.yaml`
- [ ] Reproductible : `make train CHECKPOINT=...` reproduit avec ±5% performance
- [ ] Hash SHA256 du `model.pt` enregistré dans `models/manifest.json`

## DoD spécifique : **release tag**

- [ ] `CHANGELOG.md` mis à jour (auto-généré + manual cleanup)
- [ ] Version bumped dans `CMakeLists.txt` + `pyproject.toml`
- [ ] Tag annoté : `git tag -a v0.x.y -m "..."`
- [ ] GitHub Release créé avec artifacts tar.gz
- [ ] Notes de release qui synthétisent les changements user-facing
- [ ] Démo manuelle effectuée par 2 personnes différentes

## DoD du projet à J+25

Voir [`docs/00_executive_summary.md#8-definition-de-100-du-projet-realise`](../00_executive_summary.md#8-definition-de-100-du-projet-realise) et [`docs/08_deliverables/01_final_deliverables.md`](../08_deliverables/01_final_deliverables.md).
