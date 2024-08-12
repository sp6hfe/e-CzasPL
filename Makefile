# https://www.softwaretestinghelp.com/cpp-makefile-tutorial/
# https://gist.github.com/mauriciopoppe/de8908f67923091982c8c8136a063ea6
# https://www.partow.net/programming/makefile/index.html

APP_NAME = eCzasDecoder

CXX      = g++
CXXFLAGS = -std=gnu++17 -Wall -Wextra -Werror
LDFLAGS  = -lm
BUILD    = ./build
OBJ_DIR  = $(BUILD)/objects
APP_DIR  = $(BUILD)/apps
TARGET   = program

INCLUDE  = -Iinc/
SRC      =                      \
   $(wildcard src/DataDecoder/*.cpp) \
   $(wildcard src/*.cpp)         \

OBJECTS  = $(SRC:%.cpp=$(OBJ_DIR)/%.o)
DEPENDENCIES \
         = $(OBJECTS:.o=.d)

# target all
all: build $(APP_DIR)/$(TARGET)

# targets for all objects
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

# application target
$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(APP_NAME) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

.PHONY: all build clean debug release info

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O2
release: all

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*

info:
	@echo "[*] Application dir: ${APP_DIR}     "
	@echo "[*] Object dir:      ${OBJ_DIR}     "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "
	@echo "[*] Dependencies:    ${DEPENDENCIES}"

