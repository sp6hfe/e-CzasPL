APP_NAME = eCzasPL

CXX      = g++
CXXFLAGS = -std=gnu++17 -Wall -Wextra -Werror
LDFLAGS  = -lm
BUILD    = ./build
OBJ_DIR  = $(BUILD)/objects
APP_DIR  = $(BUILD)/apps
TARGET   = program

INCLUDE  =                           \
   -Iinc/                            \
   -Ilib/

SRC      =                           \
   $(wildcard src/DataDecoder/*.cpp) \
   $(wildcard src/Tools/*.cpp)       \
   $(wildcard src/*.cpp)             \
   $(wildcard lib/ReedSolomon/*.cpp)

OBJECTS  = $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES \
         = $(OBJECTS:.o=.d)

# targets for all objects
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

# application target
$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(APP_NAME) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

# build targets
.DEFAULT_GOAL := release

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O2
release: all

all: clean build $(APP_DIR)/$(TARGET)

info:
	@echo "[*] Application dir: ${APP_DIR}     "
	@echo "[*] Object dir:      ${OBJ_DIR}     "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "
	@echo "[*] Dependencies:    ${DEPENDENCIES}"

# targets not associated with files (timestamp check) execuded always
.PHONY: clean build debug release all info