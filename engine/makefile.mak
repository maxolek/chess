# compiler and flags
CXX = g++
CXXFLAGS = -g

# include directories (if any, useful for header files in different directories)
INCLUDES = 

# source files
SOURCES = helpers.cpp PrecomputedMoveData.cpp gamestate.cpp board.cpp moveGenerator.cpp arbiter.cpp testing.cpp

# object files
OBJECTS = $(SOURCES:.cpp=.o)

# output executable
TARGET = testing.exe

#default target
all: $(TARGET)

# link object files to create the final executable
$(TARGET) : $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

# rule to compile .cpp files into .o (object files)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET)

# rebuild the project from scratch
rebuild: clean all