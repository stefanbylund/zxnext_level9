/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Implementation of mouse.h; a C API for PS/2 Kempston mouse support.
 *
 * This module queries the mouse state from the mouse driver each 1/50th second
 * in an IM2 ISR. The module then updates the mouse pointer using a hardware
 * sprite and invokes a user-supplied mouse listener. The mouse pointer is
 * hidden if the mouse is not moved for a while.
 ******************************************************************************/

#include <z80.h>
#include <intrinsic.h>
#include <input.h>
#include <stdint.h>

#include "mouse.h"
#include "sprite.h"
#include "ide_friendly.h"

#define MOUSE_SPRITE_SLOT 63
#define MOUSE_INACTIVITY_LIMIT 500

static uint16_t mouse_x = 0;
static uint16_t mouse_y = 0;
static uint8_t mouse_buttons = 0;
static uint8_t mouse_wheel = 0;
static uint16_t mouse_inactivity_count = 0;
static MOUSE_LISTENER user_mouse_listener;

/*******************************************************************************
 * Kempston mouse wheel info:
 *
 * The Kempston mouse wheel is an unsigned 4-bit wrapping counter from 0 to 15
 * and can be thought of as a disc that can rotate in both directions and having
 * 16 equally-distanced positions. When the mouse wheel is moved forward the wheel
 * counter is decreased and when the mouse wheel is moved backwards it is increased.
 *
 * The mouse wheel delta is the new wheel counter position - the old wheel counter
 * position adjusted to a signed value from -8 to 8. A forward wheel movement gives
 * a negative wheel delta while a backwards wheel movement gives a positive wheel
 * delta. To avoid direction ambiguity, it is assumed that the mouse wheel can only
 * be moved less than half a rotation between two readings.
 ******************************************************************************/

IM2_DEFINE_ISR(mouse_handler)
{
    uint16_t last_mouse_x = mouse_x;
    uint16_t last_mouse_y = mouse_y;
    uint8_t new_mouse_wheel;
    int8_t wheel_delta = 0;

    // Get mouse coordinates and buttons state from mouse driver.
    in_mouse_kempston(&mouse_buttons, &mouse_x, &mouse_y);

    // Get mouse wheel counter value [0, 15] from mouse driver.
    new_mouse_wheel = (uint8_t) in_mouse_kempston_wheel();

    // Calculate signed mouse wheel delta [-8, 8].
    if (new_mouse_wheel != mouse_wheel)
    {
        wheel_delta = (int8_t) (new_mouse_wheel - mouse_wheel);
        if (wheel_delta > 7)
        {
            wheel_delta -= 16;
        }
        else if (wheel_delta < -7)
        {
            wheel_delta += 16;
        }
        mouse_wheel = new_mouse_wheel;
    }

    // Hide mouse pointer if mouse not moved for a while.
    if ((mouse_x == last_mouse_x) && (mouse_y == last_mouse_y))
    {
        mouse_inactivity_count++;
        if (mouse_inactivity_count == 0)
        {
            mouse_inactivity_count = MOUSE_INACTIVITY_LIMIT;
        }
    }
    else
    {
        mouse_inactivity_count = 0;
    }

    // Update mouse pointer hardware sprite.
    sprite_select_slot(MOUSE_SPRITE_SLOT);
    sprite_set_attributes(MOUSE_SPRITE_SLOT,
                          mouse_x,
                          mouse_y,
                          (mouse_inactivity_count < MOUSE_INACTIVITY_LIMIT));

    // Invoke user-supplied mouse listener.
    user_mouse_listener(mouse_x, (uint8_t) mouse_y, mouse_buttons, wheel_delta);
}

void init_mouse(const void *mouse_sprite_buf, MOUSE_LISTENER mouse_listener)
{
    user_mouse_listener = mouse_listener;

    // Load and set mouse pointer hardware sprite using the default sprite palette.
    sprite_load_patterns("gfx/mouse.spr", mouse_sprite_buf, 1, MOUSE_SPRITE_SLOT);

    // Init PS/2 Kempston mouse.
    in_mouse_kempston_init();
    in_mouse_kempston_setpos(0, 0);

    // Install mouse handler interrupt service routine.
    intrinsic_di();
    z80_bpoke(0xFDFD, 0xC3); // jp
    z80_wpoke(0xFDFE, (uint16_t) mouse_handler);
    intrinsic_ei();
}
