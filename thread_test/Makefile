CXX := g++
CXXFLAGS := -Wall -std=c++98

CPPFLAGS := -I$(SYSTEMC_ROOT)/include
LDFLAGS := -L$(SYSTEMC_ROOT)/lib-linux64
LDLIBS := -lsystemc

SRC := hello.cc
BIN := hello

.PHONY: all clean

all: $(BIN)
	./$(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	-@$(RM) -rv $(BIN)