TARGET   = lua/fzx.so

INCLUDE  = $(shell pkg-config --cflags luajit libuv) -Ideps/libuv/include
FLAGS    = -fPIC -Wall -Wextra -O3
CXXFLAGS = $(FLAGS) -std=c++14
CFLAGS   = $(FLAGS) -std=c99
LDFLAGS  = -pthread $(shell pkg-config --libs libuv luajit libluv)

CXXSRCS  = src/fzx.cpp src/queue.cpp src/match.cpp src/allocator.cpp
CSRCS    = src/bonus.c

SRCS     = $(CXXSRCS) $(CSRCS)
OBJS     = $(addprefix build/,$(CXXSRCS:.cpp=.cpp.o)) \
	   $(addprefix build/,$(CSRCS:.c=.c.o))
DEPS     = $(OBJS:.o=.d)

all: $(TARGET) compile_commands.json

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -MMD -o $@ $<

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE) -c -MMD -o $@ $<

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -shared -o $(TARGET) $(OBJS) $(LDFLAGS)

compile_commands.json: Makefile
	command -v compiledb && compiledb -n make

-include $(DEPS)

.PHONY: all clean info

clean:
	@rm -rvf $(TARGET) $(OBJS) $(DEPS)

info:
	@echo "[*] Target:       $(TARGET)"
	@echo "[*] Sources:      $(SRCS)"
	@echo "[*] Objects:      $(OBJS)"
	@echo "[*] Dependencies: $(DEPS)"
