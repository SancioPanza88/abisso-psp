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

# Atlas degli asset originali (assets/atlas.rle) incorporato nell'EBOOT.
# Simboli identici a quelli di bin2o (verificati sul sorgente pspsdk):
# atlas_rle_start / atlas_rle_end / atlas_rle_size.
# bin2o e' scartato perche' assembla con psp-as senza flag di architettura
# (oggetto mips:5900/EABI64 incompatibile); qui l'assembly e' compilato con
# psp-gcc, quindi allegrex/EABI32 come i sorgenti C.
atlas_rle.s: assets/atlas.rle
	printf '.data\n.balign 16\n.globl atlas_rle_start\natlas_rle_start:\n.incbin "assets/atlas.rle"\n.globl atlas_rle_end\natlas_rle_end:\n.globl atlas_rle_size\natlas_rle_size: .word atlas_rle_end - atlas_rle_start\n' > $@

atlas_rle.o: atlas_rle.s
	psp-gcc $(ASFLAGS) -c -o $@ $<

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
