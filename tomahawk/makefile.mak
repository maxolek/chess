# -------------------
# Version
# -------------------
VERSION ?= dev
VERSION_SET := $(filter-out dev,$(VERSION))
BASE_TARGET = tomahawk

ENGINE_DEF = -DENGINE_ID=\"$(VERSION)\"

# -------------------
# Compiler & Flags
# -------------------
CXX = g++
CXXSTD = -std=c++17
COMMON_WARN = -Wall -Wextra -Wshadow -Wuninitialized -Wconversion -Wpedantic
COMMON_LINK = -pthread

EXTRA = -g -fno-optimize-sibling-calls
PROD = -O3 -ffast-math -march=native -flto

# -------------------
# Source files
# -------------------
SRCS = board.cpp gamestate.cpp helpers.cpp moveGenerator.cpp \
       magics.cpp PrecomputedMoveData.cpp UCI.cpp book.cpp \
       searcher.cpp NNUE.cpp evaluator.cpp engine.cpp tomahawk.cpp

# Object files (generated automatically)
OBJS = $(SRCS:.cpp=.o)

# -------------------
# Build configuration
# -------------------
ifeq ($(DEBUG),1)
    CXXFLAGS = $(CXXSTD) -O2 $(ENGINE_DEF) $(EXTRA) $(COMMON_WARN) $(COMMON_LINK) -DDEBUG
    TARGET = debug.exe
else ifeq ($(PROFILE),1)
    CXXFLAGS = $(CXXSTD) -O1 -g $(ENGINE_DEF) $(COMMON_WARN) $(COMMON_LINK)
    TARGET = tomahawk_profile.exe
else
    CXXFLAGS = $(CXXSTD) $(PROD) $(ENGINE_DEF) $(COMMON_WARN) $(COMMON_LINK)
    TARGET = tomahawk.exe
endif

# Optional override if VERSION explicitly set (only affects release builds)
ifneq ($(VERSION_SET),)
    ifneq ($(DEBUG),1)
        TARGET = $(VERSION).exe
    endif
endif

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
