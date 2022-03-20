/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Define IDE_FRIENDLY in your C IDE to disable Z88DK C extensions and avoid
 * parser errors/warnings in the IDE. Do NOT define IDE_FRIENDLY when compiling
 * the code with Z88DK.
 ******************************************************************************/

#ifndef _IDE_FRIENDLY_H
#define _IDE_FRIENDLY_H

#ifdef IDE_FRIENDLY
#define __z88dk_fastcall
#define __z88dk_callee
#define __preserves_regs(...)
#define __naked
#define IM2_DEFINE_ISR(name) void name(void)
#define IO_NEXTREG_REG ((void *) NULL)
#define IO_NEXTREG_DAT ((void *) NULL)
#define IO_SPRITE_SLOT ((void *) NULL)
#define IO_SPRITE_ATTRIBUTE ((void *) NULL)
#define IO_FF ((void *) NULL)
#define IO_7FFD ((void *) NULL)
#define IO_LAYER_2_CONFIG ((void *) NULL)
#endif

#endif
