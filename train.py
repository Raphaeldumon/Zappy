#!/usr/bin/env python3
"""
train.py — Harness de self-play / batch pour zappy_ai.

ATTENTION : ce script n'entraîne PAS un modèle de machine learning.
Le sujet Zappy ne demande aucun apprentissage, et zappy_ai.py est une IA
à base de FSM déterministe (pas de policy, pas de reward, pas de gradient).
Ici "train" est pris au sens "rodage / mise à l'épreuve en masse" : on fait
jouer l'IA sur beaucoup de parties pour produire des données mesurables,
exploitées ensuite par eval.py.

Déroulé d'une partie :
  1. on réserve un port TCP libre ;
  2. on lance zappy_server avec la carte / les équipes / -c / -f demandés ;
  3. on attend que le serveur écoute réellement (poll de connexion) ;
  4. on branche un observateur GUI (équipe réservée GRAPHIC) qui lit le
     protocole graphique pour suivre niveaux, morts et fin de partie ;
  5. on lance les clients zappy_ai initiaux (l'IA se réplique ensuite via Fork) ;
  6. on attend "seg" (fin de partie) ou le timeout ;
  7. on arrête proprement tous les process et on enregistre le résultat.

Les résultats sont sérialisés en JSON pour eval.py.

REMARQUE SUR LE PROTOCOLE GUI :
L'annexe protocole n'était pas fournie dans les pages du sujet que j'ai eues.
Les tokens parsés ci-dessous (msz/tna/pnw/plv/pdi/seg) sont ceux du protocole
graphique Zappy standard et stable d'Epitech. Le parseur est tolérant : tout
token inconnu est ignoré sans planter. Si ton serveur diffère, n'adapte que
GuiObserver._handle_gui_line.
"""

import argparse
import contextlib
import json
import os
import select
import signal
import socket
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional


MAX_LEVEL = 8           # niveau maximal (élévation 7->8) défini par le sujet
WIN_PLAYERS = 6         # au moins 6 joueurs au niveau max pour gagner (sujet)
GUI_TEAM_NAME = "GRAPHIC"


# --------------------------------------------------------------------------- #
# Configuration & résultats
# --------------------------------------------------------------------------- #

@dataclass
class BatchConfig:
    """Paramètres d'un lot de parties."""
    server_bin: str                       # chemin du binaire zappy_server
    ai_script: str                        # chemin de zappy_ai.py
    teams: List[str]                      # noms d'équipes (hors GRAPHIC)
    width: int = 10
    height: int = 10
    clients: int = 6                      # -c : slots initiaux par équipe
    frequency: int = 100                  # -f : réciproque de l'unité de temps
    games: int = 5                        # nombre de parties dans le lot
    max_duration: float = 120.0           # timeout par partie (secondes mur)
    host: str = "localhost"
    python_bin: str = sys.executable
    use_gui: bool = True                  # brancher l'observateur GUI ?


@dataclass
class TeamStats:
    """Statistiques d'une équipe sur UNE partie."""
    max_level: int = 1
    players_seen: int = 0
    deaths: int = 0
    # combien de joueurs DISTINCTS ont atteint chaque niveau (1..8)
    level_counts: Dict[int, int] = field(
        default_factory=lambda: {lvl: 0 for lvl in range(1, MAX_LEVEL + 1)}
    )


@dataclass
class GameResult:
    """Résultat d'UNE partie."""
    index: int
    port: int
    duration_s: float
    winner: Optional[str]                 # équipe gagnante (via seg) ou None
    reason: str                           # "seg" | "timeout" | "server_down"
    server_returncode: Optional[int]
    teams: Dict[str, dict] = field(default_factory=dict)  # team -> TeamStats


# --------------------------------------------------------------------------- #
# Outils réseau / process
# --------------------------------------------------------------------------- #

def find_free_port() -> int:
    """Réserve un port libre en laissant l'OS en choisir un (bind sur 0),
    puis le relâche immédiatement pour que le serveur puisse le reprendre."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("", 0))
        return probe.getsockname()[1]


def wait_server_ready(host: str, port: int, timeout: float = 10.0) -> bool:
    """Tente de se connecter au serveur jusqu'à ce qu'il accepte (ou timeout).
    Évite la course "on lance les IA avant que le serveur n'écoute"."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def terminate(proc: Optional[subprocess.Popen], grace: float = 2.0) -> Optional[int]:
    """Arrêt propre d'un process : SIGTERM, puis SIGKILL si récalcitrant."""
    if proc is None or proc.poll() is not None:
        return proc.poll() if proc else None
    proc.terminate()
    try:
        proc.wait(timeout=grace)
    except subprocess.TimeoutExpired:
        proc.kill()
        with contextlib.suppress(Exception):
            proc.wait(timeout=grace)
    return proc.poll()


# --------------------------------------------------------------------------- #
# Observateur GUI (protocole graphique)
# --------------------------------------------------------------------------- #

class GuiObserver:
    """Se connecte comme un GUI (envoie GRAPHIC au handshake) et lit l'état
    du monde via le protocole graphique. On ne pilote rien : on observe pour
    en déduire niveaux, morts et gagnant — c'est la source de vérité côté
    serveur, indépendante des logs de nos IA."""

    def __init__(self, host: str, port: int, known_teams: List[str]):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.buffer = ""

        # team -> TeamStats ; on pré-remplit avec les équipes connues pour
        # toujours avoir une entrée même si "tna" ne nous parvient pas.
        self.stats: Dict[str, TeamStats] = {t: TeamStats() for t in known_teams}

        self.player_team: Dict[str, str] = {}   # "#id" -> team
        self.player_level: Dict[str, int] = {}   # "#id" -> niveau courant
        self.winner: Optional[str] = None        # rempli par "seg"

    def connect(self) -> bool:
        try:
            self.sock = socket.create_connection((self.host, self.port), timeout=2.0)
        except OSError:
            return False
        self.sock.setblocking(False)

        # Handshake GUI : <-- WELCOME ; --> GRAPHIC
        if self._read_blocking_line(timeout=3.0) != "WELCOME":
            self.close()
            return False
        self._send_line(GUI_TEAM_NAME)
        return True

    def close(self) -> None:
        if self.sock:
            with contextlib.suppress(Exception):
                self.sock.close()
            self.sock = None

    # --- I/O bas niveau (même logique de buffer que zappy_ai.NetworkClient) ---

    def _send_line(self, line: str) -> None:
        if self.sock:
            self.sock.sendall((line + "\n").encode("utf-8"))

    def _read_blocking_line(self, timeout: float) -> Optional[str]:
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in self._poll_lines():
                return line
            time.sleep(0.01)
        return None

    def _poll_lines(self) -> List[str]:
        """Lit tout ce qui est disponible sans bloquer et renvoie des lignes
        complètes (séparées par \\n)."""
        if not self.sock:
            return []
        out: List[str] = []
        readable, _, _ = select.select([self.sock], [], [], 0)
        if not readable:
            return out
        try:
            data = self.sock.recv(8192)
        except (BlockingIOError, ConnectionResetError):
            return out
        if not data:                       # serveur fermé
            return out
        self.buffer += data.decode("utf-8", errors="ignore")
        while "\n" in self.buffer:
            line, self.buffer = self.buffer.split("\n", 1)
            out.append(line.strip())
        return out

    # --- Traitement protocole ---

    def pump(self) -> None:
        """À appeler régulièrement : consomme les lignes en attente."""
        for line in self._poll_lines():
            if line:
                self._handle_gui_line(line)

    def _team_of(self, name: str) -> TeamStats:
        return self.stats.setdefault(name, TeamStats())

    def _handle_gui_line(self, line: str) -> None:
        parts = line.split()
        if not parts:
            return
        tok = parts[0]

        # tna N : déclaration d'une équipe
        if tok == "tna" and len(parts) >= 2:
            self._team_of(parts[1])

        # pnw #id X Y O L N : nouveau joueur (id, x, y, orient, niveau, équipe)
        elif tok == "pnw" and len(parts) >= 7:
            pid, level, team = parts[1], int(parts[5]), parts[6]
            self.player_team[pid] = team
            self.player_level[pid] = level
            st = self._team_of(team)
            st.players_seen += 1
            self._record_level(team, pid, level)

        # plv #id L : niveau (courant) d'un joueur
        elif tok == "plv" and len(parts) >= 3:
            pid, level = parts[1], int(parts[2])
            self.player_level[pid] = level
            team = self.player_team.get(pid)
            if team:
                self._record_level(team, pid, level)

        # pdi #id : mort d'un joueur
        elif tok == "pdi" and len(parts) >= 2:
            pid = parts[1]
            team = self.player_team.get(pid)
            if team:
                self._team_of(team).deaths += 1

        # seg N : fin de partie, N = équipe gagnante
        elif tok == "seg" and len(parts) >= 2:
            self.winner = parts[1]

    def _record_level(self, team: str, pid: str, level: int) -> None:
        st = self._team_of(team)
        st.max_level = max(st.max_level, level)
        # On compte un joueur par niveau atteint (les niveaux ne font que
        # monter en Zappy, donc un "plv L" ~ un joueur de plus arrivé à L).
        # Sert directement à la condition de victoire : 6 joueurs au niveau max.
        if 1 <= level <= MAX_LEVEL:
            st.level_counts[level] += 1


# --------------------------------------------------------------------------- #
# Déroulé d'une partie
# --------------------------------------------------------------------------- #

def build_server_cmd(cfg: BatchConfig, port: int) -> List[str]:
    """Construit la ligne de commande du serveur conforme au sujet :
    -p port -x width -y height -n name1 name2 ... -c clientsNb -f freq."""
    cmd = [
        cfg.server_bin,
        "-p", str(port),
        "-x", str(cfg.width),
        "-y", str(cfg.height),
        "-n", *cfg.teams,
        "-c", str(cfg.clients),
        "-f", str(cfg.frequency),
    ]
    return cmd


def build_ai_cmd(cfg: BatchConfig, port: int, team: str) -> List[str]:
    """Ligne de commande d'un client IA conforme au sujet :
    -p port -n name -h machine. On passe -f pour éviter la phase de mesure
    de fréquence (déterminisme + démarrage plus rapide)."""
    return [
        cfg.python_bin,
        cfg.ai_script,
        "-p", str(port),
        "-n", team,
        "-h", cfg.host,
        "-f", str(cfg.frequency),
    ]


def run_one_game(cfg: BatchConfig, index: int) -> GameResult:
    port = find_free_port()
    server_proc: Optional[subprocess.Popen] = None
    ai_procs: List[subprocess.Popen] = []
    gui = GuiObserver(cfg.host, port, cfg.teams)
    started = time.time()
    reason = "timeout"

    try:
        # 1) Serveur
        server_proc = subprocess.Popen(
            build_server_cmd(cfg, port),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if not wait_server_ready(cfg.host, port):
            reason = "server_down"
            raise RuntimeError("le serveur n'a jamais écouté sur le port")

        # 2) Observateur GUI (optionnel)
        gui_ok = gui.connect() if cfg.use_gui else False

        # 3) Clients IA initiaux (l'IA se réplique ensuite via Fork)
        for team in cfg.teams:
            for _ in range(cfg.clients):
                ai_procs.append(
                    subprocess.Popen(
                        build_ai_cmd(cfg, port, team),
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                )
                time.sleep(0.02)  # léger étalement des connexions

        # 4) Boucle de partie : jusqu'à seg, mort du serveur, ou timeout
        deadline = started + cfg.max_duration
        while time.time() < deadline:
            if gui_ok:
                gui.pump()
                if gui.winner is not None:
                    reason = "seg"
                    break
            if server_proc.poll() is not None:
                reason = "server_down"
                break
            time.sleep(0.05)

    except RuntimeError:
        pass
    finally:
        for proc in ai_procs:
            terminate(proc)
        srv_rc = terminate(server_proc)
        gui.close()

    duration = time.time() - started
    return GameResult(
        index=index,
        port=port,
        duration_s=round(duration, 2),
        winner=gui.winner,
        reason=reason,
        server_returncode=srv_rc if server_proc else None,
        teams={name: asdict(st) for name, st in gui.stats.items()},
    )


def run_batch(cfg: BatchConfig) -> List[dict]:
    """Joue cfg.games parties et renvoie la liste des résultats (dicts)."""
    results: List[dict] = []
    for i in range(cfg.games):
        print(f"[train] partie {i + 1}/{cfg.games} ...", file=sys.stderr)
        result = run_one_game(cfg, i)
        print(
            f"[train]   -> winner={result.winner} "
            f"reason={result.reason} duree={result.duration_s}s",
            file=sys.stderr,
        )
        results.append(asdict(result))
    return results


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Harness de self-play pour zappy_ai (pas de ML)."
    )
    p.add_argument("--server", default="./zappy_server",
                   help="chemin du binaire zappy_server")
    p.add_argument("--ai", default="./zappy_ai.py",
                   help="chemin du script zappy_ai.py")
    p.add_argument("--teams", nargs="+", default=["team1"],
                   help="noms des équipes (hors GRAPHIC)")
    p.add_argument("-x", "--width", type=int, default=10)
    p.add_argument("-y", "--height", type=int, default=10)
    p.add_argument("-c", "--clients", type=int, default=6)
    p.add_argument("-f", "--frequency", type=int, default=100)
    p.add_argument("--games", type=int, default=5)
    p.add_argument("--max-duration", type=float, default=120.0)
    p.add_argument("--host", default="localhost")
    p.add_argument("--no-gui", action="store_true",
                   help="ne pas brancher l'observateur GUI")
    p.add_argument("--out", default="results.json",
                   help="fichier JSON de sortie")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    if not os.path.exists(args.server):
        print(f"[train] serveur introuvable: {args.server}", file=sys.stderr)
        return 84
    if not os.path.exists(args.ai):
        print(f"[train] zappy_ai introuvable: {args.ai}", file=sys.stderr)
        return 84

    cfg = BatchConfig(
        server_bin=args.server,
        ai_script=args.ai,
        teams=args.teams,
        width=args.width,
        height=args.height,
        clients=args.clients,
        frequency=args.frequency,
        games=args.games,
        max_duration=args.max_duration,
        host=args.host,
        use_gui=not args.no_gui,
    )

    results = run_batch(cfg)

    payload = {"config": asdict(cfg), "results": results}
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2)
    print(f"[train] {len(results)} parties écrites dans {args.out}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    # On veut tuer proprement les enfants si on reçoit Ctrl-C.
    with contextlib.suppress(KeyboardInterrupt):
        raise SystemExit(main())
    print("[train] interrompu", file=sys.stderr)
    raise SystemExit(130)
