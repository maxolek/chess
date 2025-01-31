# Set variables for your source and build paths
SRC_DIR := .                  # The current directory, where the Makefile is located
BIN_DIR := ../bin             # Compiled files should go in bin (one level up)
PACKAGE := gui                # The package name for the main class (gui package)

# Find all Java files in the src directory
SOURCES := $(shell find $(SRC_DIR) -name "*.java")
# Generate corresponding .class files in the bin directory
CLASSES := $(SOURCES:$(SRC_DIR)/%.java=$(BIN_DIR)/%.class)

# Default target - compile and run the program
default: compile run

# Compile the Java files
$(BIN_DIR)/%.class: $(SRC_DIR)/%.java
	@echo "Compiling $<..."
	@mkdir -p $(BIN_DIR)/$(dir $*)
	@javac -d $(BIN_DIR) $<

# Compile all the classes
compile: $(CLASSES)

# Run the main program
run: $(BIN_DIR)/gui/Main.class
	@echo "Running the program..."
	@java -cp $(BIN_DIR) $(PACKAGE).Main

# Clean the build directory
clean:
	@echo "Cleaning up..."
	@rm -rf $(BIN_DIR)

# Phony targets (not actual files)
.PHONY: default compile run clean
