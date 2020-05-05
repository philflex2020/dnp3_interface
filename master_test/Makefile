PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

$(info PROJECT_ROOT="$(PROJECT_ROOT)")
$(info MAKEFILE_LIST="$(MAKEFILE_LIST)")

BINS = dnp3_outstation dnp3_master

BIN_DIR = /usr/local/bin/
INCLUDES_DIR = /usr/local/include/
SINCLUDES_DIR= /usr/include/

LIBS_DIR = /usr/local/lib/
SLIBS_DIR = /usr/lib64/

INCLUDES_BUILD = -Iinclude/
INCLUDES = -I$(INCLUDES_DIR) -I$(SINCLUDES_DIR)
LIBS = -L$(LIBS_DIR) -L$(SLIBS_DIR) -Wl,-rpath,$(LIBS_DIR) -lcjson -lfims -lopendnp3 -lopenpal -lasiodnp3 -lpthread

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

all:	build $(LIST)

$(BUILD_DIR)dnp3_outstation: $(OBJ_DIR)dnp3_outstation.o
	$(CXX) -o $@ $^ $(INCLUDES) $(LIBS)

$(BUILD_DIR)dnp3_master: $(OBJ_DIR)dnp3_master.o $(OBJ_DIR)dnp3_utils.o
	$(CXX) -o $@ $^ $(INCLUDES) $(LIBS)

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
