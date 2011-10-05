/* Name: int_eeprom.h
 *
 * See the .c file for more information
 */

#ifndef __int_eeprom_h_included__
#define __int_eeprom_h_included__

// Init does nothing
#define init_int_eeprom() do{ }while(0)

void int_eeprom_write_block(
		const void * src,
		void* address,
		unsigned char size);


#endif  // __int_eeprom_h_included____

// vim:noexpandtab tabstop=4 shiftwidth=4 foldmethod=marker foldmarker={{{,}}}
