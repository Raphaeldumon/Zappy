# Polissage de toutes les sorties destinées à l'utilisateur (français)

**Date :** 2026-07-09
**Statut :** approuvé (en attente de revue finale de la spec)

## Objectif

Chaque information montrée à l'utilisateur du projet Zappy est actuellement mal
présentée : mise en page brouillonne, contenu peu clair ou incomplet, et style
incohérent (mélange français/anglais). On uniformise les trois surfaces
utilisateur pour qu'elles parlent d'une seule voix : **français partout**,
colonnes alignées, messages spécifiques.

### Décisions verrouillées
- **Langue :** français partout (CLI serveur, IA, HUD graphique).
- **HUD :** panneau stylisé (fond semi-transparent, grille label/valeur alignée),
  pas des lignes flottantes.
- **Log de démarrage serveur :** oui, ajouter une ligne de confirmation.
- **Profondeur GUI :** toutes les surcouches (overlays), pas seulement le HUD.

### Non-objectifs (YAGNI)
- Aucune modification de logique de jeu, réseau ou protocole.
- Aucune icône/texture nouvelle pour le HUD (pas de refonte visuelle complète).
- On **ne touche pas** au travail en cours non commité dans
  `server/runtime/server.cpp` (logique de paris / démarrage headless) ni aux
  fichiers déjà modifiés au-delà de ce qui est décrit ici.

## Surface 1 — CLI serveur (`server/runtime/`)

### `parse_args.cpp` — `usage()`
Réécrire en français, restructuré :
- Ligne synopsis : `USAGE: <prog> -p port -x width -y height -n nom1 nom2 ... -c clientsNb -f freq`
  (on garde le token `USAGE:` et les lettres de flags — spec Epitech).
- Bloc **« Options requises »** puis bloc **« Options »**, colonnes alignées
  `-flag  valeur   description` en français.
- Une ligne **« Exemple »** avec une invocation concrète.

### `parse_args.cpp` — messages d'erreur
Traduire les ~12 chaînes `throw std::invalid_argument(...)` en français
spécifique. Correspondances (sens conservé) :
- valeur entière attendue → `option <flag> attend un entier, reçu '<valeur>'`
- valeur manquante → `option <flag> attend une valeur`
- nom d'équipe vide → `un nom d'équipe ne peut pas être vide`
- `GRAPHIC` réservé → `le nom d'équipe 'GRAPHIC' est réservé à l'interface graphique`
- doublon → `nom d'équipe en double : '<nom>'`
- trop d'équipes → `trop d'équipes : <MAX_TEAMS> au maximum`
- `-n` sans nom → `l'option -n attend au moins un nom d'équipe`
- option inconnue → `option inconnue : <flag>`
- options requises manquantes → `options requises manquantes (il faut -p -x -y -n -c -f)`
- valeurs positives → `-p -x -y -c -f doivent tous être strictement positifs`
- trop de clients → `trop de clients par équipe : -c ne doit pas dépasser <MAX_CLIENTS_PER_TEAM>`

### `main.cpp`
- Préfixe d'erreur `zappy_server:` → `zappy_server :` (espace avant `:` selon
  la typographie française ; acceptable en sortie technique).
- Message fatal `fatal:` → `erreur fatale :`.

### `server.cpp` — log de démarrage (nouveau)
Au tout début de `Server::run()` (ligne ~39, loin des hunks non commités
238–460), émettre **une** ligne sur stdout :

```
Serveur Zappy · port <p> · carte <x>×<y> · équipes: <a>, <b> · <c> clients/équipe · fréquence <f>
```

Données disponibles via `args_`. Utiliser `std::cout` + `std::flush`.

## Surface 2 — Client IA (`ai/baseline/cli.py`)

Éditer **la source** `ai/baseline/cli.py` ; `make` régénère le bundle
`zappy_ai` via `_bundle.py` (ne pas éditer `zappy_ai` généré).

- Aujourd'hui une **seule** chaîne `USAGE` est imprimée pour toutes les erreurs.
  La remplacer par des messages **spécifiques** en français :
  - argument requis manquant → nommer lequel (`-p` et/ou `-n`).
  - port hors plage → `port invalide : doit être entre 1 et 65535`.
  - fréquence invalide → `fréquence invalide : doit être strictement positive`.
  - arguments inconnus → les nommer.
- Ajouter `-v/--verbose` à la chaîne d'usage (actuellement non documenté).
- Chaîne d'usage française :
  `UTILISATION : <prog> -p port -n nom -h machine [-f freq] [-v]` suivie d'une
  description par option.
- Runtime : `[AI] Interrupted` → `[IA] Interrompu` ; `[AI] Error: <e>` →
  `[IA] Erreur : <e>`.

Codes de sortie inchangés (0 pour `--help`, 84 pour erreur, 130 pour Ctrl-C).

## Surface 3 — HUD + overlays GUI (`gui/interface.cpp`, `gui/raylibWrapper.cpp`)

### Prérequis — police (`raylibWrapper.cpp`, `loadUiFont`)
`LoadFontEx(path, baseSize, nullptr, 0)` ne rastérise que l'ASCII 32–126 ; les
accents s'afficheraient en « tofu ». Construire un tableau de codepoints couvrant :
- ASCII 32–126 (95 points),
- Latin-1 Supplément utile au français : plage 0xC0–0xFF (À…ÿ), plus 0xAB « /
  0xBB » et 0xB7 · si utilisés,
- `Œ` (0x152) et `œ` (0x153).

Le passer à `LoadFontEx(path, baseSize, codepoints.data(), count)`. Si
`toxigenesis bd.otf` ne contient pas ces glyphes, le rendu reste dégradé mais ne
plante pas ; **vérification** via le smoke test GUI (voir mémoire `gui-smoke-test`).
Si les accents ne rendent pas, repli : conserver le texte français **sans**
accents pour les chaînes concernées (décision à la vérification).

### Helper de panneau partagé (nouveau, `interface.cpp`)
Petite fonction utilitaire `drawPanel(x, y, w, h[, titre])` centralisant : fond
`Color{0,0,0,alpha}`, bordure `Color{255,255,255,~90}`, padding, couleur de
titre. Réutilisée par le HUD et les overlays pour un rendu uniforme.

### `drawHud()`
Remplacer les lignes flottantes par **un** panneau stylisé haut-gauche, grille
label/valeur alignée :
- Ligne 1 : `CARTE <l>×<h>   ÉQUIPES <n>`
- Ligne 2 : `JOUEURS <n>   ŒUFS <n>`
- Ligne 3 : `VITESSE <freq>   TEMPS <mm:ss>`  (indice `[+/-]` pour la vitesse)
- Ligne 4 : `<Saison> · <Météo> · <hh>h<mm>   FX <on/off>` (marqueur `*` si forcé)
- Si suivi : `Suivi du joueur #<id>  (F pour libérer)`.
- Déconnecté : `DÉCONNECTÉ` (au lieu de `DISCONNECTED`).
- Le réticule (crosshair) central est conservé.
- Labels saison/météo (`seasonLabel`/`weatherLabel`) : vérifier qu'ils sont en
  français ; traduire sinon.

### Overlays traduits + uniformisés (via le helper)
- **Écran de fin** (`drawEndScreen`) : `VICTORY` → `VICTOIRE` ; `<x> wins` →
  `<x> remporte la partie` ; `ENTER to dismiss` → `ENTRÉE pour fermer` ;
  en-têtes stats `Team/Alive/Max/Avg/Lvl 8` → `Équipe/Vivants/Max/Moy/Niv 8` ;
  `GLOBAL CLAN STATS` → `STATISTIQUES DES CLANS` ; `ALIVE PLAYERS` →
  `JOUEURS VIVANTS` ; `ID/Team` → `ID/Équipe` ; ligne
  `Alive players/Eggs left/Time unit` → `Joueurs vivants / Œufs restants / Unité de temps`.
- **Overlay de paris** (`drawBettingOverlay`) : déjà en français
  (`CHOISIS TON PARI`) — homogénéiser le sous-titre / statut / indices et le
  passer par le helper de panneau.
- **Overlay d'aide** (`drawHelpOverlay`) : traduire les libellés de touches en
  français.
- **Fil d'événements** (`drawEventFeed`) : vérifier la langue des messages,
  traduire les libellés fixes.
- **Tooltips / info-tuile** (`drawTileInfoPanel`, `drawHoverTooltip`) :
  `(empty)` → `(vide)` ; `egg/eggs` → `œuf/œufs` ; en-têtes ressources en
  français si présents.

## Tests & vérification

- **Serveur :** `server/tests/test_parse_args.cpp` référence le comportement
  (pas le texte exact d'usage sauf `--help` → exit 0). Adapter les assertions
  qui compareraient des sous-chaînes de message d'erreur si nécessaire ;
  reconstruire et lancer `test_parse_args`.
- **IA :** vérifier `--help` (exit 0), arguments manquants (exit 84, message
  nommant l'argument), port hors plage (exit 84). Régénérer le bundle via `make`.
- **GUI :** compiler les shaders (`glslangValidator`) et lancer le smoke test
  depuis `gui/` (assets relatifs au cwd) ; confirmer visuellement que les
  accents rendent et que le panneau HUD s'affiche correctement. Étendre
  `gui/tests/test_environment.cpp` seulement si une logique testable est ajoutée
  (le rendu ne l'est pas directement).

## Ordre d'implémentation suggéré
1. Serveur (usage + erreurs + log démarrage) — le plus isolé.
2. IA (cli.py + régénération bundle).
3. GUI : police d'abord (prérequis), puis helper de panneau, puis `drawHud`,
   puis les overlays un par un.

Chaque surface est indépendante et peut être vérifiée séparément.
