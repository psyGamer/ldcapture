BUILD_DIR := bin
OBJ_DIR := obj

ASSEMBLY := ldcapture
EXTENSION := .so
COMPILER_FLAGS := -fPIC
INCLUDE_FLAGS := -Isrc -Ivendor/STC/include
LINKER_FLAGS := -shared -lm
DEFINES := 

SRC_FILES := $(shell find src -name *.c -or -name *.cpp)	# .c and .cpp files
DIRECTORIES := $(shell find src -type d)					# directories with .h files
OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)					# compiled .o objects

all: scaffold compile link

debug: COMPILER_FLAGS += -g -O0 -fsanitize=address
debug: LINKER_FLAGS += -fsanitize=address -static-libsan
debug: DEFINES += -D_GLIBCXX_DEBUG -D_DEBUG
debug: EXTENSION := -debug
debug: all
release: COMPILER_FLAGS += -O3
release: EXTENSION := -release
release: all

.PHONY: scaffold
scaffold: # create build directory
	@echo Scaffolding folder structure...
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
	@mkdir -p $(BUILD_DIR)

.PHONY: link
link: scaffold $(OBJ_FILES) # link
	@echo Linking $(ASSEMBLY)...
	clang++ $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)

.PHONY: compile
compile: #compile .c and .cpp files
	@echo Compiling...

.PHONY: clean
clean: # clean build directory
	rm -rf $(BUILD_DIR)
	rm -rf $(OBJ_DIR)

$(OBJ_DIR)/%.c.o: %.c # compile .c to .o object
	@echo   $<...
	clang -std=c17 $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

$(OBJ_DIR)/%.cpp.o: %.cpp # compile .cpp to .o object
	@echo   $<...
	clang++ -std=c++17 $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)
