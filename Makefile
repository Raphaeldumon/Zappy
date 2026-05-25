NAME	= zappy

all:	$(NAME)

$(NAME):
	@echo "Nothing to build yet"

clean:
	@echo "Nothing to clean"

fclean:	clean

re:	fclean all

.PHONY:	all clean fclean re
