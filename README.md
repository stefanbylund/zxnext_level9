# Level 9 Interpreter

This is a port of the open source Level 9 interpreter *Level9* to ZX Spectrum
Next. The original Level 9 interpreter was developed and used by the British
company [Level 9 Computing](https://en.wikipedia.org/wiki/Level_9_Computing) to
create text adventure games (interactive fiction) that would run on a variety of
8/16/32-bit computers. Level 9 compiled their text adventure games to a custom
bytecode language called A-code that was executed by an interpreter / virtual
machine called the A-machine. This was conceptually similar to Infocom's Z-code
and Z-machine.

*Level9* is a reverse-engineered Level 9 interpreter originally developed by
Glen Summers in portable C code. David Kinder, Alan Staniforth, Simon Baldwin,
Dieter Baron and Andreas Scherrer have also contributed to the development of
Level9. The Level9 interpreter has been ported to many different systems. More
information about Level9 and its source code is available on
[GitHub](https://github.com/DavidKinder/Level9). In the following text, the
terms *Level9* interpreter and Level 9 interpreter are used interchangeably.

The main challenge when porting the Level 9 interpreter to Spectrum Next is that
it assumes a flat memory model where all memory is directly addressable and the
loaded game story file is being kept in main memory. The Spectrum Next and its
Z80 CPU is an 8-bit computer which can address only 64 KB of memory. Although
the Spectrum Next has 1 MB RAM where 768 KB is available to programs, this
memory has to be switched in to the 64 KB address space in 8 or 16 KB banks.

The design approach of the Spectrum Next port is to strip down the interpreter
to fit in the 64 KB main memory and switch in the currently used part of the
game in the bottom 16 KB of the main memory. 12 KB is used for the text screen
(using Timex hi-res mode), which leaves only 36 KB for the actual interpreter
(including interrupt handling code and stack memory). See
[memory_map.txt](memory_map.txt) for a detailed memory layout of the interpreter.
For comparison, the executable for the MS-DOS 16-bit version of the Level 9
interpreter is 87 KB.

In order to make the interpreter fit in this limited memory, certain parts of it
have been removed. For example, the following features have been removed:
support for V1 games (see below), scanning of the game file for determining game
version (instead a gamedata.txt file is used for storing the game version),
listing of the game's dictionary and bypassing of the copy protection in V3/V4
games by trying every word in the game's dictionary.

The Level 9 interpreter runs Level 9 game story files. There are four main
versions of the Level 9 game file format: V1 to V4. The Level 9 interpreter for
Spectrum Next only supports V2/V3/V4 games in order to minimise the interpreter
size and since all V1 games were re-released as V2/V3 games anyway. This means
that all of Level 9's games in their later versions are supported by the Level 9
interpreter for Spectrum Next. When loaded by the interpreter for Spectrum Next,
the game story file is loaded into a contiguous set of 8 KB banks. When a
certain part of the game is referenced, e.g. an instruction in the code, that
bank and the next bank are switched in to the bottom 16 KB of the main memory
before being referenced.

There are two types of graphics used in the Level 9 games. The earlier games
used line-drawn images that were memory-efficient and portable but very simple.
The later games used platform-specific bitmap images that were quite an
improvement. In order to reduce the interpreter size, the Level 9 interpreter
for Spectrum Next only supports loading and displaying of bitmap images in the
Spectrum Next layer 2 320x256 NXI format. The NXI images are created with two
custom tools. The [convert_gfx](tools/convert_gfx) tool is used for converting
the line-drawn images in a Level 9 graphics file (picture.dat) to separate NXI
image files. The [convert_bitmap](tools/convert_bitmap) tool is used for
converting Commodore Amiga and Atari ST Level 9 bitmap image files to NXI image
files.

The porting of the Level 9 interpreter to Spectrum Next is done using the
[z88dk](https://github.com/z88dk/z88dk) C compiler. In addition to modifying the
interpreter source code to support bank switching, some C constructs have been
changed to generate better Z80 machine code. For example, unsigned integers and
chars are used where possible, the z88dk fastcall calling convention is used
where applicable, etc. More technical information about the port can be found in
the level9.c file.

## User Interface

The screen of the Level 9 interpreter for Spectrum Next uses the Timex hi-res
mode (or standard ULA mode) as background layer for displaying the text with a
proportional font. The graphics is displayed using the layer 2 mode as the
foreground layer and a clip window to limit its height. When there are too many
lines of text to display on the screen at once, the message "&lt;MORE&gt;" will
appear at the bottom left of the screen in the border area using two hardware
sprites. The last entered line of input can be edited by pressing the EDIT key.
The graphics can be hardware scrolled up and down using the up and down arrow
keys to make more or less room for the text. If a PS/2 mouse is connected to the
Spectrum Next, it can also be used to scroll the graphics up and down by dragging
it with the mouse or using the mouse wheel.

The text colour can be changed by cycling downwards or upwards through a palette
of 32 colours by pressing TRUE VIDEO and INV VIDEO, respectively. The default
text colour is light grey.

The table below shows the special keys used by the Level 9 interpreter for
Spectrum Next:

| Spectrum Key |   PS/2 Key    |                               Description                                |
|--------------|---------------|--------------------------------------------------------------------------|
| EDIT         | SHIFT + 1     | Edit last entered line of input.                                         |
| UP           | SHIFT + 7     | Scroll graphics up.                                                      |
| DOWN         | SHIFT + 6     | Scroll graphics down.                                                    |
| TRUE VIDEO   | SHIFT + 3     | Change text colour by cycling downwards through a palette of 32 colours. |
| INV VIDEO    | SHIFT + 4     | Change text colour by cycling upwards through a palette of 32 colours.   |
| Mouse        | Mouse         | Scroll graphics up and down by dragging it or using the mouse wheel.     |

## Games

The Level 9 Compilation is an unofficial compilation of all of Level 9's text
adventure and multiple choice games for the Spectrum Next. Click on the link
below to read more about it and download it.

### [The Level 9 Compilation](compilation)

The Level 9 interpreter for Spectrum Next expects the Level 9 games to be
packaged in the following way:

```
<game-directory>/level9.nex
<game-directory>/gamedat*.dat
<game-directory>/gamedata.txt
<game-directory>/gfx/<number>.nxi
<game-directory>/gfx/prompt.spr
<game-directory>/gfx/mouse.spr
```

The level9.nex executable file is the Level 9 interpreter itself. The
gamedat*.dat game story file(s) is called gamedata.dat for single-part games and
gamedat1.dat, gamedat2.dat etc for multi-part games. The game story files must
not have any superfluous file header. The gamedata.txt text file contains as its
first character the digit 2, 3 or 4 representing the used game version V2, V3 or
V4. The subdirectory gfx contains the location images in NXI format with the
naming convention &lt;number&gt;.nxi where &lt;number&gt; is the location image
number used by the game. The file 0.nxi is the title image for V4 games. This
directory also contains the sprites for the scroll prompt and mouse pointer.
For the multi-part multiple choice games, the location images are located in
subdirectories gfx/&lt;game-part-number&gt;/, one for each part of the game.

## How to Build

If you want to build the Level 9 interpreter for Spectrum Next yourself, follow
the steps below:

1. On Windows, you need [MinGW/MSYS](https://osdn.net/projects/mingw/),
[Cygwin](https://www.cygwin.com/), [UnxUtils](https://sourceforge.net/projects/unxutils/)
or similar for the basic Unix commands (GNU make, Bash, mkdir, rm, cp, cat and zip).
Make sure the Unix commands are available in your PATH environment variable.

2. Download and install [z88dk](https://github.com/z88dk/z88dk) v2.1 or later.
Add the environment variable ZCCCFG whose value should be &lt;z88dk&gt;/lib/config.
Also add &lt;z88dk&gt;/bin to your PATH environment variable.

3. Download and install the latest version of the [CSpect](http://www.cspect.org/)
or [ZEsarUX](https://github.com/chernandezba/zesarux) Spectrum Next emulator.

4. Download the GitHub repository for the Level 9 interpreter for Spectrum Next
either as an archive using the "Code -> Download ZIP" button at the top of this
page or with Git using the following command:

```
> git clone https://github.com/stefanbylund/zxnext_level9.git
```

5. Go to the zxnext_level9 directory and enter the following command to do a
clean build using the default configuration:

```
> make clean all
```

6. Note: There are several configuration options when building and they are
handled by the configure.m4 file. Either edit the configure.m4 file or pass
configuration options via make like this:

```
> make clean all CONFIG="-DUSE_TIMEX_HIRES=1 -DUSE_GFX=1 -DUSE_MOUSE=1" BUILD_OPT=true
```

7. Note: The z88dk SDCC compiler is very slow when using the highest level of
optimisation. This can be controlled with BUILD_OPT=true or BUILD_OPT=false on
the make command-line. Setting BUILD_OPT=false or skipping it will give a much
quicker build but without the highest optimisations.

8. Note: Building with "make debug" instead of "make all" will create
zxnext_level9/src/*.lis files containing the generated Z80 assembly interleaved
with the C source code. The generated file zxnext_level9/bin/level9.map contains
all symbols and their addresses.

9. Run the generated binary zxnext_level9/bin/level9.nex in the
[CSpect](http://www.cspect.org/) or [ZEsarUX](https://github.com/chernandezba/zesarux)
emulator. Use the particular game directory as the root directory in the emulator.

## Source Code

This section briefly describes the main source files of the Level 9 interpreter
for Spectrum Next.

* level9.c <br>
Implementation of the core part of the Level 9 interpreter. Theoretically, this
file is platform-independent and should not be modified when porting to a new
target. However, the original implementation assumes a flat memory model and the
loaded game story file being kept in its entirety in main memory. The Spectrum
Next has only 64 KB addressable memory and a banked memory model. The
interpreter has been refactored to fit in the main memory and to load the game
story file into a contiguous set of memory banks that are mapped in as needed to
the bottom 16 KB of the 64 KB address space. Several changes have been done to
improve the performance for Spectrum Next.

* main.c <br>
Implementation of the platform-dependent part of the Level 9 interpreter.
Handles the user interface, terminal I/O, main loop, displaying of location
images, saving/loading of game state and playback of a script file.

* interrupt.asm <br>
Module for setting up IM2 interrupt mode.

* scroll_prompt.asm <br>
Module for showing/hiding the scroll prompt.

* text_color.asm <br>
Module for changing the text colour by cycling through a palette of 32 colours.

* layer2.c <br>
Module for loading, displaying and handling layer 2 320x256 images.

* sprite.c <br>
Module for loading, displaying and handling hardware sprites. Used for
displaying the scroll prompt and mouse pointer.

* mouse.c <br>
IM2 ISR for Kempston mouse support. Updates the mouse pointer sprite and invokes
a supplied mouse listener.

* image_scroll.asm <br>
Module for handling scrolling of the location image using the keyboard or mouse.

* crt_driver_instantiation.asm.m4 <br>
Custom z88dk module for setting up the input and output terminals.

* zx_01_input_kbd_inkey_custom.asm <br>
Input terminal driver subclass for handling special keys for location image
scrolling, changing text colour, edit the last entry etc.

* zx_01_output_fzx_custom.asm <br>
FZX output terminal driver subclass for standard ULA mode. Handles the scroll
prompt and scroll limit computations.

* tshr_01_output_fzx_custom.asm <br>
FZX output terminal driver subclass for Timex hi-res mode. Handles the scroll
prompt and scroll limit computations.

* in_key_translation_table.asm <br>
Overridden z88dk module for redefining the ASCII code for some special keys.

* asm_in_mouse_kempston.asm <br>
Overridden z88dk Kempston mouse driver.

* asm_in_mouse_kempston_wheel.asm <br>
Overridden z88dk Kempston mouse wheel driver.

## Links

* [Level 9 Interpreter](https://github.com/DavidKinder/Level9) <br>
  The source code of the open source Level 9 interpreter *Level9*, which the
  Spectrum Next porting is based on.

* [Level 9 Memorial](http://l9memorial.if-legends.org/html/home.html) <br>
  Information about Level 9 Computing and their games.

## License

The *Level9* interpreter is licensed under the terms of the GNU General Public
License version 2 and is copyright (C) 1996-2011 by Glen Summers and
contributors. Contributions from David Kinder, Alan Staniforth, Simon Baldwin,
Dieter Baron and Andreas Scherrer.

The *Level9* interface for ZX Spectrum Next is copyright (C) 2021 by Stefan
Bylund.

The QLStyle font is copyright (C) 2018 by Phoebus Dokos.
