## Zappy — root Makefile.
## zappy_server: plain g++ via server/Makefile (no cmake).
## zappy_gui / zappy_ai: cmake (other teams).

JOBS     ?= $(shell nproc 2>/dev/null || echo 4)

BINARIES := zappy_server zappy_gui zappy_ai

.PHONY: all clean fclean re

all: zappy_server

zappy_server:
	$(MAKE) -C server all

clean:
	$(MAKE) -C server clean

fclean:
	$(MAKE) -C server fclean
	$(RM) $(BINARIES)

re: fclean all
