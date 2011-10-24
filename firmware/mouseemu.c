/* Name: mouseemu.c
 * Project: atmega8-magnetometer-usb-mouse
 * Author: Denilson Figueiredo de Sa
 * Creation Date: 2011-10-18
 * Tabsize: 4
 * License: GNU GPL v2 or GNU GPL v3
 */


#include <math.h>

#include "buttons.h"
#include "common.h"
#include "mouseemu.h"


// HID report
MouseReport mouse_report;


void init_mouse_emulation() {  // {{{
	// According to avr-libc FAQ, the compiler automatically initializes
	// all variables with zero.
	mouse_report.report_id = 2;
	mouse_report.x = -1;
	mouse_report.y = -1;
	//mouse_report.buttons = 0;
}  // }}}


static uchar mouse_update_buttons() {  // {{{
	// Return 1 if the button state has been updated (and thus the report
	// should be sent to the computer).

	// Since the 3 buttons are already at the 3 least significant bits, no
	// complicated conversion is need.
	// 0x07 = 0000 0111
	uchar new_state = button.state & 0x07;
	uchar modified = (new_state != mouse_report.buttons);

	mouse_report.buttons = new_state;
	return modified;
}  // }}}


static uchar mouse_axes_no_conversion() {  // {{{
	// Get X, Y, Z data from the sensor, discard the Z component, and directly
	// use X, Y as the mouse position.
	// Only useful for debugging.

	SensorData *sens = &sensor;
	FIX_POINTER(sens);

	mouse_report.x = sens->data.x * 8 + 16384;
	mouse_report.y = sens->data.y * 8 + 16384;

	return 1;
}  // }}}


static void fill_matrix_from_sensor(float m[3][4]) {  // {{{
	SensorData *sens = &sensor;
	FIX_POINTER(sens);

	// The linear system:
	// -t*P + u*(B-A) + v*(C-A) = -A
	// Where:
	//   A = topleft
	//   B = topright
	//   C = bottomleft
	//   P = current point
	// The final, (x,y) screen coordinates are (u,v)
	//
	// That system is equivalent to this one:
	// A + u*(B-A) + v*(C-A) = t*P
	// Which means the current point is equal to the topleft corner, plus "u"
	// times in the topleft/topright direction, plus "v" times in the
	// topleft/bottomleft direction. "u" and "v" are between 0.0 and 1.0.

	// First column: - current_point
	m[0][0] = -sens->data.x;
	m[1][0] = -sens->data.y;
	m[2][0] = -sens->data.z;

	// Note: the values below are constant, and could have been pre-converted
	// to float only once (either at boot, or after updating those values).
	// It would save some cycles during this runtime.

	// Second column: topright - topleft
	m[0][1] = sens->e.corners[1].x - sens->e.corners[0].x;
	m[1][1] = sens->e.corners[1].y - sens->e.corners[0].y;
	m[2][1] = sens->e.corners[1].z - sens->e.corners[0].z;

	// Third column: bottomleft - topleft
	m[0][2] = sens->e.corners[2].x - sens->e.corners[0].x;
	m[1][2] = sens->e.corners[2].y - sens->e.corners[0].y;
	m[2][2] = sens->e.corners[2].z - sens->e.corners[0].z;

	// Fourth column: - topleft
	m[0][3] = -sens->e.corners[0].x;
	m[1][3] = -sens->e.corners[0].y;
	m[2][3] = -sens->e.corners[0].z;
}  // }}}

static void swap_rows(float a[4], float b[4]) {  // {{{
	float tmp;
	uchar i;

	for (i=0; i<4; i++) {
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
	}
}  // }}}

static uchar mouse_axes_linear_equation_system() {  // {{{
#define W 4
#define H 3
	// Matrix of 3 lines and 4 columns
	float m[H][W];
	// The solution of this system
	float sol[H];

	uchar y;

	fill_matrix_from_sensor(m);

	// Gauss-Jordan elimination, based on:
	// http://elonen.iki.fi/code/misc-notes/python-gaussj/index.html

	for(y = 0; y < H; y++) {
		uchar maxrow;
		uchar y2;
		float pivot;

		// Finding the row with the maximum value at this column
		maxrow = y;
		pivot = fabs(m[maxrow][y]);
		for(y2 = y+1; y2 < H; y2++) {
			float newpivot;
			newpivot = fabs(m[y2][y]);
			if (newpivot > pivot) {
				pivot = newpivot;
				maxrow = y2;
			}
		}
		// If the maximum value in this column is zero
		if (pivot < 0.0009765625) {  // 2**-10 == 0.0009765625
			// Singular
			return 0;
		}
		if (maxrow != y) {
			swap_rows(m[y], m[maxrow]);
		}

		// Now we are ready to eliminate the column y, using m[y][y]
		for(y2 = y+1; y2 < H; y2++) {
			uchar x;
			float c;
			c = m[y2][y] / m[y][y];

			// Subtracting from every element in row y2
			for(x = y; x < W; x++) {
				m[y2][x] -= m[y][x] * c;
			}
		}
	}

	// The matrix is now in "row echelon form" (it is a triangular matrix).
	// Instead of using a loop for back-substituting all 3 elements from
	// sol[], I'm calculating only 2 of them, as only those are needed.
	sol[2] = m[2][3] / m[2][2];

	sol[1] = m[1][3] / m[1][1] - m[1][2] * sol[2] / m[1][1];

	mouse_report.x = (int)(sol[1] * 32767);
	mouse_report.y = (int)(sol[2] * 32767);
	// sol[0] is discarded

	return 1;
#undef W
#undef H
}  // }}}


static uchar mouse_update_axes() {  // {{{
	// Update the report descriptor for the axes if new data is available from
	// the sensor.

	SensorData *sens = &sensor;
	FIX_POINTER(sens);

	if (sens->new_data_available && !sens->overflow) {
		// Marking the data as "used"
		sens->new_data_available = 0;

		// Trying to convert the coordinates
		if (
			//mouse_axes_no_conversion()
			mouse_axes_linear_equation_system()
		) {
			return 1;
		} else {
			// But sometimes it will fail
			return 0;
		}
	} else {
		// Clearing the x, y to invalid values.
		// Invalid values should be ignored by USB host.
		//mouse_report.x = -1;
		//mouse_report.y = -1;

		// Well... Actually, Linux 2.6.38 does not behave that way.
		// Instead, Linux moves the mouse pointer even if the supplied X,Y
		// values are outside the LOGICAL_MINIMUM..LOGICAL_MAXIMUM range.
		//
		// As a workaround:
		// If no data is available, I just leave the previous data in there.
		//
		// Note: Windows correctly ignores the invalid values.

		return 0;
	}

	return 0;
}  // }}}


uchar mouse_prepare_next_report() {  // {{{
	// Return 1 if a new report is available and should be sent to the
	// computer.

	// I'm using a bitwise OR here because a boolean OR would short-circuit
	// the expression and wouldn't run the second function. It's ugly, but
	// it's simple and works.
	return mouse_update_buttons() | mouse_update_axes();
}  // }}}


// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
