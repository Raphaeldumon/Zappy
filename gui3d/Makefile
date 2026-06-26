NAME        =   zappy_gui3d

CXX         =   g++

SRC         =   main.cpp \
                interface.cpp \
                raylibWrapper.cpp \
                gameMap.cpp \
                netClient.cpp \
                protocolParser.cpp

OBJ         =   $(SRC:.cpp=.o)
DEP         =   $(OBJ:.o=.d)

CXXFLAGS    =   -Wall -Wextra -std=c++17 -g -MMD -MP -I.

LDFLAGS     =   -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# ---- demo knobs — override on the command line -------------------------------
#   make demo PORT=4242 MAP_W=20 MAP_H=20 BOTS=6 FREQ=100
PORT        ?=  4242
MAP_W       ?=  20
MAP_H       ?=  20
TEAMS       ?=  red
CLIENTS     ?=  100
FREQ        ?=  1000
BOTS        ?=  8
HOST        ?=  127.0.0.1

# ---- AI study test knobs (make test-ai) --------------------------------------
#   make test-ai GAMES=4 BOTS=8 DUR=900 FREQ=1000
GAMES       ?=  3       # parallel independent games (keep <=4: more cause CPU contention)
DUR         ?=  900     # seconds per run

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $(NAME) $(LDFLAGS)

-include $(DEP)

clean:
	rm -f $(OBJ) $(DEP)

fclean: clean
	rm -f $(NAME)

re: fclean all

## Launch server + fake AI + this GUI together. Closing the GUI window (or
## Ctrl+C) stops all three. Builds the server via the root Makefile first.
##   make demo
##   make demo MAP_W=20 MAP_H=20 BOTS=6 FREQ=100
demo: all
	@$(MAKE) -C .. build >/dev/null
	@$(MAKE) -C ../ai >/dev/null
	@echo "demo: server :$(PORT) | map $(MAP_W)x$(MAP_H) | teams '$(TEAMS)' | $(BOTS) AI bot(s) | freq $(FREQ)"
	@set -e; \
	../zappy_server -p $(PORT) -x $(MAP_W) -y $(MAP_H) -n $(TEAMS) -c $(CLIENTS) -f $(FREQ) & \
	SRV=$$!; \
	PIDS="$$SRV"; \
	trap 'kill $$PIDS 2>/dev/null' EXIT INT TERM; \
	sleep 0.6; \
	for t in $(TEAMS); do \
		for i in $$(seq 1 $(BOTS)); do \
			../ai/zappy_ai -p $(PORT) -n $$t & \
			PIDS="$$PIDS $$!"; \
		done; \
	done; \
	sleep 0.3; \
	./$(NAME) -p $(PORT) -h $(HOST)

## Run the AI study harness: GAMES independent games in parallel for DUR seconds,
## writing a report (time-to-each-level, deaths, forks, population-over-time) to
## ../ai/test_logs/run_<timestamp>/report.txt. No GUI; headless stats only.
##   make test-ai
##   make test-ai GAMES=4 DUR=600 FREQ=1000 BOTS=8 MAP_W=20 MAP_H=20
test-ai:
	@$(MAKE) -C .. build >/dev/null
	@N=$(GAMES) BOTS=$(BOTS) DUR=$(DUR) FREQ=$(FREQ) MAPW=$(MAP_W) MAPH=$(MAP_H) \
		../ai/run_ai_test.sh

.PHONY: all clean fclean re demo test-ai