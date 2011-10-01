/* Name: eeprom.inc.c
 * Project: atmega8-magnetometer-usb-mouse
 * Author: Denilson Figueiredo de Sa
 * Creation Date: 2011-10-01
 * Tabsize: 4
 * License: GNU GPL v2 or GNU GPL v3
 *
 * Non-blocking EEPROM writing code.
 * For reading from the EEPROM, use the avr-libc functions.
 *
 * The interrupt handler code is loosely based on "AVR104 Buffered Interrupt
 * Controlled EEPROM Writes on tinyAVR and megaAVR devices".
 */

// AVR-GCC includes:
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>


// Maximum block that can be written to the EEPROM
#define EEPROM_BUFFER_SIZE   25

// EEPROM destination address
static void* eeprom_address;
// Buffer for writing the EEPROM
static unsigned char eeprom_buffer[EEPROM_BUFFER_SIZE];
// The size of the block currently in the buffer
static unsigned char eeprom_block_size;
// The index of the next byte to be written
static unsigned char eeprom_next_byte;


#define ENABLE_EE_RDY_INTERRUPT()   do { EECR |=  (1 << EERIE); } while(0)
#define DISABLE_EE_RDY_INTERRUPT()  do { EECR &= ~(1 << EERIE); } while(0)


static void init_eeprom_handling() {  // {{{
	DISABLE_EE_RDY_INTERRUPT();
	eeprom_block_size = 0;
}  // }}}

static void eeprom_write_block_int(
		const void * src,
		void* address,
		unsigned char size) {  // {{{

	memcpy(eeprom_buffer, src, size);
	eeprom_block_size = size;
	eeprom_address = address;
	eeprom_next_byte = 0;

	ENABLE_EE_RDY_INTERRUPT();
}  // }}}

ISR(EE_RDY_vect) {  // {{{
	//if ( SPMCR & (1 << SPMEN) ) // Is Self-Programming Currently Active?
	//	return;                   // Yes, Return to main()

	EEAR = (unsigned int) eeprom_address + eeprom_next_byte;
	EEDR = eeprom_buffer[eeprom_next_byte];

	EECR |= (1 << EEMWE);  // Assert EEPROM Master Write Enable
	EECR |= (1 << EEWE);   // Assert EEPROM Write Enable

	eeprom_next_byte++;

	if (eeprom_next_byte == eeprom_block_size) {
		// Buffer is now empty
		eeprom_block_size = 0;

		DISABLE_EE_RDY_INTERRUPT();
	}
}  // }}}

// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
