TARGET = abisso
OBJS = src/main.o src/game.o src/dungeon.o src/data.o src/gfx.o src/ui.o src/audio.o \
       src/atlas_rects.o atlas_rle.o

INCDIR = src
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgum -lpspgu -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ABISSO

# Atlas degli asset originali (assets/atlas.rle) incorporato nell'EBOOT:
# genera i simboli atlas_rle / atlas_rle_end / atlas_rle_size
atlas_rle.o: assets/atlas.rle
	bin2o -i $< -o $@ -n atlas_rle

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
