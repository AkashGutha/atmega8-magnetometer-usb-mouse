/* Name: main.c
 * Project: atmega8-magnetometer-usb-mouse
 * Author: Denilson Figueiredo de Sa
 * Creation Date: 2011-08-29
 * Tabsize: 4
 * License: GNU GPL v2 or GNU GPL v3
 *
 * Includes third-party code:
 *
 * - V-USB from OBJECTIVE DEVELOPMENT Software GmbH
 *   http://www.obdev.at/products/vusb/index.html
 *
 * - USBaspLoader from OBJECTIVE DEVELOPMENT Software GmbH
 *   http://www.obdev.at/products/vusb/usbasploader.html
 *
 * - AVR315 TWI Master Implementation from Atmel
 *   http://www.atmel.com/dyn/products/documents.asp?category_id=163&family_id=607&subfamily_id=760
 */

// Headers from AVR-Libc
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

// V-USB driver from http://www.obdev.at/products/vusb/
#include "usbdrv.h"

// It's also possible to include "usbdrv.c" directly, if we also add
// this definition at the top of this file:
// #define USB_PUBLIC static
// However, this only saved 10 bytes.

// I'm not using serial-line debugging
//#include "oddebug.h"

// AVR315 Using the TWI module as I2C master
#include "avr315/TWI_Master.h"

// Non-blocking interrupt-based EEPROM writing.
#include "int_eeprom.h"

// Sensor communication over I2C (TWI)
#include "sensor.h"

// Menu user interface for configuring the device
#include "menu.h"

// Button handling code
#include "buttons.h"

// Keyboard emulation code
#include "keyemu.h"

////////////////////////////////////////////////////////////
// Hardware description                                  {{{

/* ATmega8 pin assignments:
 * PB0: (not used)
 * PB1: (not used)
 * PB2: (not used)
 * PB3: (not used - MOSI)
 * PB4: (not used - MISO)
 * PB5: (not used - SCK)
 * PB6: 12MHz crystal
 * PB7: 12MHz crystal
 *
 * PC0: Button 1
 * PC1: Button 2
 * PC2: Button 3
 * PC3: Switch button (if held to GND during power-on, starts the bootloader)
 * PC4: I2C - SDA
 * PC5: I2C - SCL
 * PC6: Reset pin (with an external 10K pull-up to VCC)
 *
 * PD0: USB-
 * PD1: (not used - debug tx)
 * PD2: USB+ (int0)
 * PD3: (not used)
 * PD4: (not used)
 * PD5: red debug LED
 * PD6: yellow debug LED
 * PD7: green debug LED
 *
 * If you change the ports, remember to update these functions:
 * - hardware_init()
 * - update_button_state()
 */

#define LED_TURN_ON(led)  do { PORTD |=  (led); } while(0)
#define LED_TURN_OFF(led) do { PORTD &= ~(led); } while(0)
#define LED_TOGGLE(led)   do { PORTD ^=  (led); } while(0)

// Bit masks for each LED (in PORTD)
#define RED_LED    (1 << 5)
#define YELLOW_LED (1 << 6)
#define GREEN_LED  (1 << 7)
#define ALL_LEDS   (GREEN_LED | YELLOW_LED | RED_LED)

// }}}

////////////////////////////////////////////////////////////
// USB HID Report Descriptor                             {{{

uchar report_buffer[2];    /* buffer for HID reports */

// XXX: If this HID report descriptor is changed, remember to update
//      USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH from usbconfig.h
PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH]
__attribute__((externally_visible))
= {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};
/* We use a simplifed keyboard report descriptor which does not support the
 * boot protocol. We don't allow setting status LEDs and we only allow one
 * simultaneous key press (except modifiers). We can therefore use short
 * 2 byte input reports.
 * The report descriptor has been created with usb.org's "HID Descriptor Tool"
 * which can be downloaded from http://www.usb.org/developers/hidpage/.
 * Redundant entries (such as LOGICAL_MINIMUM and USAGE_PAGE) have been omitted
 * for the second INPUT item.
 */

// }}}

////////////////////////////////////////////////////////////
// Main code                                             {{{

static const char hello_world[] PROGMEM = "Hello, !@#$%&*() -_ =+ ,< .> ;: /?\n";

// As defined in section 7.2.4 Set_Idle Request
// of Device Class Definition for Human Interface Devices (HID) version 1.11
// pages 52 and 53 (or 62 and 63) of HID1_11.pdf
//
// Set/Get IDLE defines how long the device should keep "quiet" if the state
// has not changed.
// Recommended default value for keyboard is 500ms, and infinity for joystick
// and mice.
//
// This value is measured in multiples of 4ms.
// A value of zero means indefinite/infinity.
static uchar idle_rate;


static void hardware_init(void) {  // {{{
	// Configuring Watchdog to about 2 seconds
	// See pages 43 and 44 from ATmega8 datasheet
	// See also http://www.nongnu.org/avr-libc/user-manual/group__avr__watchdog.html
	wdt_enable(WDTO_2S);

	PORTB = 0xff;  // activate all pull-ups
	DDRB = 0;      // all pins input
	PORTC = 0xff;  // activate all pull-ups
	DDRC = 0;      // all pins input

	// From usbdrv.h:
	//#define USBMASK ((1<<USB_CFG_DPLUS_BIT) | (1<<USB_CFG_DMINUS_BIT))

	// activate pull-ups, except on USB lines and LED pins
	PORTD = 0xFF ^ (USBMASK | ALL_LEDS);
	// LED pins as output, the other pins as input
	DDRD = 0 | ALL_LEDS;

	// Doing a USB reset
	// This is done here because the device might have been reset
	// by the watchdog or some condition other than power-up.
	//
	// A reset is done by holding both D+ and D- low (setting the
	// pins as output with value zero) for longer than 10ms.
	//
	// See page 145 of usb_20.pdf
	// See also http://www.beyondlogic.org/usbnutshell/usb2.shtml

	DDRD |= USBMASK;    // Setting as output
	PORTD &= ~USBMASK;  // Setting as zero

	_delay_ms(15);  // Holding this state for at least 10ms

	DDRD &= ~USBMASK;   // Setting as input
	//PORTD &= ~USBMASK;  // Pull-ups are already disabled

	// End of USB reset


	// Disabling Timer0 Interrupt
	// It's disabled by default, anyway, so this shouldn't be needed
	TIMSK &= ~(TOIE0);

	// Configuring Timer0 (with main clock at 12MHz)
	// 0 = No clock (timer stopped)
	// 1 = Prescaler = 1     =>   0.0213333ms
	// 2 = Prescaler = 8     =>   0.1706666ms
	// 3 = Prescaler = 64    =>   1.3653333ms
	// 4 = Prescaler = 256   =>   5.4613333ms
	// 5 = Prescaler = 1024  =>  21.8453333ms
	// 6 = External clock source on T0 pin (falling edge)
	// 7 = External clock source on T0 pin (rising edge)
	//
	// See page 72 from ATmega8 datasheet.
	// Also thanks to http://frank.circleofcurrent.com/cache/avrtimercalc.htm
	TCCR0 = 3;

	// I'm using Timer0 as a 1.365ms ticker. Every time it overflows, the TOV0
	// flag in TIFR is set. The main loop clears this flag near the end of each
	// iteration.

	// I'm not using serial-line debugging
	//odDebugInit();

	LED_TURN_ON(YELLOW_LED);
}  // }}}


uchar
__attribute__((externally_visible))
usbFunctionSetup(uchar data[8]) {  // {{{
	usbRequest_t *rq = (void *)data;

	usbMsgPtr = report_buffer;
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {    /* class request type */
		if (rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
			/* we only have one report type, so don't look at wValue */

			// XXX: Ainda não entendi quando isto é chamado...
			LED_TOGGLE(GREEN_LED);
			build_report_from_char('\0');

			usbMsgPtr = report_buffer;
			return sizeof(report_buffer);
		} else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
			usbMsgPtr = &idle_rate;
			return 1;
		} else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
			idle_rate = rq->wValue.bytes[1];
		}
	} else {
		/* no vendor specific requests implemented */
	}
	return 0;
}  // }}}


void
__attribute__ ((noreturn))
main(void) {  // {{{
	uchar should_send_report = 1;
	int idle_counter = 0;

	uchar sensor_probe_counter = 0;

	cli();

	hardware_init();
	TWI_Master_Initialise();
	usbInit();
	init_int_eeprom();

	wdt_reset();
	sei();

	init_button_state();
	init_keyboard_emulation();
	init_ui_system();

	// Sensor initialization must be done with interrupts enabled!
	sensor_init_configuration();

	LED_TURN_ON(GREEN_LED);

	for (;;) {	// main event loop
		wdt_reset();
		usbPoll();

		update_button_state();

		// Red LED lights up if there is any kind of error in I2C communication
		if ( TWI_statusReg.lastTransOK ) {
			LED_TURN_OFF(RED_LED);
		} else {
			LED_TURN_ON(RED_LED);
		}

		// Handling of the main switch
		if (button.changed & BUTTON_SWITCH) {
			// When the switch changes state, it's better to reset this
			// variable, in order to avoid bugs.
			sensor.func_step = 0;
		}

		if (ON_KEY_UP(BUTTON_SWITCH)) {
			// Upon releasing the switch, stop the continuous reading.
			sensor.continuous_reading = 0;
			//sensor_stop_continuous_reading();

			// And also reset the menu system.
			init_ui_system();
		}

		if (button.state & BUTTON_SWITCH) {
			sensor.continuous_reading = 1;
		} else {
			ui_main_code();
		}

		// Continuous reading of sensor data
		if (sensor.continuous_reading) {
			// Timer is set to 1.365ms
			if (TIFR & (1<<TOV0)) {
				// The sensor is configured for 75Hz measurements.
				// I'm using this timer to read the values at twice that rate.
				// 5 * 1.365ms = 6.827ms ~= 146Hz
				if (sensor_probe_counter > 0){
					// Waiting...
					sensor_probe_counter--;
				}
			}
			if (sensor_probe_counter == 0) {
				// Time for reading new data!
				uchar return_code;

				return_code = sensor_read_data_registers();
				if (return_code == SENSOR_FUNC_DONE || return_code == SENSOR_FUNC_ERROR) {
					// Restart the counter+timer
					sensor_probe_counter = 5;
				}
			}
		}


		// Timer is set to 1.365ms
		if (TIFR & (1<<TOV0)) {
			// Implementing the idle rate...
			if (idle_rate != 0) {
				if (idle_counter > 0){
					idle_counter--;
				} else {
					// idle_counter counts how many Timer0 overflows are
					// required before sending another report.
					// The exact formula is:
					// idle_counter = (idle_rate * 4)/1.365;
					// But it's better to avoid floating point math.
					// 4/1.365 = 2.93, so let's just multiply it by 3.
					idle_counter = idle_rate * 3;

					//keyDidChange = 1;
					LED_TOGGLE(YELLOW_LED);
					// TODO: implement this... should re-send the current status
					// Either that, or the idle_rate support should be removed.
				}
			}
		}

		// Resetting the Timer0
		if (TIFR & (1<<TOV0)) {
			// Setting this bit to one will clear it.
			// Yeah, weird, but that's how it works.
			TIFR = 1<<TOV0;
		}

		// Automatically send report if there is something in the buffer.
		if (string_output_pointer != NULL) {
			// TODO: refactor this "should_send_report" var. Currently, its
			// meaning is the same as comparing string_output_buffer to NULL.
			// So, it's redundant.
			should_send_report = 1;
		}

		// Sending USB Interrupt-in report
		if(should_send_report && usbInterruptIsReady()){
			// TODO: no need to return a value here...
			should_send_report = send_next_char();
			usbSetInterrupt(report_buffer, sizeof(report_buffer));
		}
	}
}  // }}}

// }}}

// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
