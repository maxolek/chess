## all backend engine files are already compiled
## via makefile{_dll}.mak
## so this only compiles the endpoint files
## engine.cpp + game.cpp

# Compiler and flags
CXX = g++
CFLAGS = -Wall -shared -fPIC
JFLAGS = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/win32 # Include paths for JNI on Windows

# Toggle debug mode: make DEBUG=1
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -DDEBUG
    TARGET = engine_debug.exe
else
    CXXFLAGS += -O2
    TARGET = main.exe
endif

# Source files
SRCS = engine.cpp game.cpp

OBJS = $(SRCS:.cpp=.o)

# Default build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.exe
