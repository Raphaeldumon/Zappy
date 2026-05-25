## Zappy — Epitech-compliant wrapper around CMake.
## Produces zappy_server, zappy_gui, zappy_ai at the repository root.

BUILD_DIR    ?= build
BUILD_TYPE   ?= Release
JOBS         ?= $(shell nproc 2>/dev/null || echo 4)
CMAKE        ?= cmake

BINARIES     := zappy_server zappy_gui zappy_ai

# Mandatory Epitech rules: all, clean, fclean, re
.PHONY: all build clean fclean re tests_run config gui_on coverage format help

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

## Build with the real Vulkan GUI (needs glfw3 + Vulkan SDK installed).
gui_on:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DZAPPY_BUILD_GUI=ON
	$(MAKE) build

## Run the full CTest suite.
tests_run: config
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

coverage:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DZAPPY_ENABLE_COVERAGE=ON
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@command -v gcovr >/dev/null && gcovr -r . $(BUILD_DIR) || echo "install gcovr for a report"

format:
	@bash tools/format_all.sh

clean:
	@[ -d $(BUILD_DIR) ] && $(CMAKE) --build $(BUILD_DIR) --target clean || true

fclean:
	$(RM) -r $(BUILD_DIR)
	$(RM) $(BINARIES)

re: fclean all

help:
	@echo "Targets: all build gui_on tests_run coverage format clean fclean re"
	@echo "Vars   : BUILD_TYPE=$(BUILD_TYPE) JOBS=$(JOBS) BUILD_DIR=$(BUILD_DIR)"
