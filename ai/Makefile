## Makefile for zappy_ai — the rule-based Python AI (baseline).
## The root Makefile's `zappy_ai` target invokes this, then copies ./zappy_ai
## up to the repository root.

NAME	= zappy_ai
SRC	= baseline/zappy_ai_baseline.py
# The deliverable is bundled from the split modules; regenerate it whenever a
# module changes so a stale checked-in bundle can never ship silently.
MODULES	= $(filter-out $(SRC),$(wildcard baseline/*.py))

all:	$(NAME)

$(SRC):	$(MODULES)
	python3 baseline/_bundle.py > $(SRC)
	chmod +x $(SRC)

$(NAME):	$(SRC)
	cp $(SRC) $(NAME)
	chmod +x $(NAME)

clean:
	rm -f $(NAME)

fclean: clean

re: fclean all

.PHONY: all clean fclean re
