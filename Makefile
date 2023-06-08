# This is the makefile for the RCEU 2023 project "Host-Based CD-ROM Data Recovery"

SRC_DIR := src
OBJ_DIR := obj
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES))
LDFLAGS := "-lfec"

readcd: $(OBJ_DIR)/readcd.o
	g++ $^ -o readcd

$(OBJ_DIR)/readcd.o: $(OBJ_FILES)
	g++ $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	g++ $(LDFLAGS) -c -o $@ $<
	
clean:
	rm -f $(OBJ_FILES)
	rm -f readcd
