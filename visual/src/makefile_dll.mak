# Java tools
JAVAC = javac
JAVA = java

# Paths
JAVA_SRC_DIR = C:/Users/maxol/code/chess/visual/src
ENGINE_DLL_PATH = C:/Users/maxol/code/chess/tomahawk/engine.dll
ENGINE_DLL_DIR = C:/Users/maxol/code/chess/tomahawk
BUILD_DIR = C:/Users/maxol/code/chess/visual/build

# Use PowerShell to find all Java files
JAVA_SRC = $(shell powershell -Command "Get-ChildItem -Recurse -Filter *.java -Path $(JAVA_SRC_DIR) | Select-Object -ExpandProperty FullName")

# Outputs (compiled Java classes)
JAVA_CLASSES = $(patsubst $(JAVA_SRC_DIR)/%.java, $(BUILD_DIR)/%.class, $(JAVA_SRC))

# Default target: compile Java code
all: $(JAVA_CLASSES)

# Compile the Java code (all required source files)
$(BUILD_DIR)/%.class: $(JAVA_SRC_DIR)/%.java
	$(JAVAC) -d $(BUILD_DIR) $<

# Run the Java program (assuming the main class is JChess)
run: $(JAVA_CLASSES) $(ENGINE_DLL_PATH)
	$(JAVA) -Djava.library.path=$(ENGINE_DLL_DIR) -cp $(BUILD_DIR) JChess

# Clean up compiled Java classes
clean:
	del /q $(BUILD_DIR)\*.class
