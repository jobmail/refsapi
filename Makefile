CSSDK = include/cssdk
METAMOD = include/metamod

NAME = refsapi

COMPILER = g++

OBJECTS = *.cpp include/cssdk/public/interface.cpp 

LINK = -m32 -static-libgcc -static-libstdc++ -lstdc++fs -pthread $(mysql_config --cflags) $(mysql_config --libs)

#-l:libm.a -l:libc.a
#-s -Llib/linux32 -static-libgcc -static-libstdc++
#-ldl -m32 -s -Llib/linux32 -static-libgcc

OPT_FLAGS = -O3 -msse3 -flto=auto -fno-strict-aliasing -Wno-uninitialized -funroll-loops -fomit-frame-pointer -fpermissive -pthread

#-pipe
#-O3 -msse3 -flto=auto -funroll-loops -fomit-frame-pointer -fno-stack-protector -fPIC -mtune=generic -fno-sized-deallocation -Wno-strict-aliasing

INCLUDE = -I. -I$(CSSDK)/common -I$(CSSDK)/dlls -I$(CSSDK)/engine \
        -I$(CSSDK)/game_shared -I$(CSSDK)/pm_shared -I$(CSSDK)/public -I$(METAMOD) -Iinclude \
        -Iinclude/amxmodx -Iinclude/amxmodx/public -Iinclude/amxmodx/amtl -Iinclude/amxmodx/third_party/hashing -Icommon

BIN_DIR = Release
CFLAGS = $(OPT_FLAGS) -Wno-unused-result

CFLAGS += -m32 -fvisibility=hidden -shared -std=gnu++17 

#-D_GLIBCXX_USE_CXX11_ABI=0
#-fabi-version=11 -fabi-compat-version=11 -Wabi=11 
#-fabi-version=11 -fabi-compat-version=11 -Wabi=11 -fno-exceptions 
#-g0 -DNDEBUG -Dlinux -D__linux__ -std=gnu++14 -shared -m32 -D_GLIBCXX_USE_CXX11_ABI=0 -DHAVE_STRONG_TYPEDEF

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
