# Protocoles

## 1. Protocole AI ↔ Server

Conforme au sujet G-YEP-400 (PDF `G-YEP-400_zappy.pdf`).

### Handshake

```
S → C : WELCOME\n
C → S : TEAM_NAME\n
S → C : CLIENT_NUM\n        (nb de slots libres)
S → C : X Y\n               (dimensions du monde)
```

Cas spécial `TEAM_NAME == "GRAPHIC"` → le serveur switche sur le protocole GUI (voir section 2).

### Commandes AI (toutes terminées par `\n`)

| Commande         | Réponse | Coût | Description |
|------------------|---------|------|-------------|
| `Forward`        | `ok\n`                     | 7/f | Avance d'une case |
| `Right`          | `ok\n`                     | 7/f | Pivote à droite |
| `Left`           | `ok\n`                     | 7/f | Pivote à gauche |
| `Look`           | `[tile1, tile2, ...]\n`    | 7/f | Cône de vision |
| `Inventory`      | `[food n, sibur n, ...]\n` | 1/f | Inventaire courant |
| `Broadcast text` | `ok\n`                     | 7/f | Diffuse texte à tous |
| `Connect_nbr`    | `value\n`                  | -   | Slots libres team |
| `Fork`           | `ok\n`                     | 42/f | Pond un oeuf |
| `Eject`          | `ok\n` ou `ko\n`           | 7/f | Éjecte les autres de la case |
| `Take object`    | `ok\n` ou `ko\n`           | 7/f | Prend objet |
| `Set object`     | `ok\n` ou `ko\n`           | 7/f | Dépose objet |
| `Incantation`    | `Elevation underway...` | 300/f | Démarre rituel |

Réponses asynchrones du serveur (non-sollicitées) :
- `message K, text\n` : broadcast reçu, K = direction (0..8)
- `eject: K\n` : on a été éjecté, K = direction
- `dead\n` : on est mort, connexion fermée par le serveur

Contraintes :
- Max **10 commandes** en queue côté serveur, au-delà ignorées
- Server exécute en ordre FIFO par client
- Une commande bloque uniquement le drone concerné, pas le serveur

## 2. Protocole GUI ↔ Server

Conforme au PDF `G-YEP-400_zappy_GUI_protocol.pdf`.

### Symboles

| Symbole | Sens |
|---------|------|
| `X, Y` | position (largeur / hauteur) |
| `q0..q6` | quantités food / linemate / deraumere / sibur / mendiane / phiras / thystame |
| `n` | id joueur |
| `O` | orientation 1(N) 2(E) 3(S) 4(W) |
| `L` | level joueur ou level incantation |
| `e` | id egg |
| `T` | time unit |
| `N` | nom team |
| `R` | résultat incantation |
| `M` | message |
| `i` | id ressource |

### Messages

| Serveur → GUI | GUI → Serveur | Description |
|---------------|---------------|-------------|
| `msz X Y\n` | `msz\n` | dimensions de la carte |
| `bct X Y q0..q6\n` | `bct X Y\n` | contenu d'une case |
| `bct X Y q0..q6\n × nb_tiles` | `mct\n` | contenu de toute la carte |
| `tna N\n × nb_teams` | `tna\n` | noms des teams |
| `pnw n X Y O L N\n` | — | nouvelle connexion joueur |
| `ppo n X Y O\n` | `ppo n\n` | position joueur |
| `plv n L\n` | `plv n\n` | niveau joueur |
| `pin n X Y q0..q6\n` | `pin n\n` | inventaire joueur |
| `pex n\n` | — | expulsion |
| `pbc n M\n` | — | broadcast (message d'une IA) |
| `pic X Y L n n …\n` | — | début incantation |
| `pie X Y R\n` | — | fin incantation |
| `pfk n\n` | — | oeuf posé |
| `pdr n i\n` | — | ressource posée |
| `pgt n i\n` | — | ressource ramassée |
| `pdi n\n` | — | mort joueur |
| `enw e n X Y\n` | — | oeuf pondu |
| `ebo e\n` | — | connexion à un oeuf |
| `edi e\n` | — | mort d'un oeuf |
| `sgt T\n` | `sgt\n` | accès à la fréquence|
| `sst T\n` | `sst T\n` | modification de la fréquence |
| `seg N\n` | — | fin de la partie(team N gagne) |
| `smg M\n` | — | message serveur |
| `suc\n` | — | commande inconnue |
| `sbp\n` | — | mauvais paramètre d'une commande |

### Stratégie d'émission serveur

À la connexion d'un GUI :
1. `msz`, `sgt`
2. `tna` × n_teams
3. `bct` × W*H (full map state)
4. `pnw` × n_players
5. `enw` × n_eggs
6. (puis events au fil de l'eau)

Pendant le jeu :
- Sur chaque action joueur → émet l'event correspondant (pas re-broadcast complet de l'état)
- Sur respawn ressource → émet `bct` SEULEMENT pour les tiles modifiées
- Sur `sst T` du GUI → ack `sst T\n`

## 3. Index des ressources

| Index | Nom | Densité cible |
|-------|-----|---------------|
| 0 | `food` | 0.5 |
| 1 | `linemate` | 0.3 |
| 2 | `deraumere` | 0.15 |
| 3 | `sibur` | 0.1 |
| 4 | `mendiane` | 0.1 |
| 5 | `phiras` | 0.08 |
| 6 | `thystame` | 0.05 |

Utilisé dans : `q0..q6` (GUI protocol), `Take/Set object` (AI protocol), `pdr/pgt n i` (GUI events).

## 4. Gestion d'erreurs

### Côté AI

| Situation | Comportement serveur |
|-----------|----------------------|
| Commande inconnue | `ko\n` immédiat (sans coût) |
| Argument manquant ou invalide (`Take`, `Set`) | `ko\n` immédiat |
| Action impossible (Take sur tile vide, Set sans ressource) | `ko\n` après délai normal |
| Queue pleine (> 10 commandes) | commande silencieusement ignorée |
| Client déconnecté brutalement | joueur marqué mort, slot team libéré, GUIs notifiés `pdi n\n` |
| `Incantation` sans conditions remplies | `ko\n` immédiat (pas `Elevation underway`) |
| Participant mort pendant incantation | incantation continue sans lui ; si plus assez → `ko\n` à l'initiateur |

### Côté GUI

| Situation | Comportement serveur |
|-----------|----------------------|
| Tag inconnu | `suc\n` |
| Paramètre invalide ou manquant | `sbp\n` |
| `bct X Y` avec X ou Y hors map | `sbp\n` |
| `ppo/plv/pin n` avec n inexistant | `sbp\n` |
| `sst T` avec T ≤ 0 | `sbp\n` |
| GUI déconnecté brutalement | silencieusement retiré, aucune notification |

### Connexion / handshake

| Situation | Comportement serveur |
|-----------|----------------------|
| Team inconnue | connexion fermée immédiatement (pas de `WELCOME` ou après `WELCOME` selon implémentation) |
| Plus de slots disponibles | connexion fermée après handshake |
| Ligne trop longue (> 4096 octets sans `\n`) | connexion fermée |
| Données illisibles / binaire | connexion fermée |

## 5. Protocole admin (bonus)

Socket séparé, port `-p + 1000`. Auth via `--admin-token`.

Commandes texte :
```
auth <token>
pause
resume
set f <int>
kill <player_id>
spawn <res_id> <x> <y> <n>
snapshot <path>
reload-config <path>
quit
```

Réponse : `ok\n` ou `ko: reason\n`.

## 6. Broadcast codé team (bonus IA)

Format du `text` dans `Broadcast text` :

```
<magic_byte><b64_encoded_payload>
```

- `magic_byte` : XOR du `team_id` et d'un `team_secret_byte` partagé dans `team_codes.json`
- `payload` = struct binaire :
  ```
  uint8  type;            // 0x01 HELP, 0x02 HERE, 0x03 READY_LVL, 0x04 GATHER, 0x05 PROBE
  uint16 sender_id;
  uint16 game_tick;
  uint8  payload_len;
  uint8  payload[payload_len];
  ```
- Encodage : payload binary → base64 (sans padding, chars `[A-Za-z0-9+/]`)

Si le `magic_byte` ne match pas le team local → on ignore (paquet pour autre team).

## 7. Format `.zrec` (replay)

```
+---------------------------+
| MAGIC "ZREC" (4 bytes)    |
+---------------------------+
| VERSION u32 = 1           |
+---------------------------+
| HEADER_LEN u32            |
+---------------------------+
| HEADER_JSON (bytes)       |  ← config: map_size, teams, f, server_version, start_timestamp_utc
+---------------------------+
| FRAMES...                 |
+---------------------------+
| FRAME : {                 |
|   u64  timestamp_ms       |
|   u32  payload_len        |
|   char payload[payload_len]  ← raw GUI protocol line, sans `\n` final
| }                         |
+---------------------------+
| FOOTER MAGIC "EOZR" (4)   |
+---------------------------+
```

Outil CLI `tools/zrec_inspect`:
```
zrec_inspect file.zrec --header        # affiche header JSON
zrec_inspect file.zrec --dump          # dump toutes les frames texte
zrec_inspect file.zrec --stats         # nb frames, durée, taille moyenne
```

## 8. Configuration JSON serveur (bonus hot-reload)

`config/server.json` :
```json
{
  "densities": {
    "food": 0.5,
    "linemate": 0.3,
    "deraumere": 0.15,
    "sibur": 0.1,
    "mendiane": 0.1,
    "phiras": 0.08,
    "thystame": 0.05
  },
  "respawn_interval": 20,
  "fork_cost": 42,
  "incantation_duration": 300,
  "lifespan_per_food": 126,
  "admin_token": "change-me-in-prod"
}
```

Reload via `SIGUSR2` ou commande admin `reload-config <path>`. Changements appliqués au prochain tick.
