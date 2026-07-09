## Zappy — Epitech root Makefile.
## `make` builds the three mandatory binaries AT THE REPOSITORY ROOT:
##   zappy_server  — C++ / CMake          (server/, via build/)
##   zappy_gui     — C++ / raylib         (gui/, via build/)
##   zappy_ai      — Python               (ai/)
## server and gui are both CMake targets, so their object files live under
## build/ and never clutter the source tree.

BUILD_DIR    ?= build
BUILD_TYPE   ?= Release
JOBS         ?= $(shell nproc 2>/dev/null || echo 4)
CMAKE        ?= cmake

# Root deliverables (subject-mandated names) and the one non-CMake source bin.
SERVER_BIN   := zappy_server
GUI_BIN      := zappy_gui
AI_BIN       := zappy_ai
AI_SRC_BIN   := ai/zappy_ai
BINARIES     := $(SERVER_BIN) $(GUI_BIN) $(AI_BIN)

# ---- demo knobs — override on the command line -------------------------------
#   make demo PORT=4242 MAP_W=20 MAP_H=20 BOTS=4 FREQ=100 TEAMS="red blue"
PORT         ?= 4242
MAP_W        ?= 25
MAP_H        ?= 25
TEAMS        ?= rouge bleu vert jaune
CLIENTS      ?= 50
FREQ         ?= 1000
BOTS         ?= 6
HOST         ?= 127.0.0.1

# Mandatory Epitech rules: all, clean, fclean, re — plus the subject-mandated
# per-binary rules zappy_server / zappy_gui / zappy_ai (eponymous to the bins).
.PHONY: all build zappy_server zappy_gui zappy_ai clean fclean re \
        tests_run config coverage format help demo keynote

all: zappy_server zappy_gui zappy_ai

# ---- CMake configure (server + gui live in one build tree) -------------------
config:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

# ---- zappy_server (C++ / CMake) ----------------------------------------------
zappy_server: config
	$(CMAKE) --build $(BUILD_DIR) --target zappy_server -j $(JOBS)
	@cp -f "$(BUILD_DIR)/bin/$(SERVER_BIN)" "./$(SERVER_BIN)"
	@echo "  -> ./$(SERVER_BIN)"

# Back-compat alias: old habits / scripts invoke `make build` for the server.
build: zappy_server

# ---- zappy_gui (C++ / raylib, gui/) ----------------------------------------
zappy_gui: config
	@if [ -d "$(BUILD_DIR)/gui" ]; then \
		$(CMAKE) --build $(BUILD_DIR) --target zappy_gui -j $(JOBS) && \
		cp -f "$(BUILD_DIR)/bin/$(GUI_BIN)" "./$(GUI_BIN)" && \
		echo "  -> ./$(GUI_BIN)"; \
	else \
		echo "  -- zappy_gui skipped: raylib not found (server/AI built; install raylib for the GUI)"; \
	fi

# ---- zappy_ai (Python, ai/) --------------------------------------------------
zappy_ai:
	$(MAKE) -C ai
	@cp -f "$(AI_SRC_BIN)" "./$(AI_BIN)"
	@echo "  -> ./$(AI_BIN)"

## Run the full CTest suite.
tests_run: config
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

## Coverage: prints a clean line/branch summary (no per-line spam).
## Tests are excluded so the % reflects production code, not the test files.
## (HTML report skipped: gcovr 5.0's HTML writer is broken against jinja2 >= 3.1.)
coverage:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DZAPPY_ENABLE_COVERAGE=ON
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@command -v gcovr >/dev/null && gcovr -r . $(BUILD_DIR) \
		--exclude '.*/tests/.*' --exclude '.*/external/.*' -s \
		|| echo "install gcovr for a report"

## Launch server + real AI bots + the GUI together (Ctrl+C / closing the window
## stops all three). The GUI is run from gui/ so its CWD-relative assets load.
##   make demo
##   make demo MAP_W=30 MAP_H=30 BOTS=6 FREQ=100 TEAMS="red blue"
demo: all
	@echo "demo: server :$(PORT) | map $(MAP_W)x$(MAP_H) | teams '$(TEAMS)' | $(BOTS) AI/team | freq $(FREQ)"
	@set -e; \
	./$(SERVER_BIN) -p $(PORT) -x $(MAP_W) -y $(MAP_H) -n $(TEAMS) -c $(CLIENTS) -f $(FREQ) & \
	SRV=$$!; PIDS="$$SRV"; \
	trap 'kill $$PIDS 2>/dev/null' EXIT INT TERM; \
	sleep 0.6; \
	for t in $(TEAMS); do \
		for i in $$(seq 1 $(BOTS)); do \
			./$(AI_BIN) -p $(PORT) -n $$t -h $(HOST) -f $(FREQ) & \
			PIDS="$$PIDS $$!"; \
		done; \
	done; \
	sleep 0.3; \
	( cd gui && exec ../$(GUI_BIN) -p $(PORT) -h $(HOST) )

## Regénère Keynote.pptx depuis presentation/ (rendu Chrome + assemblage pptx).
PRESENTATION_PY := presentation/.venv/bin/python

keynote:
	@test -x $(PRESENTATION_PY) || { \
		python3 -m venv presentation/.venv && \
		presentation/.venv/bin/pip install --quiet python-pptx pillow; }
	@./presentation/render.sh
	@$(PRESENTATION_PY) presentation/build_pptx.py

format:
	@bash tools/format_all.sh

clean:
	@[ -d $(BUILD_DIR) ] && $(CMAKE) --build $(BUILD_DIR) --target clean || true
	@$(MAKE) -C ai clean

fclean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(BINARIES)
	@$(MAKE) -C ai fclean

re: fclean all

help:
	@echo "Targets: all zappy_server zappy_gui zappy_ai build demo tests_run coverage format clean fclean re"
	@echo "Builds : ./zappy_server  ./zappy_gui  ./zappy_ai"
	@echo "Vars   : BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS) BUILD_DIR=$(BUILD_DIR)"
	@echo "demo   : PORT=$(PORT) MAP_W=$(MAP_W) MAP_H=$(MAP_H) TEAMS='$(TEAMS)' CLIENTS=$(CLIENTS) FREQ=$(FREQ) BOTS=$(BOTS) HOST=$(HOST)"
	@echo "keynote: régénère Keynote.pptx depuis presentation/"
