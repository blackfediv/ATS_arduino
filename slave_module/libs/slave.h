/******************************************************************************
 Atmel - C driver - TWI slave
   - TWI (I2C) slave driver, implemented in C for Atmel microcontroller
 Copyright (c) 2017 Martin Singer <martin.singer@web.de>
 ******************************************************************************/

/** TWI (I2C) slave driver.
 *
 * @file      slave.h
 * @see       slave.c
 * @author    Martin Singer
 * @date      2017
 * @copyright GNU General Public License version 3 (or in your opinion any later version)
 */


#ifndef TWI_SLAVE_H
#define TWI_SLAVE_H

#include <stdint.h>


#define TWI_SLAVE_MODE_RESET   0 ///< Every data transfer (r/w) starts with register address 0
                                 ///< (like a port extension IC)
#define TWI_SLAVE_MODE_ADDRESS 1 ///< First received data byte defines the r/w start register address
                                 ///< (like a EEPROM memory IC)


/** TWI slave data buffer struct definition.
 *
 * Read and write buffer.
 * The access to this buffer works for the TWI Master like the access to a generic TWI EEPROm.
 * The first data byte addresses the read/write position in the buffer.
 */
struct twi_slave_buffer_s {
	uint8_t volatile * data;  ///< Data buffer array.
	                          ///< (non-volatile pointer to volatile variable)
	uint8_t volatile pos;     ///< Buffer (next) read/write position.
	uint8_t volatile pos_max; ///< Last buffer element (size = pos_max + 1).
	uint8_t volatile mode;    ///< Register mode (reset, address)
};


/** TWI Slave data buffer. */
extern struct twi_slave_buffer_s twi_slave_buffer;


extern volatile uint8_t* twi_slave_init(uint8_t, uint8_t, uint8_t);
extern volatile int8_t twi_slave_connect_callback(void (*));


#endif // TWI_SLAVE_H

