/* Name: menu.c
 * Project: atmega8-magnetometer-usb-mouse
 * Author: Denilson Figueiredo de Sa
 * Creation Date: 2011-09-27
 * Tabsize: 4
 * License: GNU GPL v2 or GNU GPL v3
 */


// For NULL definition
#include <stddef.h>

#include <avr/pgmspace.h>

#include "buttons.h"
#include "common.h"
#include "int_eeprom.h"
#include "keyemu.h"
#include "sensor.h"

#include "menu.h"


#define BUTTON_PREV    BUTTON_1
#define BUTTON_NEXT    BUTTON_2
#define BUTTON_CONFIRM BUTTON_3


// Things that are currently declared at main.c:
uchar* debug_print_X_Y_Z_to_string_output_buffer(XYZVector* vector);


////////////////////////////////////////////////////////////
// Menu definitions (constants in progmem)               {{{

// All available UI widgets  {{{

// Menus
// Note: the ROOT (empty) menu must be ZERO
#define UI_ROOT_MENU    0
#define UI_MAIN_MENU    1
#define UI_ZERO_MENU    2
#define UI_CORNERS_MENU 3
#define UI_SENSOR_MENU  4

// UI_MIN_MENU_ID must be ZERO
#define UI_MIN_MENU_ID UI_ROOT_MENU
#define UI_MAX_MENU_ID UI_SENSOR_MENU

// Checking if a widget is a menu
#define UI_IS_MENU(id) ((id) >= UI_MIN_MENU_ID && (id) <= UI_MAX_MENU_ID)

// Other widgets
#define UI_ZERO_PRINT_WIDGET       0x10
#define UI_ZERO_CAL_WIDGET         0x11
#define UI_ZERO_TOGGLE_WIDGET      0x12
#define UI_SENSOR_ID_WIDGET        0x13
#define UI_SENSOR_XYZ_ONCE_WIDGET  0x14
#define UI_SENSOR_XYZ_CONT_WIDGET  0x15
// }}}

typedef struct MenuItem {  // {{{
	// The menu text string pointer
	PGM_P text;
	// The widget that will be activated on this menu item
	uchar action;
} MenuItem;  // }}}

// Root/empty menu  {{{
// Yeah, this is just a "fake" menu with only one empty item.

static const char     empty_menu_1[] PROGMEM = "";
#define               empty_menu_total_items 1
static const MenuItem empty_menu_items[] PROGMEM = {
	{empty_menu_1, UI_MAIN_MENU}
};
// }}}

// Error menu, for when something goes wrong  {{{
static const char     error_menu_1[] PROGMEM = "Error in menu system!\n";
#define               error_menu_total_items 1
static const MenuItem error_menu_items[] PROGMEM = {
	{error_menu_1, 0}
};
// }}}

// Main menu, with all main options  {{{
static const char     main_menu_1[] PROGMEM = "1. Calibrate zero\n";
static const char     main_menu_2[] PROGMEM = "2. Calibrate corners\n";
static const char     main_menu_3[] PROGMEM = "3. Sensor data\n";
static const char     main_menu_4[] PROGMEM = "4. << quit menu\n";
#define               main_menu_total_items 4
static const MenuItem main_menu_items[] PROGMEM = {
	{main_menu_1, UI_ZERO_MENU},
	{main_menu_2, UI_CORNERS_MENU},
	{main_menu_3, UI_SENSOR_MENU},
	{main_menu_4, 0}
};
// }}}

// Zero calibration menu  {{{
static const char     zero_menu_1[] PROGMEM = "1.1. Print calibrated zero\n";
static const char     zero_menu_2[] PROGMEM = "1.2. Recalibrate zero\n";
static const char     zero_menu_3[] PROGMEM = "1.3. Toggle zero compensation\n";
static const char     zero_menu_4[] PROGMEM = "1.4. << main menu\n";
#define               zero_menu_total_items 4
static const MenuItem zero_menu_items[] PROGMEM = {
	{zero_menu_1, UI_ZERO_PRINT_WIDGET},
	{zero_menu_2, UI_ZERO_CAL_WIDGET},
	{zero_menu_3, UI_ZERO_TOGGLE_WIDGET},
	{zero_menu_4, 0}
};

// Other messages:
static const char zero_calibration[] PROGMEM = "Move the sensor in all possible directions, then press the button to finish\n";
static const char zero_compensation_prefix[] PROGMEM = "Zero compensation is ";
static const char zero_compensation_suffix_on[] PROGMEM = "ENABLED\n";
static const char zero_compensation_suffix_off[] PROGMEM = "DISABLED\n";
// }}}

// Corner calibration menu  {{{
static const char     corners_menu_1[] PROGMEM = "2.1. Print calibrated corners\n";
static const char     corners_menu_2[] PROGMEM = "2.2. Recalibrate corners\n";
static const char     corners_menu_3[] PROGMEM = "2.3. TODO: toggle algorithm being used, but here?\n";
static const char     corners_menu_4[] PROGMEM = "2.4. << main menu\n";
#define               corners_menu_total_items 4
static const MenuItem corners_menu_items[] PROGMEM = {
	{corners_menu_1, UI_ROOT_MENU},
	{corners_menu_2, UI_ROOT_MENU},
	{corners_menu_3, UI_ROOT_MENU},
	{corners_menu_4, 0}
};
// }}}

// Sensor data menu  {{{
static const char     sensor_menu_1[] PROGMEM = "3.1. Print sensor identification string\n";
static const char     sensor_menu_2[] PROGMEM = "3.2. Print X,Y,Z once\n";
static const char     sensor_menu_3[] PROGMEM = "3.3. Print X,Y,Z continually\n";
static const char     sensor_menu_4[] PROGMEM = "3.4. << main menu\n";
#define               sensor_menu_total_items 4
static const MenuItem sensor_menu_items[] PROGMEM = {
	{sensor_menu_1, UI_SENSOR_ID_WIDGET},
	{sensor_menu_2, UI_SENSOR_XYZ_ONCE_WIDGET},
	{sensor_menu_3, UI_SENSOR_XYZ_CONT_WIDGET},
	{sensor_menu_4, 0}
};

// Error message:
static const char  error_sensor_string[] PROGMEM = "Error while reading the sensor!\n";
// }}}

// Data used by ui_load_menu_items()  {{{
typedef struct MenuLoadingInfo {
	// Pointer to the array of MenuItems
	PGM_VOID_P menu_items;
	// Number of items in this menu
	uchar total_items;
} MenuLoadingInfo;

#define MENU_LOADING(prefix) {prefix##_menu_items, prefix##_menu_total_items}
static const MenuLoadingInfo menu_loading[] PROGMEM = {
	MENU_LOADING(error),  // The error menu is at element 0
	MENU_LOADING(empty),  // And the UI with id ZERO starts at element 1
	MENU_LOADING(main),
	MENU_LOADING(zero),
	MENU_LOADING(corners),
	MENU_LOADING(sensor)
};
#undef MENU_LOADING
// }}}

// }}}

////////////////////////////////////////////////////////////
// UI and Menu variables (state)                         {{{

typedef struct UIState {
	// Current active UI widget
	uchar widget_id;
	// Current menu item, or other state for non-menu widgets
	uchar menu_item;
} UIState;

// Current UI state
UIState ui;

// At most 5 stacked widgets. This value is arbitrary.
UIState ui_stack[5];
uchar ui_stack_top;

// Each menu can have at most 6 menu items. This value is arbitrary.
static MenuItem ui_menu_items[6];
static uchar ui_menu_total_items;

// Should the current menu item be printed in the next ui_main_code() call?
// If the firmware is busy printing something else, this flag won't be reset,
// and the current menu item will be printed whenever it is appropriate.
uchar ui_should_print_menu_item;

// }}}

////////////////////////////////////////////////////////////
// UI and menu handling functions                        {{{

static void ui_load_menu_items() {  // {{{
	// Loads the menu strings from PROGMEM to RAM

	// Load from this id
	uchar id;
	// Pointer to the source address
	PGM_VOID_P menu_items;

	if (UI_IS_MENU(ui.widget_id)) {
		id = ui.widget_id - UI_MIN_MENU_ID + 1;
	} else {
		id = 0;
	}

	// This version is more portable:
	//memcpy_P(&menu_items, &menu_loading[id].menu_items, sizeof(PGM_P));
	// But this version is shorter (40 bytes smaller):
	// But this assertion must be true: assert(sizeof(PGM_VOID_P) == 2)
	menu_items = (PGM_VOID_P) pgm_read_word_near(&menu_loading[id].menu_items);

	ui_menu_total_items = pgm_read_byte_near(&menu_loading[id].total_items);

	// Copying to RAM array ui_menu_items
	memcpy_P(ui_menu_items, menu_items, ui_menu_total_items * sizeof(*ui_menu_items));
}  // }}}

void ui_push_state() {  // {{{
	ui_stack[ui_stack_top] = ui;
	ui_stack_top++;
}  // }}}

void ui_pop_state() {  // {{{
	if (ui_stack_top > 0) {
		ui_stack_top--;
		ui = ui_stack[ui_stack_top];
	} else {
		ui.widget_id = UI_ROOT_MENU;
		ui.menu_item = 0;
	}

	// If the state is a menu, reload the items and print the current item.
	if (UI_IS_MENU(ui.widget_id)) {
		ui_load_menu_items();
		ui_should_print_menu_item = 1;
	}
}  // }}}

static void ui_prev_menu_item() {  // {{{
	if (ui.menu_item == 0) {
		ui.menu_item = ui_menu_total_items;
	}
	ui.menu_item--;
	ui_should_print_menu_item = 1;
}  // }}}

static void ui_next_menu_item() {  // {{{
	ui.menu_item++;
	if (ui.menu_item == ui_menu_total_items) {
		ui.menu_item = 0;
	}
	ui_should_print_menu_item = 1;
}  // }}}

static void ui_enter_widget(uchar new_widget) {  // {{{
	// Pushes the current widget (usually a parent menu), and enters a
	// (sub)menu or widget.

	ui_push_state();

	ui.widget_id = new_widget;
	ui.menu_item = 0;

	// If the new widget is a menu, load the items and print the current one.
	if (UI_IS_MENU(ui.widget_id)) {
		ui_load_menu_items();
		ui_should_print_menu_item = 1;
	}
}  // }}}

// }}}

////////////////////////////////////////////////////////////
// UI and menu public functions                          {{{

void init_ui_system() {   // {{{
	// Must be called in the main initialization routine

	// Emptying the stack
	ui_stack_top = 0;

	// Calling ui_pop_state() with an empty stack will reload the initial
	// widget (the root/empty menu).
	ui_pop_state();
}  // }}}

static void ui_menu_code() {  // {{{

	// If the current menu item needs to be printed and the firmware is not
	// busy printing something else
	if (ui_should_print_menu_item && string_output_pointer == NULL) {
		output_pgm_string(ui_menu_items[ui.menu_item].text);
		ui_should_print_menu_item = 0;
	}

	// Handling button events
	if (ON_KEY_DOWN(BUTTON_PREV)) {
		ui_prev_menu_item();
	} else if (ON_KEY_DOWN(BUTTON_NEXT)) {
		ui_next_menu_item();
	} else if (ON_KEY_DOWN(BUTTON_CONFIRM)) {
		uchar action;
		action = ui_menu_items[ui.menu_item].action;

		if (action == 0) {
			ui_pop_state();
		} else {
			ui_enter_widget(action);
		}
	}
}  // }}}

void ui_main_code() {  // {{{
	// This must be called in the main loop.
	//
	// This function handles the actions of all UI widgets.

	uchar return_code;

	SensorData *sens = &sensor;
	FIX_POINTER(sens);

	if (UI_IS_MENU(ui.widget_id)) {
		ui_menu_code();
	} else {
		switch (ui.widget_id) {
			////////////////////
			case UI_ZERO_PRINT_WIDGET:  // {{{
				if (string_output_pointer != NULL) {
					// Do nothing, let's wait the previous output...
				} else {
					// Printing X,Y,Z zero...
					debug_print_X_Y_Z_to_string_output_buffer(&sens->zero);

					// ...and the boolean value
					strcat_P(string_output_buffer, zero_compensation_prefix);
					if (sens->zero_compensation) {
						strcat_P(string_output_buffer, zero_compensation_suffix_on);
					} else {
						strcat_P(string_output_buffer, zero_compensation_suffix_off);
					}

					string_output_pointer = string_output_buffer;
					ui_pop_state();
				}
				break;  // }}}

			////////////////////
			case UI_ZERO_CAL_WIDGET:  // {{{
				if (ui.menu_item == 0) {
					if (string_output_pointer != NULL) {
						// Do nothing, let's wait the previous output...
						break;
					}

					// Print instructions
					output_pgm_string(zero_calibration);

					// Must disable zero compensation before calibration
					sens->zero_compensation = 0;

					sensor_start_continuous_reading();
					ui.menu_item = 1;
				} else if(ui.menu_item == 1) {
					// The first reading
					if (sens->new_data_available) {
						sens->new_data_available = 0;

						if (!sens->overflow) {
							sens->zero_min = sens->data;
							sens->zero_max = sens->data;
							// Using memcpy or simple attribution cost the same amount of bytes
							//memcpy(&sens->zero_min, &sens->data, sizeof(sens->data));
							//memcpy(&sens->zero_max, &sens->data, sizeof(sens->data));

							ui.menu_item = 2;
						}
					}
				} else {
					if (sens->new_data_available) {
						sens->new_data_available = 0;

						if (!sens->overflow) {
							// The code inside this if costs 172 bytes :(
							if (sens->data.x < sens->zero_min.x) sens->zero_min.x = sens->data.x;
							if (sens->data.y < sens->zero_min.y) sens->zero_min.y = sens->data.y;
							if (sens->data.z < sens->zero_min.z) sens->zero_min.z = sens->data.z;

							if (sens->data.x > sens->zero_max.x) sens->zero_max.x = sens->data.x;
							if (sens->data.y > sens->zero_max.y) sens->zero_max.y = sens->data.y;
							if (sens->data.z > sens->zero_max.z) sens->zero_max.z = sens->data.z;
						}

						if (string_output_pointer == NULL) {
							debug_print_X_Y_Z_to_string_output_buffer(&sens->data);
							string_output_pointer = string_output_buffer;
						}
					}

					if (ON_KEY_DOWN(BUTTON_CONFIRM)) {
						sensor_stop_continuous_reading();

						sens->zero.x = (sens->zero_min.x + sens->zero_max.x) / 2;
						sens->zero.y = (sens->zero_min.y + sens->zero_max.y) / 2;
						sens->zero.z = (sens->zero_min.z + sens->zero_max.z) / 2;

						// Saving to EEPROM
						int_eeprom_write_block(&sens->zero, EEPROM_SENSOR_ZERO_VECTOR, sizeof(sens->zero));

						// FIXME: must save this together with the zero...
						sens->zero_compensation = 1;

						ui_pop_state();
						ui_enter_widget(UI_ZERO_PRINT_WIDGET);
					}
				}
				break;  // }}}

			////////////////////
			case UI_ZERO_TOGGLE_WIDGET:  // {{{
				// Toggling current state
				sens->zero_compensation = !sens->zero_compensation;

				// Saving to EEPROM
				// Note: I can't get the address of a bit-field
				//int_eeprom_write_block(&sens->zero_compensation, EEPROM_SENSOR_ZERO_ENABLE, 1);
				// using "return_code" as a temporary var. Ugly, I know.
				return_code = sens->zero_compensation;
				int_eeprom_write_block(&return_code, EEPROM_SENSOR_ZERO_ENABLE, 1);

				ui_pop_state();
				ui_enter_widget(UI_ZERO_PRINT_WIDGET);
				break;  // }}}


			////////////////////
			case UI_SENSOR_ID_WIDGET:  // {{{
				if (string_output_pointer != NULL) {
					// Do nothing, let's wait the previous output...
					break;
				}
				if (ui.menu_item == 0) {
					sens->func_step = 0;
					ui.menu_item = 1;
				}

				return_code = sensor_read_identification_string(string_output_buffer);

				if (return_code == SENSOR_FUNC_DONE) {
					string_output_buffer[3] = '\n';
					string_output_buffer[4] = '\0';
					// I could have used this function:
					//append_newline_to_str(string_output_buffer);
					// But it adds 18 bytes to the firmware

					string_output_pointer = string_output_buffer;
					ui_pop_state();
				} else if (return_code == SENSOR_FUNC_ERROR) {
					output_pgm_string(error_sensor_string);
					ui_pop_state();
				}
				break;  // }}}

			////////////////////
			case UI_SENSOR_XYZ_ONCE_WIDGET:  // {{{
				if (ui.menu_item == 0) {
					if (string_output_pointer != NULL) {
						// Do nothing, let's wait the previous output...
						break;
					}
					sensor_start_continuous_reading();
					ui.menu_item = 1;
				} else {
					if (string_output_pointer == NULL) {
						if (sens->new_data_available) {
							sens->new_data_available = 0;
							sensor_stop_continuous_reading();
							debug_print_X_Y_Z_to_string_output_buffer(&sens->data);
							string_output_pointer = string_output_buffer;
							ui_pop_state();
						} else if (sens->error_while_reading) {
							sensor_stop_continuous_reading();
							output_pgm_string(error_sensor_string);
							ui_pop_state();
						}
					}
				}
				break;  // }}}

			////////////////////
			case UI_SENSOR_XYZ_CONT_WIDGET:  // {{{
				if (ui.menu_item == 0) {
					if (string_output_pointer != NULL) {
						// Do nothing, let's wait the previous output...
						break;
					}
					sensor_start_continuous_reading();
					ui.menu_item = 1;
				} else {
					if (string_output_pointer == NULL) {
						if (sens->new_data_available) {
							sens->new_data_available = 0;
							debug_print_X_Y_Z_to_string_output_buffer(&sens->data);
							string_output_pointer = string_output_buffer;
						} else if (sens->error_while_reading) {
							sensor_stop_continuous_reading();
							output_pgm_string(error_sensor_string);
							ui_pop_state();
						}
					}
					if (ON_KEY_DOWN(BUTTON_CONFIRM)) {
						sensor_stop_continuous_reading();
						ui_pop_state();
					}
				}
				break;  // }}}

			default:
				// Fallback in case of errors
				ui_pop_state();
		}
	}
}  // }}}

// }}}


// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
