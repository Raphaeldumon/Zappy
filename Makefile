## Zappy — Epitech-compliant wrapper around CMake.
## Produces zappy_server at the repository root. The AI is pure Python (ai/).

BUILD_DIR    ?= build
BUILD_TYPE   ?= Release
JOBS         ?= $(shell nproc 2>/dev/null || echo 4)
CMAKE        ?= cmake

BINARIES     := zappy_server

# ---- demo knobs — override on the command line -------------------------------
#   make demo PORT=4242 MAP_W=20 MAP_H=20 BOTS=5 FREQ=100
PORT         ?= 4242
MAP_W        ?= 10
MAP_H        ?= 10
TEAMS        ?= red blue
CLIENTS      ?= 5
FREQ         ?= 100
BOTS         ?= 3
DELAY        ?= 0.3

# Mandatory Epitech rules: all, clean, fclean, re
.PHONY: all build clean fclean re tests_run config coverage format help demo

all: build

config:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: config
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	@for b in $(BINARIES); do \
		if [ -f "$(BUILD_DIR)/bin/$$b" ]; then \
			cp -f "$(BUILD_DIR)/bin/$$b" "./$$b"; \
			echo "  -> ./$$b"; \
		fi; \
	done

## Run the full CTest suite.
tests_run: config
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

coverage:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DZAPPY_ENABLE_COVERAGE=ON
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@command -v gcovr >/dev/null && gcovr -r . $(BUILD_DIR) || echo "install gcovr for a report"

## Launch the server + fake AI bots for a quick smoke run (Ctrl+C stops both).
##   make demo
##   make demo MAP_W=20 MAP_H=20 BOTS=6 FREQ=200
demo: build
	@echo "demo: server :$(PORT) | map $(MAP_W)x$(MAP_H) | teams '$(TEAMS)' | $(BOTS) bot(s) | freq $(FREQ)"
	@set -e; \
	"$(BUILD_DIR)/bin/zappy_server" -p $(PORT) -x $(MAP_W) -y $(MAP_H) -n $(TEAMS) -c $(CLIENTS) -f $(FREQ) & \
	SRV=$$!; \
	trap 'kill $$SRV 2>/dev/null' EXIT INT TERM; \
	sleep 0.5; \
	python3 tools/fake_ai.py -p $(PORT) -n $(firstword $(TEAMS)) --bots $(BOTS) --delay $(DELAY)

format:
	@bash tools/format_all.sh

clean:
	@[ -d $(BUILD_DIR) ] && $(CMAKE) --build $(BUILD_DIR) --target clean || true

fclean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(BINARIES)

re: fclean all

help:
	@echo "Targets: all build demo tests_run coverage format clean fclean re"
	@echo "Vars   : BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS) BUILD_DIR=$(BUILD_DIR)"
	@echo "demo   : PORT=$(PORT) MAP_W=$(MAP_W) MAP_H=$(MAP_H) TEAMS='$(TEAMS)' CLIENTS=$(CLIENTS) FREQ=$(FREQ) BOTS=$(BOTS) DELAY=$(DELAY)"
