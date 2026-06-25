## Makefile for zappy_ai Python baseline

NAME	=	zappy_ai

SRC	=	baseline/zappy_ai_baseline.py

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

venv:
	rl/setup_venv.sh

clean:
	rm -f $(NAME) train_ai rl/eval_ai

re:	clean all

.PHONY:	all clean re