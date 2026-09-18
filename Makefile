TARGET = abisso
OBJS = src/main.o src/game.o src/dungeon.o src/data.o src/gfx.o src/ui.o src/audio.o \
       src/net.o src/atlas_rects.o atlas_rle.o

INCDIR = src
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgum -lpspgu -lm \
       -lpspnet -lpspnet_inet -lpspnet_apctl \
       -lpspnet_adhoc -lpspnet_adhocctl -lpspnet_resolver

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ABISSO

# La prima regola diventa il goal di default: forziamo "all" (definito in
# build.mak) cosi' che "make" costruisca l'ELF e l'EBOOT.PBP, non solo l'atlas.
.DEFAULT_GOAL := all

# Atlas degli asset originali (assets/atlas.rle) incorporato nell'EBOOT:
# genera i simboli atlas_rle_start / atlas_rle_end / atlas_rle_size
atlas_rle.o: assets/atlas.rle
	bin2o assets/atlas.rle atlas_rle.o atlas_rle

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
