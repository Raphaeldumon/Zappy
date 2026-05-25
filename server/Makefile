CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic
INCLUDES := -I. -I../ai/include

SRCS :=	core/world_state.cpp \
	core/event_scheduler.cpp \
	net/client.cpp \
	net/network_layer.cpp \
	protocol/handshake.cpp \
	protocol/ai_handler.cpp \
	protocol/gui_emitter.cpp \
	protocol/gui_request_handler.cpp \
	runtime/server.cpp \
	runtime/args.cpp \
	runtime/main.cpp

OBJ_DIR := .obj
OBJS    := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS    := $(OBJS:.o=.d)

TARGET  := ../zappy_server

.PHONY: all clean fclean re

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) $(TARGET)

re: fclean all
