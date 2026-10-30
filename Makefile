CSSDK = include/cssdk
METAMOD = include/metamod
NAME = refsapi
COMPILER = g++
OBJECTS = *.cpp include/cssdk/public/interface.cpp 
LINK = -L./lib/ -L/usr/lib/i386-linux-gnu/ \
	-s -m32 -ldl -lm -static-libgcc -static-libstdc++ -lstdc++fs -lpthread -l:libmariadb.a -l:libssl.a -l:libcrypto.a

OPT_FLAGS = -O3 -mmmx -msse -msse2 -msse3 -msse4.2 -fno-strict-aliasing -Wno-parentheses -Wno-int-to-pointer-cast -Wno-switch -Wno-uninitialized -fpermissive -shared -pipe -pthread

INCLUDE = -I. -I$(CSSDK)/ -I$(CSSDK)/common/ -I$(CSSDK)/dlls/ -I$(CSSDK)/engine/ \
        -I$(CSSDK)/game_shared/ -I$(CSSDK)/pm_shared/ -I$(CSSDK)/public/ -I$(CSSDK)/public/regamedll/ -I$(METAMOD) -Iinclude -Iinclude/mariadb -I/usr/lib/gcc/i686-linux-gnu/9/include/ \
        -Iinclude/amxmodx/ -Iinclude/amxmodx/public/ -Iinclude/amxmodx/amtl/ -Iinclude/amxmodx/third_party/hashing/ -Icommon/
		
#-I/usr/include/mariadb

BIN_DIR = Release
CFLAGS = $(OPT_FLAGS) -Wno-unused-result

CFLAGS += -m32 -s -fvisibility=hidden -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -g0 \
	-fno-stack-protector -ffunction-sections -fdata-sections -fnon-call-exceptions -flto=auto -fPIC -DNDEBUG

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
