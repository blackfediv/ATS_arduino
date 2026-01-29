/******************************************************************************
 Atmel - C driver - TWI slave
   - TWI (I2C) slave driver, implemented in C for Atmel microcontroller
 Copyright (c) 2017 Martin Singer <martin.singer@web.de>
 ******************************************************************************/

/** TWI (I2C) slave driver.
 *
 * @file      slave.c
 * @see       slave.h
 * @author    Martin Singer
 * @date      2017
 * @copyright GNU General Public License version 3 (or in your opinion any later version)
 *
 *
 * About
 * -----
 *
 *
 * Links
 * -----
 *
 * - <http://rn-wissen.de/wiki/index.php/TWI_Slave_mit_avr-gcc>
 * - <https://github.com/kelvinlawson/avr311-twi-slave-gcc/tree/master/standard>
 * - <http://homepage.hispeed.ch/peterfleury/avr-software.html#libs>
 * - <http://www.ermicro.com/blog/?p=1239>
 */


#include <stdlib.h>
#include <util/twi.h>
#include <avr/interrupt.h>

//#include <mk3/leds.h> //DEBUG
#include "slave.h"


#define BYTE_IS_DATA 0 ///< received byte is data (to store in the data array)_
#define BYTE_IS_ADDR 1 ///< received byte is buffer pos address
                  ///< (written to the twi_slave buffer.pos variable)


// Sends ACK after receiving data / expects ACK after sending data
// (pulling the SDA line low during the ninth SCL cycle)
#define twcr_ack() \
	do { \
		TWCR = (1<<TWEN)| \
		       (1<<TWIE)|(1<<TWINT)| \
		       (1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)| \
		       (0<<TWWC); \
	} while (0)

// Sends NACK after receiving data / expects NACK after sending data
// (leaves the SDA line high during the ninth SCL cycle;
// virtually disconnected from the TWI)
#define twcr_nack() \
	do { \
		TWCR = (1<<TWEN)| \
		       (1<<TWIE)|(1<<TWINT)| \
		       (0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)| \
		       (0<<TWWC); \
	} while (0)

// switch to the non addressed slave mode ...
#define twcr_reset() \
	do { \
		TWCR = (1<<TWEN)| \
		       (1<<TWIE)|(1<<TWINT)| \
		       (1<<TWEA)|(0<<TWSTA)|(1<<TWSTO)| \
		       (0<<TWWC); \
	} while (0)


/** TWI Slave data buffer. */
struct twi_slave_buffer_s twi_slave_buffer = { NULL, 0, 0, TWI_SLAVE_MODE_RESET };


/** Callback function anchor.
 *
 * * The connected function becomes called back in the end of `ISR(TWI_vect)`.
 * * The anchor becomes connected with `twi_slave_connect_callback(void (*))`.
 */
void volatile (*twi_slave_callback) (uint8_t const);


// Local Prototypes
void twi_slave_buffer_pos_set(uint8_t);
void twi_slave_buffer_pos_next(void);


/** ISR: TWI Event Interrupt.
 *
 * This routine is called if a TWI Event happened.
 * It evaluates the TWSR register, to detect which event happened.
 */
ISR (TWI_vect)
{
	static uint8_t received_byte_type = BYTE_IS_DATA;
	uint8_t const tw_status = TW_STATUS; // TW_STATUS = TWSR & TW_STATUS_MASK

	switch (tw_status) {

		// Slave Receiver
		case TW_SR_SLA_ACK:   // 0x60: SLA+W received, ACK returned
			switch (twi_slave_buffer.mode) {
				case TWI_SLAVE_MODE_RESET:
					twi_slave_buffer_pos_set(0);
					received_byte_type = BYTE_IS_DATA;
					break;
				case TWI_SLAVE_MODE_ADDRESS:
					received_byte_type = BYTE_IS_ADDR;
					break;
			}
			twcr_ack();
			break;
		case TW_SR_DATA_ACK:  // 0x80: data received, ACK returned
			switch (received_byte_type) {
				case BYTE_IS_ADDR:
					twi_slave_buffer_pos_set(TWDR);
					received_byte_type = BYTE_IS_DATA;
					break;
				case BYTE_IS_DATA:
					twi_slave_buffer.data[twi_slave_buffer.pos] = TWDR;
					twi_slave_buffer_pos_next();
					break;
			}
			twcr_ack();
			break;
		case TW_SR_DATA_NACK: // 0x88: data received, NACK returned
			twcr_reset();
			break;
		case TW_SR_STOP:      // 0xA0: stop or repeated start condition received while selected
			twcr_ack();
			break;

		// Slave Transmitter
		case TW_ST_SLA_ACK:   // 0xA8: SLA+R received, ACK returned
			if (twi_slave_buffer.mode == TWI_SLAVE_MODE_RESET) {
				twi_slave_buffer_pos_set(0);
			}
			// no break!
		case TW_ST_DATA_ACK:  // 0xB8: data transmitted, ACK received
			TWDR = twi_slave_buffer.data[twi_slave_buffer.pos];
//			mk3_led_set_output(twi_slave_buffer.data[twi_slave_buffer.pos]); //DEBUG
			twi_slave_buffer_pos_next();
			twcr_ack();
			break;
		case TW_ST_DATA_NACK: // 0xC0: data transmitted, NACK received - no more data is requested
			twcr_reset();
			break;
		case TW_ST_LAST_DATA: // 0xC8: last data byte in TWDR transmitted (TWEA = “0”), ACK received
			twcr_reset();
			break;
		default:
			twcr_reset();
			break;
	}

	if (twi_slave_callback != NULL) {
		twi_slave_callback(tw_status);
	}
}


/** Initialize the microcontroller as TWI slave.
 *
 * @param addr   Slave address to set for this IC,
 *               the 0th bit is reserved to indicate the read/write mode (done by the master).
 *               (only use the higher 7 bits [1..7] for the address)
 * @param max    Maximal buffer register address.
 *               Defines the size of the TWI slave data buffer (size = max + 1).
 *               Valid values are [0..255].
 *               - 0 indicates a buffer size of 1 byte,
 *                   and selects implicit the direct buffer selection mode.
 *                 that means every data byte becomes written in or read from this one buffer byte.
 *                 there is no buffer register (address, position) selection prcedure.
 *               - a value from 1 to 255 selects implicit the buffer addressing mode.
 *                 that means, the first data byte sent by the master
 *                 selects the buffer register (address, position), to read from, or to write in.
 * @param mode   Buffer mode:
 *               - TWI_SLAVE_MODE_RESET
 *               - TWI_SLAVE_MODE_ADDRESS
 * @return       Pointer to TWI slave buffer (twi_slave_buffer[0]).
 *               (non-volatile pointer to volatile data)
 */
volatile uint8_t* twi_slave_init(uint8_t addr, uint8_t max, uint8_t mode)
{
	twi_slave_callback = NULL;

	twi_slave_buffer.data = NULL;
	twi_slave_buffer.pos_max = max;
	twi_slave_buffer.mode = mode;
	twi_slave_buffer.pos = 0;
	twi_slave_buffer.data = malloc((max + 1) * sizeof(uint8_t));
	if (twi_slave_buffer.data != NULL) {
		TWAR = addr << 1;
		TWBR = 18;
		TWSR |= (1 << TWPS0);
		TWCR &= ~((1 << TWSTA)|(1 << TWSTO));
		TWCR |= (1 << TWEA)|(1 << TWEN)|(1 << TWIE);
	}
	return twi_slave_buffer.data;
}


/** Connect callback function to callback anchor.
 *
 * * The callback function becomes called in the end of the ISR(TWI_vect) function.
 * * Committing 'NULL' for the function pointer parameter disconnects the callback call.
 *
 * @param (*fp) function pointer to callback function,
 *              from type 'void function(uint8_t const)'
 * @retval  0   callback function was connected right
 * @retval -1   something went wrong
 */
int8_t twi_slave_connect_callback(void (*fp))
{
	twi_slave_callback = fp;
	if (twi_slave_callback != fp) {
		return -1;
	}
	return 0;
}


/** Set buffer read/write position to array element.
 *
 * @param pos Data buffer array position.
 */
void twi_slave_buffer_pos_set(uint8_t pos)
{
	if (pos <= twi_slave_buffer.pos_max) {
		twi_slave_buffer.pos = pos;
	}
}


/** Iterate buffer read/write position to next array element. */
void twi_slave_buffer_pos_next(void)
{
	if (twi_slave_buffer.pos < twi_slave_buffer.pos_max) {
		++twi_slave_buffer.pos;
	} else {
		twi_slave_buffer.pos = 0;
	}
}

