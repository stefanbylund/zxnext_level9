################################################################################
# Stefan Bylund 2021
#
# Makefile for compiling the Level 9 graphics conversion tool.
################################################################################

MKDIR := mkdir -p

RM := rm -rf

all:
	$(MKDIR) bin
	gcc -O2 -Wall -o bin/convert_gfx src/convert_gfx.c src/level9_gfx.c -mconsole -mwindows

clean:
	$(RM) bin .vs x64 convert_gfx.vcxproj.user
