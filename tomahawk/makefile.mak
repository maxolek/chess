# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall

# Toggle debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -DDEBUG
    TARGET = testing_debug.exe
else
    CXXFLAGS += -O2
    TARGET = testing.exe
endif

# Source files
SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
       PrecomputedMoveData.cpp searcher.cpp testing.cpp

OBJS = $(SRCS:.cpp=.o)

# Default build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
