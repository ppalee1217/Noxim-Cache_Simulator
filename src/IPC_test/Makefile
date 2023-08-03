CC := g++
CXX := g++
CFLAGS := -Wall -Wextra -std=gnu99 -g -O0
CXXFLAGS := -Wall -std=c++11 -g -O0

CPPFLAGS := -I$(SYSTEMC_ROOT)/include
LDFLAGS := -L$(SYSTEMC_ROOT)/lib-linux64
LDLIBS := -lrt

BIN_DIR := bin
BIN_READER := $(BIN_DIR)/reader

.PHONY: all clean format

all: $(BIN_READER)

$(BIN_DIR)/%: %.cc
	-@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# $(BIN_DIR)/%: %.cc
# 	-@mkdir -p $(@D)
# 	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lsystemc

clean:
	-@$(RM) -rv $(BIN_DIR)

format:
	-clang-format -i *.c *.cc
