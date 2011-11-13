/* Name: buttons.h
 *
 * See the .c file for more information
 */

#ifndef __buttons_h_included__
#define __buttons_h_included__

#include "common.h"


typedef struct ButtonState {
	// "Public" vars, already filtered for debouncing
	uchar state;
	uchar changed;

	// This is used to "freeze" the pointer movement for a short while, right
	// after a click, in order to avoid accidentally dragging the clicked
	// object. This is useful because the sensor captures a lot of noise.
	uchar recent_state_change;

	// "Private" button debouncing state
	uchar debouncing[4];  // We have 4 buttons/switches
} ButtonState;

extern ButtonState button;


// Bit masks for each button (in PORTC and PINC)
// (also used in button.state and button.changed vars)
#define BUTTON_1      (1 << 0)
#define BUTTON_2      (1 << 1)
#define BUTTON_3      (1 << 2)
#define BUTTON_SWITCH (1 << 3)
#define ALL_BUTTONS   (BUTTON_1 | BUTTON_2 | BUTTON_3 | BUTTON_SWITCH)


// Handy macros!
// These have the same name/meaning of JavaScript events
#define ON_KEY_DOWN(button_mask) ((button.changed & (button_mask)) &&  (button.state & (button_mask)))
#define ON_KEY_UP(button_mask)   ((button.changed & (button_mask)) && !(button.state & (button_mask)))


// Init does nothing
#define init_button_state() do{ }while(0)

void update_button_state(uchar timer_overflow);


#endif  // __buttons_h_included____

// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
