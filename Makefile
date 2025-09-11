CC     = gcc
CFLAGS = -Wall -Wpedantic -Wno-overlength-strings -O3 -DNDEBUG

SDL = ../SDL2

ifeq ($(OS),Windows_NT)
  OS            = win32
  ARCH          = $(OS)-x64
else
  OS            = $(shell uname -s)
  ARCH          = $(OS)_$(shell uname -m)
endif

ifeq ($(OS),Linux)
  INCDIR        = -I$(SDL)/include -D_REENTRANT -Iexternal -Iexternal/arcajs
  LIBS          = -rdynamic -Lexternal/$(ARCH) -larcajs
  ifeq ($(ARCH),Linux_armv7l)
    LIBS       += -L$(SDL)/lib/$(ARCH) -Wl,-rpath,$(SDL)/lib/$(ARCH) -Wl,--enable-new-dtags -lSDL2 -Wl,--no-undefined -Wl,-rpath,/opt/vc/lib -L/opt/vc/lib -lbcm_host -lpthread -lrt -ldl -lm
  else
    LIBS       +=  -L$(SDL)/lib/$(ARCH) -Wl,-rpath,$(SDL)/lib/$(ARCH) -Wl,--enable-new-dtags -lSDL2 -Wl,--no-undefined -lpthread -ldl -lm
  endif
  GLLIBS        = -lGL
  CFLAGS       += -fPIC -no-pie
  EXESUFFIX     =
  RM = rm -f
  SEP = /
else
  ifeq ($(OS),Darwin) # MacOS
    EXESUFFIX = .app
  else # windows, MinGW
    INCDIR        = -I$(SDL)/include -Iexternal -Iexternal/arcajs
    LIBS          = -Lexternal/$(ARCH) -larcajs \
                     -L$(SDL)/lib/$(ARCH) -static -lmingw32 -lSDL2main -lSDL2 -Wl,--no-undefined -lm \
                    -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 \
                    -lshell32 -lsetupapi -lversion -luuid -lwininet -lwsock32 -static-libgcc -mconsole
    EXESUFFIX     = .exe
    RM = del /s
    SEP = \\#
  endif
endif

SRCPY = arcapy.c external/pocketpy.c bindings_arcapy.c arcamini.c
OBJPY = $(SRCPY:.c=.o)
EXEPY = arcapy$(EXESUFFIX)

SRCQJS = arcaqjs.c bindings_arcaqjs.c arcamini.c
OBJQJS = $(SRCQJS:.c=.o)
EXEQJS = arcaqjs$(EXESUFFIX)

SRCLUA = arcalua.c external/minilua.h bindings_arcalua.c arcamini.c
OBJLUA = $(SRCLUA:.c=.o)
EXELUA = arcalua$(EXESUFFIX)

all: $(EXEPY) $(EXEQJS) $(EXELUA)

# executable link rules:
$(EXEPY) : $(OBJPY)
	$(CC) $(CFLAGS) $^ $(LIB) $(LIBS) -o $@ -s
$(EXEQJS) : $(OBJQJS)
	$(CC) $(CFLAGS) $^ $(LIB) $(LIBS) -lquickjs -o $@ -s
$(EXELUA) : $(OBJLUA)
	$(CC) $(CFLAGS) $^ $(LIB) $(LIBS) -o $@ -s

arcapy.o: arcapy.c bindings.h pkpy_debug.h arcamini.h
arcaqjs.o: arcaqjs.c arcamini.h bindings.h qjs_debug.h
arcalua.o: arcalua.c external/minilua.h bindings.h arcamini.h arcalua_debug.h
arcamini.o: arcamini.c arcamini.h
external/pocketpy.o: external/pocketpy.c external/pocketpy.h
bindings_arcalua.o: bindings.h bindings_arcalua.c external/minilua.h arcamini.h
bindings_arcapy.o: bindings.h bindings_arcapy.c external/pocketpy.h arcamini.h
bindings_arcaqjs.o: bindings.h bindings_arcaqjs.c arcamini.h

# generic rules and targets:
.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $< -o $@

clean:
	$(RM) $(OBJPY) $(OBJQJS)  $(OBJLUA) $(EXEPY) $(EXEQJS) $(EXELUA)
