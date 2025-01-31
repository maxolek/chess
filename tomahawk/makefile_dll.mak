# Compiler and tools
CC = gcc  # C/C++ compiler (MinGW or similar for Windows)
CXX = g++  # C++ compiler
CFLAGS = -Wall -shared -fPIC
JFLAGS = -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/win32 # Include paths for JNI on Windows
LDFLAGS = -L. -lengine # Link against engine.dll

# Paths
BUILD_DIR = build
TOMAHAWK_SRC = $(wildcard C:/Users/maxol/code/chess/tomahawk/*.cpp)  # C source files in tomahawk directory
ENGINE_DLL_PATH = C:/Users/maxol/code/chess/tomahawk/engine.dll

# Targets

# Default target: build the DLL
all: $(ENGINE_DLL_PATH)

# Compile the C/C++ code into the engine.dll
$(ENGINE_DLL_PATH): $(TOMAHAWK_SRC)
	$(CXX) $(JFLAGS) $(CFLAGS) -o $(ENGINE_DLL_PATH) $(TOMAHAWK_SRC) $(LDFLAGS)

# Clean up build artifacts
clean:
	del /q $(ENGINE_DLL_PATH)

# Run the build process
run:
	make
