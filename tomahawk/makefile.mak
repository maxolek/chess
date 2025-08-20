# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -pthread

# Toggle debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CXX = C:/msys64/mingw64/bin/clang++.exe
    CXXFLAGS += -g -DDEBUG -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
    CXXFLAGS += -Wextra -Wshadow -Wuninitialized -Wconversion -Wpedantic
    TARGET = debug.exe
# toggle test mode: make TEST=1
else ifeq ($(TEST),1)
    CXXFLAGS += -O2
    TARGET = testing.exe
else
    CXXFLAGS += -O2
    TARGET = tomahawk.exe
endif

# Source files
SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
       magics.cpp PrecomputedMoveData.cpp game.cpp UCI.cpp \
       searcher.cpp evaluator.cpp engine.cpp testing.cpp
       #tomahawk.cpp        

OBJS = $(SRCS:.cpp=.o)

# Default build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
