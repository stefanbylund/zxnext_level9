################################################################################
# Stefan Bylund 2021
#
# Makefile for compiling the Level 9 interpreter for ZX Spectrum Next using the
# Z88DK SDCC compiler.
#
# The Level 9 interpreter is configured in the configure.m4 file, which is used
# by this makefile to generate the configuration files (zconfig.h, zconfig.inc,
# zconfig.m4 and zpragma.inc) and the list of source files to compile
# (zproject.lst).
#
# Make rules:
# clean: Remove all generated files.
# all: Compile all source code.
# debug: Compile with debug info.
# config: Only generate configuration files.
# compilation: Build the Level 9 games compilation for Spectrum Next.
# compilation_boot: Build the Level 9 auto-bootable games compilation for Spectrum Next.
#
# Optional make command-line options:
# CONFIG: List of defines passed to configure.m4 to override its configuration.
# BUILD_OPT: Set to true to build with high compiler optimizations.
#
# Example:
# make clean all CONFIG="-DUSE_TIMEX_HIRES=1 -DUSE_GFX=1 -DUSE_MOUSE=1" BUILD_OPT=true
################################################################################

CC := zcc

M4 := m4

CP := cp -r

MV := mv

MKDIR := mkdir -p

RM := rm -rf

CAT := cat

ZIP := zip -r -q

UNZIP := unzip -q

ifeq ($(BUILD_OPT), true)
CFLAGS_OPT := -SO3 --max-allocs-per-node200000
endif

CFLAGS := +zxn -vn -clib=sdcc_iy -Cm"-I src" $(CFLAGS_OPT)

CFLAGS_DEBUG := --list --c-code-in-asm

LDFLAGS := -m -startup=31 -pragma-include:src/zpragma.inc -subtype=nex -Cz"--main-fence 0xFDFD" -create-app

CFG := src/zconfig.h src/zconfig.inc src/zconfig.m4 src/zpragma.inc zproject.lst

DERIVED := zcc_opt.def zcc_proj.lst

INCLUDES := $(wildcard src/*.h)

SOURCES := $(shell [ -f zproject.lst ] && $(CAT) zproject.lst)

OBJECTS := $(patsubst %.asm.m4,%.o,$(patsubst %.asm,%.o,$(patsubst %.c,%.o,$(addprefix obj/,$(notdir $(SOURCES))))))

BINARY := bin/level9.nex

GAMES := ../level9_games

SPUI := compilation/resources/SPUI-0.4.2.zip

# This makefile runs in two steps for configuring and then building the project.
# The first step processes the configure.m4 file to generate the configuration
# files (zconfig.h, zconfig.inc, zconfig.m4 and zpragma.inc) and the project
# file (zproject.lst) containing the list of source files to compile.
# The second step will create the OBJECTS variable from the zproject.lst file
# and compile all source code. It is necessary to have two separate steps using
# a recursive make to guarantee that the OBJECTS variable will be correct when
# rebuilding after a configuration change.

.PHONY: all
all: config
	$(MKDIR) obj bin
	$(MAKE) $(BINARY)

.PHONY: debug
debug: config
	$(MKDIR) obj bin
	$(MAKE) DEBUG="$(CFLAGS_DEBUG)" $(BINARY)

.PHONY: config
config: $(CFG)

src/zconfig.h: configure.m4
	$(M4) -DTARGET=1 $(CONFIG) $< > $@

src/zconfig.inc: configure.m4
	$(M4) -DTARGET=2 $(CONFIG) $< > $@

src/zconfig.m4: configure.m4
	$(M4) -DTARGET=3 $(CONFIG) $< > $@

src/zpragma.inc: configure.m4
	$(M4) -DTARGET=4 $(CONFIG) $< > $@

zproject.lst: configure.m4
	$(M4) -DTARGET=5 $(CONFIG) $< > $@

$(BINARY): $(OBJECTS) src/zconfig.m4 src/zpragma.inc
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(basename $@)

obj/%.o: src/%.c $(INCLUDES)
	$(CC) $(CFLAGS) $(DEBUG) -c -o $@ $<

obj/%.o: src/%.asm.m4 src/zconfig.inc src/zconfig.m4
	$(CC) $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.asm src/zconfig.inc
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: compilation
compilation:
	$(RM) tmp
	$(MKDIR) tmp/level9
	$(MAKE) clean all CONFIG="-DUSE_TIMEX_HIRES=0 -DUSE_GFX=1 -DUSE_MOUSE=1" BUILD_OPT=true
	$(CP) $(BINARY) tmp/level9_256.nex
	$(MAKE) clean all CONFIG="-DUSE_TIMEX_HIRES=1 -DUSE_GFX=1 -DUSE_MOUSE=1" BUILD_OPT=true
	$(CP) $(BINARY) tmp/level9_512.nex
	$(MAKE) game GAME=adventure-quest
	$(MAKE) game GAME=archers
	$(MAKE) game GAME=colossal-adventure
	$(MAKE) game GAME=dungeon-adventure
	$(MAKE) game GAME=emerald-isle
	$(MAKE) game GAME=erik-the-viking
	$(MAKE) game GAME=gnome-ranger
	$(MAKE) game GAME=growing-pains-of-adrian-mole
	$(MAKE) game GAME=ingrids-back
	$(MAKE) game GAME=knight-orc
	$(MAKE) game GAME=lancelot
	$(MAKE) game GAME=return-to-eden
	$(MAKE) game GAME=scapeghost
	$(MAKE) game GAME=secret-diary-of-adrian-mole
	$(MAKE) game GAME=snowball
	$(MAKE) game GAME=time-and-magik
	$(MAKE) game GAME=worm-in-paradise
	$(CP) compilation/resources/run.gde tmp/level9
	$(CP) compilation/resources/run.bas tmp/level9
	$(CP) compilation/resources/menu.ini tmp/level9
	$(MV) tmp/level9 tmp/level9.run
	$(RM) compilation/level9.zip
	cd tmp; $(ZIP) ../compilation/level9.zip level9.run
	$(RM) tmp

.PHONY: compilation_boot
compilation_boot:
	$(RM) tmp
	$(MKDIR) tmp/boot/dot/
	$(MKDIR) tmp/boot/nextzxos/
	$(UNZIP) $(SPUI) -d tmp
	$(CP) tmp/SPUI*/SPUI/dot/SPUI tmp/boot/dot/
	$(CP) compilation/resources/autoexec.bas tmp/boot/nextzxos/
	$(UNZIP) compilation/level9.zip -d tmp/boot
	$(RM) compilation/level9-boot.zip
	cd tmp/boot; $(ZIP) ../../compilation/level9-boot.zip .
	$(RM) tmp

.PHONY: game
game:
	$(MKDIR) tmp/level9/$(GAME)
	$(CP) $(GAMES)/$(GAME)/gamedat*.dat tmp/level9/$(GAME)
	$(CP) $(GAMES)/$(GAME)/gamedata.txt tmp/level9/$(GAME)
	$(CP) $(GAMES)/$(GAME)/gfx tmp/level9/$(GAME)
ifneq ("$(wildcard $(GAMES)/$(GAME)/script*.txt)", "")
	$(CP) $(GAMES)/$(GAME)/script*.txt tmp/level9/$(GAME)
endif
	$(CP) tmp/level9_256.nex tmp/level9/$(GAME)
	$(CP) tmp/level9_512.nex tmp/level9/$(GAME)
	$(CP) scripts/run_cspect.bat tmp/level9/$(GAME)
	$(CP) scripts/run_zesarux.bat tmp/level9/$(GAME)

.PHONY: clean
clean:
	$(RM) $(CFG) $(DERIVED) obj bin src/*.lis
