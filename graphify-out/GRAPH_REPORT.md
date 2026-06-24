# Graph Report - .  (2026-06-23)

## Corpus Check
- Large corpus: 81 files · ~612,804 words. Semantic extraction will be expensive (many Claude tokens). Consider running on a subfolder.

## Summary
- 1168 nodes · 1822 edges · 82 communities (68 shown, 14 thin omitted)
- Extraction: 98% EXTRACTED · 2% INFERRED · 0% AMBIGUOUS · INFERRED: 44 edges (avg confidence: 0.79)
- Token cost: 75,000 input · 8,366 output

## Community Hubs (Navigation)
- [[_COMMUNITY_3D GUI Renderer (raylib)|3D GUI Renderer (raylib)]]
- [[_COMMUNITY_AI Brain Decision Logic|AI Brain Decision Logic]]
- [[_COMMUNITY_Server Command Dispatch|Server Command Dispatch]]
- [[_COMMUNITY_World State Game Logic|World State Game Logic]]
- [[_COMMUNITY_World State Operations|World State Operations]]
- [[_COMMUNITY_AI Baseline Client (Python)|AI Baseline Client (Python)]]
- [[_COMMUNITY_RL Training Orchestration|RL Training Orchestration]]
- [[_COMMUNITY_AI Brain Strategy|AI Brain Strategy]]
- [[_COMMUNITY_GUI Interface Drawing|GUI Interface Drawing]]
- [[_COMMUNITY_Core Data Types|Core Data Types]]
- [[_COMMUNITY_GUI Protocol Wire Commands|GUI Protocol Wire Commands]]
- [[_COMMUNITY_GUI State Model|GUI State Model]]
- [[_COMMUNITY_Server Network Layer|Server Network Layer]]
- [[_COMMUNITY_GUI Event Emitter|GUI Event Emitter]]
- [[_COMMUNITY_GUI Request Parsing|GUI Request Parsing]]
- [[_COMMUNITY_Game Rules & Elevation|Game Rules & Elevation]]
- [[_COMMUNITY_AI Command Handlers|AI Command Handlers]]
- [[_COMMUNITY_AI Protocol Formatting|AI Protocol Formatting]]
- [[_COMMUNITY_Server Lifecycle & Scheduling|Server Lifecycle & Scheduling]]
- [[_COMMUNITY_Event Scheduler|Event Scheduler]]
- [[_COMMUNITY_CLI Argument Parsing|CLI Argument Parsing]]
- [[_COMMUNITY_GUI Player Entity|GUI Player Entity]]
- [[_COMMUNITY_Client IO Buffering|Client I/O Buffering]]
- [[_COMMUNITY_Legacy AI Baseline|Legacy AI Baseline]]
- [[_COMMUNITY_AI Frequency & Network Sync|AI Frequency & Network Sync]]
- [[_COMMUNITY_AI Response Parser|AI Response Parser]]
- [[_COMMUNITY_GUI Components Overview|GUI Components Overview]]
- [[_COMMUNITY_Training Evaluation Metrics|Training Evaluation Metrics]]
- [[_COMMUNITY_Network Layer Implementation|Network Layer Implementation]]
- [[_COMMUNITY_PlayerResource Events|Player/Resource Events]]
- [[_COMMUNITY_GUI Game Map|GUI Game Map]]
- [[_COMMUNITY_GUI Network Client|GUI Network Client]]
- [[_COMMUNITY_GUI Game Map Operations|GUI Game Map Operations]]
- [[_COMMUNITY_GUI Net Client Impl|GUI Net Client Impl]]
- [[_COMMUNITY_Network Tests|Network Tests]]
- [[_COMMUNITY_Egg Events|Egg Events]]
- [[_COMMUNITY_Tile & Inventory Events|Tile & Inventory Events]]
- [[_COMMUNITY_Server Arguments|Server Arguments]]
- [[_COMMUNITY_AI Inference & Policy|AI Inference & Policy]]
- [[_COMMUNITY_Client Buffer Impl|Client Buffer Impl]]
- [[_COMMUNITY_RL Training Design|RL Training Design]]
- [[_COMMUNITY_CMake Build Targets|CMake Build Targets]]
- [[_COMMUNITY_Player Spawn Event|Player Spawn Event]]
- [[_COMMUNITY_GUI Map Tile Types|GUI Map Tile Types]]
- [[_COMMUNITY_Raylib Engine Wrapper|Raylib Engine Wrapper]]
- [[_COMMUNITY_GUI Request Struct|GUI Request Struct]]
- [[_COMMUNITY_Server Architecture|Server Architecture]]
- [[_COMMUNITY_Event Scheduler Impl|Event Scheduler Impl]]
- [[_COMMUNITY_Broadcast & GameEnd Events|Broadcast & GameEnd Events]]
- [[_COMMUNITY_Client Handshake|Client Handshake]]
- [[_COMMUNITY_AI Command Struct|AI Command Struct]]
- [[_COMMUNITY_Simulator & ADRs|Simulator & ADRs]]
- [[_COMMUNITY_Documentation & Protocol Spec|Documentation & Protocol Spec]]
- [[_COMMUNITY_Headless Simulator API|Headless Simulator API]]
- [[_COMMUNITY_Incantation Start Event|Incantation Start Event]]
- [[_COMMUNITY_Player Move Event|Player Move Event]]
- [[_COMMUNITY_GUI Player Orientation|GUI Player Orientation]]
- [[_COMMUNITY_Raylib Engine Impl|Raylib Engine Impl]]
- [[_COMMUNITY_3D Texture Assets|3D Texture Assets]]
- [[_COMMUNITY_GUI Protocol Parser|GUI Protocol Parser]]
- [[_COMMUNITY_Event Scheduler Tests|Event Scheduler Tests]]
- [[_COMMUNITY_World State Tests|World State Tests]]
- [[_COMMUNITY_Server Header|Server Header]]
- [[_COMMUNITY_Incantation End Event|Incantation End Event]]
- [[_COMMUNITY_Fake AI Test Bot|Fake AI Test Bot]]
- [[_COMMUNITY_Player Level Event|Player Level Event]]
- [[_COMMUNITY_GUI AI Player Header|GUI AI Player Header]]
- [[_COMMUNITY_GUI Main Entry|GUI Main Entry]]
- [[_COMMUNITY_Code Format Script|Code Format Script]]
- [[_COMMUNITY_AI Protocol Header|AI Protocol Header]]
- [[_COMMUNITY_Interface Header|Interface Header]]
- [[_COMMUNITY_Prometheus Metrics|Prometheus Metrics]]
- [[_COMMUNITY_Admin Protocol (Bonus)|Admin Protocol (Bonus)]]
- [[_COMMUNITY_Raylib Post-FX Chain|Raylib Post-FX Chain]]
- [[_COMMUNITY_Naming & Git Conventions|Naming & Git Conventions]]
- [[_COMMUNITY_Dependabot Actions Updates|Dependabot Actions Updates]]

## God Nodes (most connected - your core abstractions)
1. `Interface` - 61 edges
2. `Brain` - 56 edges
3. `Server` - 56 edges
4. `WorldState` - 45 edges
5. `Brain` - 41 edges
6. `aiPlayer` - 28 edges
7. `string` - 26 edges
8. `GuiEmitter` - 25 edges
9. `NetworkLayer` - 24 edges
10. `EventScheduler` - 19 edges

## Surprising Connections (you probably didn't know these)
- `zappy_core static lib (CMake)` --implements--> `Server core + adapters split`  [INFERRED]
  server/CMakeLists.txt → docs/01_architecture/05_simulator.md
- `Dependabot pip updates (ai_python)` --references--> `zappy_train Python package`  [INFERRED]
  .github/dependabot.yml → docs/01_architecture/04_ai_rl.md
- `CI build-and-test job` --references--> `Root CMake project (zappy)`  [INFERRED]
  .github/workflows/ci.yml → CMakeLists.txt
- `GUI-Server protocol` --cites--> `G-YEP-400 Zappy GUI protocol PDF`  [EXTRACTED]
  docs/01_architecture/06_protocols.md → docs/G-YEP-400_zappy_GUI_protocol.pdf
- `MkDocs documentation site` --references--> `G-YEP-400 Zappy GUI protocol PDF`  [EXTRACTED]
  mkdocs.yml → docs/G-YEP-400_zappy_GUI_protocol.pdf

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Offline RL training data flow** — 05_simulator_libzappy_sim, 01_overview_pybind11, 04_ai_rl_zappy_train, 04_ai_rl_torchscript_export, 01_overview_model_pt [EXTRACTED 0.90]
- **Server runtime core components** — 02_server_class_server, 02_server_network_layer, 02_server_world_state, 02_server_event_scheduler, 02_server_protocol_dispatcher [EXTRACTED 0.90]
- **Shared protocol consumers** — 01_overview_zappy_server, 01_overview_zappy_gui, 01_overview_zappy_ai, 06_protocols_ai_protocol, 06_protocols_gui_protocol [INFERRED 0.85]

## Communities (82 total, 14 thin omitted)

### Community 0 - "3D GUI Renderer (raylib)"
Cohesion: 0.03
Nodes (59): Camera3D, GameMap, Interface, GuiState, Texture2D, unique_ptr, applyLightingToModels, _audioReady (+51 more)

### Community 2 - "Server Command Dispatch"
Cohesion: 0.04
Nodes (53): EventScheduler, IncantState, NetworkLayer, args_, broadcast_message_to_ai, cmd_broadcast, cmd_connect_nbr, cmd_eject (+45 more)

### Community 3 - "World State Game Logic"
Cohesion: 0.04
Nodes (43): add_egg, check_win, consume_team_slot, eggs_, eject, find_egg, find_player, find_team_by_name (+35 more)

### Community 4 - "World State Operations"
Cohesion: 0.10
Nodes (43): add_egg(), check_win(), consume_team_slot(), eject(), eject_k_for_victim(), facing_delta(), find_egg(), find_player() (+35 more)

### Community 5 - "AI Baseline Client (Python)"
Cohesion: 0.09
Nodes (12): Namespace, CommandQueue, FrequencyEstimator, main(), NetworkClient, parse_args(), Parser, PendingCommand (+4 more)

### Community 6 - "RL Training Orchestration"
Cohesion: 0.10
Nodes (27): Namespace, Popen, BatchConfig, build_ai_cmd(), build_server_cmd(), find_free_port(), GameResult, GuiObserver (+19 more)

### Community 8 - "GUI Interface Drawing"
Cohesion: 0.10
Nodes (35): Color, applyLightingToModels(), CountLabel, color, count, worldPos, Interface(), Orientation (+27 more)

### Community 9 - "Core Data Types"
Cohesion: 0.07
Nodes (33): Egg, hatched, id, layer, team, x, y, Player (+25 more)

### Community 10 - "GUI Protocol Wire Commands"
Cohesion: 0.15
Nodes (33): bct(), ebo(), edi(), enw(), msz(), orient_wire(), pbc(), pdi() (+25 more)

### Community 11 - "GUI State Model"
Cohesion: 0.12
Nodes (26): EggInfo, x, y, GuiState, eggs, frequency, hasWinner, incanting (+18 more)

### Community 12 - "Server Network Layer"
Cohesion: 0.10
Nodes (21): ConnectHandler, DisconnectHandler, LineHandler, accept_new_connection, broadcast_all, broadcast_gui, clients_, close_client (+13 more)

### Community 13 - "GUI Event Emitter"
Cohesion: 0.08
Nodes (25): GuiEmitter, bct, ebo, edi, enw, msz, pbc, pdi (+17 more)

### Community 14 - "GUI Request Parsing"
Cohesion: 0.16
Nodes (23): ParsedGuiRequest, parse_gui_request(), parse_int(), token(), string_view, main(), test_gui_emitter_bct(), test_gui_emitter_enw() (+15 more)

### Community 15 - "Game Rules & Elevation"
Cohesion: 0.17
Nodes (22): broadcast_direction(), can_elevate(), consume_elevation_stones(), ElevationRequirement, players, stones, orient_facing(), orient_right() (+14 more)

### Community 16 - "AI Command Handlers"
Cohesion: 0.21
Nodes (23): broadcast_message_to_ai(), cmd_broadcast(), cmd_connect_nbr(), cmd_eject(), cmd_fork(), cmd_forward(), cmd_incantation(), cmd_incantation_complete() (+15 more)

### Community 17 - "AI Protocol Formatting"
Cohesion: 0.17
Nodes (20): fmt_connect_nbr(), fmt_inventory(), fmt_look(), parse_ai_command(), resource_index_of(), LookTile, optional, ParsedCommand (+12 more)

### Community 18 - "Server Lifecycle & Scheduling"
Cohesion: 0.18
Nodes (21): complete_ai_handshake(), complete_gui_handshake(), handle_handshake_line(), init_world(), kill_player(), ms_until_next_event(), now_ticks(), on_client_connect() (+13 more)

### Community 19 - "Event Scheduler"
Cohesion: 0.11
Nodes (16): EventScheduler, advance_to, cancel, cancelled_, next_id_, next_seq_, now_, queue_ (+8 more)

### Community 20 - "CLI Argument Parsing"
Cohesion: 0.19
Nodes (20): consume_value(), is_help_flag(), is_numeric_option(), is_option(), parse_args(), parse_numeric_option(), parse_team_names(), RequiredOptions (+12 more)

### Community 21 - "GUI Player Entity"
Cohesion: 0.10
Nodes (9): aiPlayer, alive, _id, level, life_units, orientation, _team, x (+1 more)

### Community 22 - "Client I/O Buffering"
Cohesion: 0.12
Nodes (16): ClientState, deque, Client, command_queue, drain_lines, enqueue, enqueue_front, fd (+8 more)

### Community 23 - "Legacy AI Baseline"
Cohesion: 0.16
Nodes (8): Namespace, CommandQueue, main(), parse_args(), PendingCommand, print_usage(), State, WorldMemory

### Community 24 - "AI Frequency & Network Sync"
Cohesion: 0.18
Nodes (5): FrequencyEstimator, NetworkClient, Mesure la fréquence RÉELLE du serveur pour s'y synchroniser.      Principe (cf., Envoie une commande et mesure le temps jusqu'à SA réponse.         Renvoie le te, Récupère les lignes bufferisées non consommées (à ré-injecter         dans le tr

### Community 25 - "AI Response Parser"
Cohesion: 0.19
Nodes (4): Parser, Lance un nouveau client zappy_ai pour occuper un slot libre         (un œuf de l, Propage la fréquence (mesurée ou override) à tous les composants         qui en, ZappyAI

### Community 26 - "GUI Components Overview"
Cohesion: 0.18
Nodes (14): zappy_gui component, Recorder (.zrec writer), GuiClient (raylib) protocol parser, Raylib GUI renderer, GuiClient (vulkan) protocol parser, Vulkan 1.3 GUI renderer, AI-Server protocol, GUI-Server protocol (+6 more)

### Community 27 - "Training Evaluation Metrics"
Cohesion: 0.26
Nodes (13): Namespace, all_teams(), _avg(), global_metrics(), load_results(), main(), parse_args(), print_report() (+5 more)

### Community 28 - "Network Layer Implementation"
Cohesion: 0.23
Nodes (13): accept_new_connection(), broadcast_all(), broadcast_gui(), close_client(), find_client(), poll_once(), rebuild_pollfds(), send_to() (+5 more)

### Community 29 - "Player/Resource Events"
Cohesion: 0.15
Nodes (13): EvPlayerDie, id, EvPlayerExpel, id, EvPlayerFork, id, EvResourceDrop, id (+5 more)

### Community 30 - "GUI Game Map"
Cohesion: 0.15
Nodes (13): addPlayerToTile, addResource, getHeight, getIndex, getWidth, _height, isValidCoordinate, removePlayerFromTile (+5 more)

### Community 31 - "GUI Network Client"
Cohesion: 0.17
Nodes (10): NetClient, string, _buf, connect, drainLines, _fd, handshake, poll (+2 more)

### Community 32 - "GUI Game Map Operations"
Cohesion: 0.20
Nodes (3): addPlayerToTile(), aiPlayer, removePlayerFromTile()

### Community 33 - "GUI Net Client Impl"
Cohesion: 0.33
Nodes (9): connect(), string, string_view, vector, drainLines(), handshake(), poll(), readLineBlocking() (+1 more)

### Community 34 - "Network Tests"
Cohesion: 0.35
Nodes (10): main(), test_command_queue_max(), test_command_queue_ordering(), test_drain_lines_complete(), test_drain_lines_crlf(), test_drain_lines_empty(), test_drain_lines_partial(), test_enqueue_appends_newline() (+2 more)

### Community 35 - "Egg Events"
Cohesion: 0.20
Nodes (10): EvEggDie, egg, EvEggHatch, egg, EvEggNew, egg, layer, x (+2 more)

### Community 36 - "Tile & Inventory Events"
Cohesion: 0.20
Nodes (10): EvPlayerInv, id, inv, x, y, EvTileUpdate, resources, x (+2 more)

### Community 37 - "Server Arguments"
Cohesion: 0.24
Nodes (9): ServerArgs, clients_per_team, frequency, height, port, teams, width, string (+1 more)

### Community 38 - "AI Inference & Policy"
Cohesion: 0.22
Nodes (9): model.pt TorchScript checkpoint, Offline RL training pipeline, zappy_ai component, Coded team broadcast protocol, Inference policy (libtorch), Rule-based FSM fallback policy, TorchScript export to model.pt, zappy_ai C++ binary (+1 more)

### Community 39 - "Client Buffer Impl"
Cohesion: 0.25
Nodes (6): drain_lines(), enqueue(), enqueue_front(), string, string_view, vector

### Community 40 - "RL Training Design"
Cohesion: 0.25
Nodes (8): Discrete action space, Curriculum learning stages, Multi-objective reward shaping, Self-play tournament + ELO, zappy_train Python package, CI build-and-test job, CI push-to-mirror job, Dependabot pip updates (ai_python)

### Community 41 - "CMake Build Targets"
Cohesion: 0.39
Nodes (8): zappy_protocol header-only target, Root CMake project (zappy), Release packaging workflow, zappy_core static lib (CMake), zappy_server executable (CMake), zappy_server_net lib (CMake), zappy_server_protocol lib (CMake), server/ README (layout & boundaries)

### Community 42 - "Player Spawn Event"
Cohesion: 0.25
Nodes (8): EvPlayerNew, id, level, o, team, x, y, TeamId

### Community 43 - "GUI Map Tile Types"
Cohesion: 0.32
Nodes (7): aiPlayer, array, vector, MapTile, players, resources, MAP_RESOURCE_COUNT

### Community 44 - "Raylib Engine Wrapper"
Cohesion: 0.25
Nodes (6): RaylibEngine, Texture2D, _background, beginDrawing, endDrawing, shouldClose

### Community 45 - "GUI Request Struct"
Cohesion: 0.25
Nodes (7): GuiRequest, ParsedGuiRequest, n, t, type, x, y

### Community 46 - "Server Architecture"
Cohesion: 0.38
Nodes (7): class Server, EventScheduler, NetworkLayer (poll/asio), protocol_ai parser, ProtocolDispatcher, protocol_gui emitter, WorldState

### Community 47 - "Event Scheduler Impl"
Cohesion: 0.38
Nodes (6): Callback, advance_to(), cancel(), schedule(), Tick, uint64_t

### Community 48 - "Broadcast & GameEnd Events"
Cohesion: 0.38
Nodes (6): EvGameEnd, team_name, EvPlayerBroadcast, id, text, string

### Community 49 - "Client Handshake"
Cohesion: 0.29
Nodes (6): HandshakeResult, handle_handshake(), string, string_view, Team, vector

### Community 50 - "AI Command Struct"
Cohesion: 0.33
Nodes (6): ParsedCommand, arg, cmd, resource_index, Command, string

### Community 51 - "Simulator & ADRs"
Cohesion: 0.33
Nodes (6): Architectural decision records (ADRs), libzappy_sim headless simulator, pybind11 binding layer, zappy_sim_env (PettingZoo), Bindless descriptor indexing, Vulkan frame graph (deferred passes)

### Community 52 - "Documentation & Protocol Spec"
Cohesion: 0.33
Nodes (6): zappy_server component, G-YEP-400 Zappy GUI protocol PDF, Monorepo directory structure, UML overview diagrams, Deploy Docs workflow, MkDocs documentation site

### Community 53 - "Headless Simulator API"
Cohesion: 0.33
Nodes (6): Server simulation mode, Observation space (per drone), Conformity test (sim vs runtime), Server core + adapters split, libzappy_sim static lib, Sim public API (reset/step)

### Community 54 - "Incantation Start Event"
Cohesion: 0.33
Nodes (6): EvIncantStart, level, participants, x, y, vector

### Community 55 - "Player Move Event"
Cohesion: 0.33
Nodes (6): EvPlayerMove, id, o, x, y, Orientation

### Community 58 - "3D Texture Assets"
Cohesion: 0.60
Nodes (5): Background (Starry Galaxy Skybox), Monster Green Base Color Texture, Roast Chicken Base Color Texture, Dark Soil/Ground Texture (Cratered), Orange Soil/Ground Texture (Red Dirt)

### Community 60 - "GUI Protocol Parser"
Cohesion: 0.40
Nodes (4): GameMap, GuiState, ProtocolParser, apply

### Community 61 - "Event Scheduler Tests"
Cohesion: 0.70
Nodes (4): main(), test_cancel(), test_fifo_within_same_tick(), test_tick_ordering()

### Community 62 - "World State Tests"
Cohesion: 0.70
Nodes (4): main(), test_add_player(), test_tile_identity_across_wrap(), test_toroidal_wrap()

### Community 63 - "Server Header"
Cohesion: 0.50
Nodes (3): atomic, unordered_map, unordered_set

### Community 64 - "Incantation End Event"
Cohesion: 0.50
Nodes (4): EvIncantEnd, success, x, y

### Community 65 - "Fake AI Test Bot"
Cohesion: 0.67
Nodes (3): main(), Event, run_bot()

### Community 66 - "Player Level Event"
Cohesion: 0.67
Nodes (3): EvPlayerLevel, id, level

## Knowledge Gaps
- **424 isolated node(s):** `State`, `Namespace`, `State`, `Namespace`, `Namespace` (+419 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **14 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Interface` connect `3D GUI Renderer (raylib)` to `Interface Header`?**
  _High betweenness centrality (0.005) - this node is a cross-community bridge._
- **Why does `Brain` connect `AI Brain Strategy` to `AI Team Broadcast`, `Legacy AI Baseline`?**
  _High betweenness centrality (0.005) - this node is a cross-community bridge._
- **Why does `Brain` connect `AI Brain Decision Logic` to `AI Baseline Client (Python)`?**
  _High betweenness centrality (0.004) - this node is a cross-community bridge._
- **What connects `State`, `Namespace`, `State` to the rest of the system?**
  _445 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `3D GUI Renderer (raylib)` be split into smaller, more focused modules?**
  _Cohesion score 0.03333333333333333 - nodes in this community are weakly interconnected._
- **Should `AI Brain Decision Logic` be split into smaller, more focused modules?**
  _Cohesion score 0.09545454545454546 - nodes in this community are weakly interconnected._
- **Should `Server Command Dispatch` be split into smaller, more focused modules?**
  _Cohesion score 0.03773584905660377 - nodes in this community are weakly interconnected._