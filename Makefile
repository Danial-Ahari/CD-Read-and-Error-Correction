# This is the makefile for the RCEU 2023 project "Host-Based CD-ROM Data Recovery"

SRC_DIR := src
INCL_DIR := include
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
DEPS := $(wildcard $(INCL_DIR)/*.hpp)

readcd: $(SRC_FILES) $(DEPS)
	g++ $(SRC_FILES) $(DEPS) -o $@
	
clean:
	rm -f readcd scramout.bin unscramout.bin
