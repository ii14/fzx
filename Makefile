TARGET   = fzx
SOURCES  = fzx.cpp queue.cpp
CXXFLAGS = -std=c++17 -Wall -Wextra -O3
INCLUDE  =
LDFLAGS  = -pthread

OBJS     = $(addprefix build/,$(SOURCES:.cpp=.o))
DEPS     = $(OBJS:.o=.d)

all: $(TARGET)

build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -MMD -o $@ $<

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

-include $(DEPS)

.PHONY: all clean info

clean:
	@rm -rvf $(TARGET) $(OBJS) $(DEPS)

info:
	@echo "[*] Target:       $(TARGET)"
	@echo "[*] Sources:      $(SOURCES)"
	@echo "[*] Objects:      $(OBJS)"
	@echo "[*] Dependencies: $(DEPS)"
