NAME        =   zappy_gui3d

CXX         =   g++

SRC         =   main.cpp \
                interface.cpp \
                raylibWrapper.cpp \
                gameMap.cpp

OBJ         =   $(SRC:.cpp=.o)

CXXFLAGS    =   -Wall -Wextra -std=c++17 -I.

LDFLAGS     =   -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re