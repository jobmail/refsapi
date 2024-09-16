CSSDK = include/cssdk
METAMOD = include/metamod

NAME = refsapi

COMPILER = g++
#clang++
#g++

OBJECTS = *.cpp include/cssdk/public/interface.cpp 

LINK = -L./lib/ -L/usr/lib/i386-linux-gnu/ \
	-s -m32 -static-libgcc -static-libstdc++ -lstdc++fs -lpthread -l:libmariadb.a
#-static-libasan -static-libubsan
#-l:libssl.a -l:libcrypto.a

#-lm -lrt -Wl,--whole-archive -lmariadbd -lmariadb -lpcre -lz -llz4 -laio -ldl -lsnappy -lcrypt -Wl,--no-whole-archive

#-lp11-kit -lgnutls -lidn2 -lunistring -ltasn1 -lnettle -lhogweed -lgmp -lc -ldl -lffi \
-lpcre -lz -llz4 
#-l:libmariadbd.a -l:libmariadb.a
# -l:libz.a -l:liblz4.a -l:libpcre.a -l:libsnappy.a -l:libcrypt.a -l:libaio.a #-l:libmariadb.a -l:libpcre.a #-DWITH_SSL=OFF #-lmariadb
#
#
#-L./lib/ 
#-static -lpthread -lmariadb -lssl -lcrypto -DWITH_SSL=OFF

#-DWITH_SSL=OFF \	
#-l:libmariadb.a -l:libssl.a -l:libcrypto.a \
	-l:libgnutls.a -l:libz.a -l:libidn2.a -l:libunistring.a -l:libtasn1.a -l:libnettle.a -l:libhogweed.a -l:libgmp.a -l:libffi.a #-lmariadb 
	
#-static-libgcc -static-libstdc++ -lstdc++fs
#-l:libmariadb.a #-l:libssl.a -l:libcrypto.a

#-static-libgcc -static-libstdc++ -lstdc++fs -lpthread -l:libmariadb.a -l:libssl.a -l:libcrypto.a
	
# -l:libmariadb.a -l:libssl.a -l:libcrypto.a
#-static-libgcc -static-libstdc++
#	-Wl,--whole-archive -l:libmariadb.a -l:libz.a -l:libgnutls.a -l:libidn2.a \
#	-l:libunistring.a -l:libtasn1.a -l:libnettle.a -l:libhogweed.a -l:libgmp.a -l:libffi.a -l:libpcre.a -Wl,--no-whole-archive
#LINK = -L/lib/ -L/usr/lib/i386-linux-gnu/ -L/lib/i386-linux-gnu/ \
#    -m32 -static-libgcc -static-libstdc++ -lstdc++fs -lpthread -Wl,--whole-archive -l:libmariadb.a -l:libz.a -l:libssl.a -l:libcrypto.a -l:libzstd.a -Wl,--no-whole-archive
# -lz -lgnutls -lanl -lp11-kit -lidn2 -lunistring -lasan -lnettle -lhogweed -lgmp -lffi
#-static-libgcc -static-libstdc++ -lstdc++fs 
#-Wl,--as-needed 
#-static-libgcc -static-libstdc++
#	-l:libmariadb.a -l:libssl.a -l:libgnutls.a -l:libcrypto.a \
#	-l:librt.a -l:libz.a -l:libanl.a -l:libnettle.a \
#	-l:libidn2.a -l:libunistring.a -l:libffi.a -l:libhogweed.a \
#	-l:libgmp.a -l:libpcre.a -lpthread -L/lib/i386-linux-gnu/ -L/usr/lib/i386-linux-gnu/ -L/lib/
#



# -lpthread -L/usr/lib/i386-linux-gnu/ -L/libs/
# 
#-l:libmariadb.a -l:libssl.a -l:libcrypto.a -l:libm.a  -l:librt.a -l:libz.a -lpthread -L/usr/lib/i386-linux-gnu/ -L/lib/
#-l:libmariadb.a -l:libm.a -l:librt.a -l:libz.a
#-lpthread -lm -lrt
#-L/usr/lib/i386-linux-gnu/
#-l:libmysqlclient_r.a -l:libmariadbd.a -l:libmariadbclient.a -l:libpthread.a
#-lmariadb -lpthread 
#-lmariadb
#-L/usr/lib/i386-linux-gnu/ -lmariadb

#-l:libm.a -l:libc.a
#-s -Llib/linux32 -static-libgcc -static-libstdc++
#-ldl -m32 -s -Llib/linux32 -static-libgcc

OPT_FLAGS = -O3 -msse3 -fno-strict-aliasing -Wno-uninitialized -fpermissive -shared -flto -fPIC -pipe -pthread
# -funroll-loops -fomit-frame-pointer 
# -fno-rtti
#-fno-split-stack
# -flto -fPIC
#-O3 -msse3 -flto=auto -funroll-loops -fomit-frame-pointer -fno-stack-protector -fPIC -mtune=generic -fno-sized-deallocation -Wno-strict-aliasing

INCLUDE = -I. -I$(CSSDK)/common -I$(CSSDK)/dlls -I$(CSSDK)/engine \
        -I$(CSSDK)/game_shared -I$(CSSDK)/pm_shared -I$(CSSDK)/public -I$(METAMOD) -Iinclude -Iinclude/mariadb \
        -Iinclude/amxmodx -Iinclude/amxmodx/public -Iinclude/amxmodx/amtl -Iinclude/amxmodx/third_party/hashing -Icommon
		
#-I/usr/include/mariadb

BIN_DIR = Release
CFLAGS = $(OPT_FLAGS) -Wno-unused-result

CFLAGS += -m32 -fvisibility=hidden -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -s -fno-stack-protector
#-DWITHOUT_SQL
#-g3
#-fsanitize=undefined
#-fsanitize=address
#-g3 -ggdb
#-g -DWITHOUT_SQL
#-Wl,--as-needed
#-D_GLIBCXX_USE_CXX11_ABI=0 
#-Wl,--as-needed
#-D_GLIBCXX_USE_CXX11_ABI=0 -Wl,--as-needed
#-static
#-Wl,--allow-multiple-definition
#-Wl,--as-needed -Wl,--whole-archive
#-D_GLIBCXX_USE_CXX11_ABI=0
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
