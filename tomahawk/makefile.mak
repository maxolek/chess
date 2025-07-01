# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -pthread

# Toggle debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -O0 -DDEBUG
    TARGET = debug.exe
else
    CXXFLAGS += -O2
    TARGET = tomahawk.exe
endif

# Source files
SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
       PrecomputedMoveData.cpp game.cpp UCI.cpp \
       searcher.cpp evaluator.cpp engine.cpp tomahawk.cpp

OBJS = $(SRCS:.cpp=.o)

# Default build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
