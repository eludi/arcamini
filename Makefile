CC = gcc
CFLAGS = -Wall -Wpedantic -Wno-overlength-strings -O3 -DNDEBUG

SDL						= ../SDL2
INCDIR					= -I$(SDL)/include -D_REENTRANT -Iexternal -Iexternal/arcajs

ifeq ($(OS),Windows_NT)
	OS					= win32
	ARCH				= $(OS)-x64
else
	OS					= $(shell uname -s)
	MACHINE				= $(shell uname -m)
	ARCH				= $(OS)_$(MACHINE)
endif

ifeq ($(OS),Linux)
	LIBS				= -rdynamic -Lexternal/$(ARCH) -larcajs
	SHLIBS				= -rdynamic -Lexternal/$(ARCH) -larcajs
	ifeq ($(ARCH),Linux_armv7l)
		LIBS			+= -L$(SDL)/lib/$(ARCH) -Wl,-rpath,$(SDL)/lib/$(ARCH) -Wl,--enable-new-dtags -lSDL2 -Wl,--no-undefined -Wl,-rpath,/opt/vc/lib -L/opt/vc/lib -lbcm_host -lpthread -lrt -ldl -lm
		SHLIBS			+= -L$(SDL)/lib/$(ARCH) -Wl,-rpath,$(SDL)/lib/$(ARCH) -Wl,--enable-new-dtags -lSDL2 -Wl,--no-undefined -Wl,-rpath,/opt/vc/lib -L/opt/vc/lib -lbcm_host -lpthread -lrt -ldl -lm
	else
		LIBS			+= -L$(SDL)/lib/$(ARCH) -Wl,-rpath,$(SDL)/lib/$(ARCH) -Wl,--enable-new-dtags -lSDL2 -Wl,--no-undefined -lpthread -ldl -lm
		SHLIBS			+= -lSDL2 -Wl,--no-undefined -lpthread -ldl -lm -s
	endif
	CFLAGS				+= -fPIC -no-pie
	EXESUFFIX			= .$(MACHINE)
	DLLPREFIX			= lib
	DLLSUFFIX			= .$(MACHINE).so
	RM = rm -f
	SEP = /
	CP = cp
else
	ifeq ($(OS),Darwin) # MacOS
		EXESUFFIX = .app
		DLLPREFIX = lib
		DLLSUFFIX = .dylib
	else # windows, MinGW
		LIBS			= -Lexternal/$(ARCH) -larcajs \
							-L$(SDL)/lib/$(ARCH) -static -lmingw32 -lSDL2main -lSDL2 -Wl,--no-undefined -lm \
							-ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 \
							-lshell32 -lsetupapi -lversion -luuid -lwininet -lwsock32 -static-libgcc -mwindows
		SHLIBS			= $(LIBS)
		EXESUFFIX		= .exe
		DLLPREFIX		=
		DLLSUFFIX		= .dll
		RM = del
		SEP = \\#
		CP = copy
	endif
endif

SRCPY = arcapy.c external/pocketpy.c bindings_arcapy.c arcamini.c
OBJPY = $(SRCPY:.c=.o)
EXEPY = arcapy$(EXESUFFIX)

SRCQJS = arcaqjs.c bindings_arcaqjs.c arcamini.c
OBJQJS = $(SRCQJS:.c=.o)
EXEQJS = arcaqjs$(EXESUFFIX)

SRCLUA = arcalua.c bindings_arcalua.c arcamini.c
OBJLUA = $(SRCLUA:.c=.o)
EXELUA = arcalua$(EXESUFFIX)

SRCLIB = libarcamini.c arcamini.c
LIB = $(DLLPREFIX)arcamini$(DLLSUFFIX)

all: $(EXEPY) $(EXEQJS) $(EXELUA) $(LIB)

# executable link rules:
$(EXEPY) : $(OBJPY)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ -s
$(EXEQJS) : $(OBJQJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -lquickjs -o $@ -s
$(EXELUA) : $(OBJLUA)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@ -s
$(LIB) : $(SRCLIB)
	$(CC) $(INCDIR) $(CFLAGS) -shared -o $@ $^ $(SHLIBS) -s

arcapy.o: arcapy.c bindings.h pkpy_debug.h arcamini.h
arcapy.o: CFLAGS += -DPK_IS_PUBLIC_INCLUDE
arcaqjs.o: arcaqjs.c arcamini.h bindings.h qjs_debug.h
arcalua.o: arcalua.c external/minilua.h bindings.h arcamini.h arcalua_debug.h
arcamini.o: arcamini.c arcamini.h
external/pocketpy.o: external/pocketpy.c external/pocketpy.h
bindings_arcalua.o: bindings.h bindings_arcalua.c external/minilua.h arcamini.h
bindings_arcapy.o: bindings.h bindings_arcapy.c external/pocketpy.h arcamini.h
bindings_arcapy.o: CFLAGS += -DPK_IS_PUBLIC_INCLUDE
bindings_arcaqjs.o: bindings.h bindings_arcaqjs.c arcamini.h

# generic rules and targets:
.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $< -o $@

clean:
	$(RM) external$(SEP)*.o *.o $(EXEPY) $(EXEQJS) $(EXELUA) $(LIB)

pub: all
	$(CP) $(EXEPY) pub/$(EXEPY).$(MACHINE)
	$(CP) $(EXEQJS) pub/$(EXEQJS).$(MACHINE)
	$(CP) $(EXELUA) pub/$(EXELUA).$(MACHINE)
	$(CP) $(LIB) pub/$(LIB)
