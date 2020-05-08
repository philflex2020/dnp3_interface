PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

$(info PROJECT_ROOT="$(PROJECT_ROOT)")
$(info MAKEFILE_LIST="$(MAKEFILE_LIST)")

BINS = dnp3_outstation dnp3_master dnp_outstation_pw

BIN_DIR = /usr/local/bin/
INCLUDES_DIR = /usr/local/include/
SINCLUDES_DIR= /usr/include/

LIBS_DIR = /usr/local/lib/
SLIBS_DIR = /usr/local/lib64/

INCLUDES_BUILD = -Iinclude/
INCLUDES = -I$(INCLUDES_DIR) -I$(SINCLUDES_DIR)


CPPFLAGS += -std=c++11 
CPPFLAGS += -Wall -pedantic -Werror

# BUILD_MODE = debug
ifeq ($(BUILD_MODE),debug)
	CPPFLAGS += -g -DFPS_DEBUG_MODE
else ifeq ($(BUILD_MODE),test)
	CPPFLAGS += -DFPS_TEST_MODE
	LIBS += -lgtest -lgmock
else
	BUILD_MODE=release
	CPPFLAGS += -O2
endif

BUILD_DIR = build/$(BUILD_MODE)/
OBJ_DIR = build/$(BUILD_MODE)_obj/
LIST = $(addprefix $(BUILD_DIR), $(BINS))
LIBS = -L$(OBJ_DIR) -L$(LIBS_DIR) -L$(SLIBS_DIR) -Wl,-rpath,$(LIBS_DIR) -lstdc++ -lcjson -lfims -lopendnp3 -lopenpal -lasiopal -lasiodnp3 -lpthread

all:	build $(LIST)

$(OBJ_DIR)libdmap.so:	$(OBJ_DIR)libdmap.o 
	$(CXX) -shared -o $@ $^ $(INCLUDES) $(LIBS)

$(OBJ_DIR)libdmap.o:	$(PROJECT_ROOT)src/dnp3_utils.cpp $(PROJECT_ROOT)include/dnp3_utils.h
	$(CXX) -c $(CPPFLAGS) -fpic -o $@ $< $(INCLUDES_BUILD)

$(BUILD_DIR)dnp3_outstation: $(OBJ_DIR)dnp3_outstation.o
	$(CXX) -o $@ $^ $(INCLUDES) $(LIBS)

$(BUILD_DIR)dnp3_outstation_pw: $(OBJ_DIR)dnp3_outstation_pw.o $(OBJ_DIR)libdmap.so
	$(CXX) -o $@ $^ $(INCLUDES) $(LIBS) -ldmap

$(BUILD_DIR)dnp3_master: $(OBJ_DIR)dnp3_master.o $(OBJ_DIR)newSOEHandler.o $(OBJ_DIR)libdmap.so
	$(CXX) -o $@ $^ $(INCLUDES) $(LIBS) -ldmap

$(OBJ_DIR)%.o: $(PROJECT_ROOT)src/%.cpp
	$(CXX) -c $(CPPFLAGS) -o $@ $< $(INCLUDES_BUILD)

.PHONY: build
build:
	mkdir -p $(BUILD_DIR) $(OBJ_DIR)

.PHONY: clean
clean:
	rm -fr build

.PHONY: uninstall
uninstall:
	rm -rf $(BIN_DIR)dnp3_interface

.PHONY: install
install:
	mkdir -p $(BIN_DIR)dnp3_interface
	cp $(BUILD_DIR)dnp3_outstation $(BIN_DIR)dnp3_interface
