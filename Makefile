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
       -lpspnet_adhoc -lpspnet_adhocctl -lpspnet_resolver \
       -lpsppower -lpspaudio -lpspwlan

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = ABISSO

# La prima regola diventa il goal di default: forziamo "all" (definito in
# build.mak) cosi' che "make" costruisca l'ELF e l'EBOOT.PBP, non solo l'atlas.
.DEFAULT_GOAL := all

# Atlas degli asset originali (assets/atlas.rle) incorporato nell'EBOOT:
# genera i simboli atlas_rle_start / atlas_rle_end / atlas_rle_size.
# NOTA: bin2o assembla con psp-as senza i flag di architettura e produce un
# oggetto mips:5900/EABI64 incompatibile; si usa bin2s + psp-gcc che compila
# con l'architettura corretta (allegrex/EABI32) come i sorgenti C.
atlas_rle.s: assets/atlas.rle
	bin2s assets/atlas.rle atlas_rle.s atlas_rle

atlas_rle.o: atlas_rle.s
	psp-gcc $(ASFLAGS) -c -o $@ $<

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
