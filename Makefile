TARGET   = fzx

CXXSRCS  = fzx.cpp queue.cpp match.cpp
CSRCS    = bonus.c
SRCS     = $(CXXSRCS) $(CSRCS)

INCLUDE  =
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
CFLAGS   = -std=c99 -Wall -Wextra -O3
LFLAGS   = -pthread

CXXOBJS  = $(addprefix build/,$(CXXSRCS:.cpp=.cpp.o))
COBJS    = $(addprefix build/,$(CSRCS:.c=.c.o))
OBJS     = $(CXXOBJS) $(COBJS)
DEPS     = $(OBJS:.o=.d)

all: $(TARGET)

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -MMD -o $@ $<

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE) -c -MMD -o $@ $<

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LFLAGS)

-include $(DEPS)

.PHONY: all clean info

clean:
	@rm -rvf $(TARGET) $(OBJS) $(DEPS)

info:
	@echo "[*] Target:       $(TARGET)"
	@echo "[*] Sources:      $(SRCS)"
	@echo "[*] Objects:      $(OBJS)"
	@echo "[*] Dependencies: $(DEPS)"
