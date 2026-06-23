## Makefile for zappy_ai Python baseline

NAME	=	zappy_ai

SRC	=	baseline/zappy_ai_baseline.py

all:	$(NAME)

$(NAME):	$(SRC)
	cp $(SRC) $(NAME)
	chmod +x $(NAME)

clean:
	rm -f $(NAME)

re:	clean all

.PHONY:	all clean re