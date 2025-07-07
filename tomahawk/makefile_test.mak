# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall

# Toggle debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -O0 -DDEBUG
    TARGET = debug.exe
else
    CXXFLAGS += -O2
    TARGET = testing.exe
endif

# Source files
SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
       magics.cpp PrecomputedMoveData.cpp game.cpp UCI.cpp \
       searcher.cpp evaluator.cpp engine.cpp testing.cpp

OBJS = $(SRCS:.cpp=.o)

# Default build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
