## Makefile for zappy_ai Python baseline + RL train

NAME	= zappy_ai
SRC	= baseline/zappy_ai_baseline.py

PYTHON		?= python3

HOST		?= localhost
PORT		?= 4242
FREQUENCY	?= 100
EPISODES	?= 271000
MAX_STEPS	?= 700

TEAMS		?= RL1 RL2 RL3
AI_PER_TEAM	?= 6

TRAIN_OUT	?= runs
TRAIN_LOG_DIR	?= runs/logs
TRAIN_PID_DIR	?= runs/pids
TRAIN_ARGS	?=

all:	$(NAME)

$(NAME):	$(SRC)
	cp $(SRC) $(NAME)
	chmod +x $(NAME)

train:
	cp rl/train.py train_ai
	chmod +x train_ai

eval:
	cp rl/eval.py rl/eval_ai
	chmod +x rl/eval_ai

# Lance plusieurs IA train sur un serveur déjà lancé.
#
# Exemple :
#   make train-multi TEAMS="RL1 RL2 RL3" AI_PER_TEAM=4 EPISODES=1000 FREQUENCY=100
#
# Ça lance :
#   RL1 -> 4 IA
#   RL2 -> 4 IA
#   RL3 -> 4 IA
train-multi:
	@mkdir -p $(TRAIN_LOG_DIR) $(TRAIN_PID_DIR)
	@echo "[make] serveur      : $(HOST):$(PORT)"
	@echo "[make] teams        : $(TEAMS)"
	@echo "[make] IA par team  : $(AI_PER_TEAM)"
	@echo "[make] fréquence    : $(FREQUENCY)"
	@for team in $(TEAMS); do \
		for id in $$(seq 1 $(AI_PER_TEAM)); do \
			out_dir="$(TRAIN_OUT)/$${team}/ai_$${id}"; \
			log="$(TRAIN_LOG_DIR)/train_$${team}_ai_$${id}.log"; \
			pidfile="$(TRAIN_PID_DIR)/train_$${team}_ai_$${id}.pid"; \
			mkdir -p "$$out_dir"; \
			echo "[make] start $$team IA $$id"; \
			nohup $(PYTHON) -u rl/train.py \
				--host $(HOST) \
				--port $(PORT) \
				--team "$$team" \
				--frequency $(FREQUENCY) \
				--episodes $(EPISODES) \
				--max-steps $(MAX_STEPS) \
				--out-dir "$$out_dir" \
				$(TRAIN_ARGS) \
				> "$$log" 2>&1 & \
			echo $$! > "$$pidfile"; \
			echo "       pid=$$(cat $$pidfile) log=$$log"; \
		done; \
	done
	@echo "[make] lancé. Pour voir les logs : make train-logs"
	@echo "[make] pour stop : make stop-train"

# Voir les logs en live
train-logs:
	tail -f $(TRAIN_LOG_DIR)/train_*.log

# Voir les process lancés
train-ps:
	@ps aux | grep '[p]ython.*rl/train.py' || true

# Stop toutes les IA train lancées par cette règle
stop-train:
	@if [ -d "$(TRAIN_PID_DIR)" ]; then \
		for pidfile in $(TRAIN_PID_DIR)/*.pid; do \
			[ -f "$$pidfile" ] || continue; \
			pid=$$(cat "$$pidfile"); \
			echo "[make] kill pid=$$pid"; \
			kill "$$pid" 2>/dev/null || true; \
			rm -f "$$pidfile"; \
		done; \
	fi

venv:
	rl/setup_venv.sh

clean:
	rm -f $(NAME) train_ai rl/eval_ai

fclean: clean
	rm -rf runs

re: clean all

.PHONY: all clean fclean re train eval venv train-multi train-logs train-ps stop-train