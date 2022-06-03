TARGET   = fzx

CXXSRCS  = src/fzx.cpp src/queue.cpp src/match.cpp src/allocator.cpp
CSRCS    = src/bonus.c
SRCS     = $(CXXSRCS) $(CSRCS)

INCLUDE  =
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
CFLAGS   = -std=c99 -Wall -Wextra -O3
LFLAGS   = -pthread

CXXOBJS  = $(addprefix build/,$(CXXSRCS:.cpp=.cpp.o))
COBJS    = $(addprefix build/,$(CSRCS:.c=.c.o))
OBJS     = $(CXXOBJS) $(COBJS)
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
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LFLAGS)

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
