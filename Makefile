CSSDK = include/cssdk
METAMOD = include/metamod

NAME = refsapi

COMPILER = g++

OBJECTS = *.cpp

LINK = -ldl

OPT_FLAGS = -O3 -msse3 -flto -funroll-loops -fomit-frame-pointer -fno-stack-protector -fPIC -mtune=generic -fno-sized-deallocation -Wno-strict-aliasing

INCLUDE = -I. -I$(CSSDK)/common -I$(CSSDK)/dlls -I$(CSSDK)/engine \
		-I$(CSSDK)/game_shared -I$(CSSDK)/pm_shared -I$(CSSDK)/public -I$(METAMOD) -Iinclude -Icommon

BIN_DIR = Release
CFLAGS = $(OPT_FLAGS) -Wno-unused-result

CFLAGS += -g -DNDEBUG -Dlinux -D__linux__ -D__USE_GNU -std=gnu++14 -shared -m32

OBJ_LINUX := $(OBJECTS:%.c=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.c
	$(COMPILER) $(INCLUDE) $(CFLAGS) -o $@ -c $<

all:
	mkdir -p $(BIN_DIR)

	$(MAKE) $(NAME) && strip -x $(BIN_DIR)/$(NAME)_amxx_i386.so

$(NAME): $(OBJ_LINUX)
	$(COMPILER) $(INCLUDE) $(CFLAGS) $(OBJ_LINUX) $(LINK) -o$(BIN_DIR)/$(NAME)_amxx_i386.so

check:
	cppcheck $(INCLUDE) --quiet --max-configs=100 -D__linux__ -DNDEBUG .

debug:
	$(MAKE) all DEBUG=false

default: all

clean:
	rm -rf Release/*.o
	rm -rf Release/$(NAME)_amxx_i386.so