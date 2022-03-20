################################################################################
# Stefan Bylund 2021
#
# Makefile for compiling the Level 9 bitmap conversion tool.
################################################################################

MKDIR := mkdir -p

RM := rm -rf

all:
	$(MKDIR) bin
	gcc -O2 -Wall -o bin/convert_bitmap src/convert_bitmap.c

clean:
	$(RM) bin
