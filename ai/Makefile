## Makefile for zappy_ai — the rule-based Python AI (baseline).
## The root Makefile's `zappy_ai` target invokes this, then copies ./zappy_ai
## up to the repository root.

NAME	= zappy_ai
SRC	= baseline/zappy_ai_baseline.py

all:	$(NAME)

$(NAME):	$(SRC)
	cp $(SRC) $(NAME)
	chmod +x $(NAME)

clean:
	rm -f $(NAME)

fclean: clean

re: fclean all

.PHONY: all clean fclean re
