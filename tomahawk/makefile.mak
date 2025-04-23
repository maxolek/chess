# Source files (ensure all relevant .cpp files are listed)
SRC_FILES = helpers.cpp PrecomputedMoveData.cpp gamestate.cpp board.cpp moveGenerator.cpp arbiter.cpp testing.cpp

# Object files (compiled from the source files)
OBJECTS = $(SRC_FILES:.cpp=.o)

# Executable name
EXEC_TARGET = testing.exe

# Compiler flags
CXX = g++
CXXFLAGS = -g -Wall

# Default target (this will be the entry point)
all: $(EXEC_TARGET)

# Rule to link object files to create the executable
$(EXEC_TARGET): $(OBJECTS)
	echo $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(EXEC_TARGET)


# Rule to compile .cpp files into .o (object files)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up generated files
clean:
	del -f $(OBJECTS) $(EXEC_TARGET)

# Rebuild the project from scratch (clean + build)
rebuild: clean all
