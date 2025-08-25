# -------------------
# Compiler & Flags
# -------------------

CXX = clang++
CXXSTD = -std=c++17
COMMON_WARN = -Wall -Wextra -Wshadow -Wuninitialized -Wconversion -Wpedantic
COMMON_LINK = -pthread

# Sanitizer flags
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
UBSAN_FLAGS = -fsanitize=undefined -fno-sanitize-recover=undefined

EXTRA = -g -fno-optimize-sibling-calls
PROD = -O3 -ffast-math -march=native -flto #-fprofile-generate

# -------------------
# Source Files
# -------------------

PROD_SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
            magics.cpp PrecomputedMoveData.cpp game.cpp UCI.cpp \
            searcher.cpp evaluator.cpp engine.cpp tomahawk.cpp

TEST_SRCS = arbiter.cpp board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
            magics.cpp PrecomputedMoveData.cpp game.cpp UCI.cpp \
            searcher.cpp evaluator.cpp engine.cpp testing.cpp

# -------------------
# Build Configuration
# -------------------

ifeq ($(DEBUG_OPT),1) # prod testing
    CXXFLAGS = $(CXXSTD) -O0 $(EXTRA) $(COMMON_WARN) $(COMMON_LINK) -DDEBUG $(ASAN_FLAGS)
    TARGET = debug_opt_sanitize.exe
    SRCS = $(PROD_SRCS)
else ifeq ($(DEBUG),1) # test testing
    CXXFLAGS = $(CXXSTD) -O0 $(EXTRA) $(COMMON_WARN) $(COMMON_LINK) -DDEBUG $(ASAN_FLAGS)
    TARGET = debug_noopt_sanitize.exe
    SRCS = $(TEST_SRCS)
else ifeq ($(TEST),1)
    CXXFLAGS = $(CXXSTD) $(PROD) $(COMMON_WARN) $(COMMON_LINK)
    TARGET = testing.exe
    SRCS = $(TEST_SRCS)
else
    CXXFLAGS = $(CXXSTD) $(PROD) $(COMMON_WARN) $(COMMON_LINK)
    TARGET = tomahawk.exe
    SRCS = $(PROD_SRCS)
endif

OBJS = $(SRCS:.cpp=.o)

# -------------------
# Rules
# -------------------

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
