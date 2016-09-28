/**
 * Copyright 2016 Frank Duerr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hd44780nrf51.h"
#include <nrf_gpio.h>
#include <nrf_delay.h>

// Waiting time for fast instructions [us]. Must be longer than 40 us.
#define SHORT_WAIT 50
// Waiting time for slow instructions [ms]. Must be longer than 1.52 ms.
#define LONG_WAIT 2

// The following definitions should make it easy to port the code to other
// platforms than nRF51.
#define PIN_CFG_OUTPOUT(X) nrf_gpio_cfg_output(X)
#define PIN_CLR(X) nrf_gpio_pin_clear(X)
#define PIN_SET(X) nrf_gpio_pin_set(X)
#define DELAY_MS(X) nrf_delay_ms(X)
#define DELAY_US(X) nrf_delay_us(X)

/**
 * Wait for fast instructions requiring 37 us to 40 us waiting time.
 */
static void short_instr_wait()
{
     DELAY_US(SHORT_WAIT);
}

/**
 * Wait for slow instructions requiring 1.52 ms waiting time.
 */
static void long_instr_wait()
{
     DELAY_MS(LONG_WAIT);
}

/**
 * Send enable pulse.
 *
 * The enable pin is set high for 1 us, then low for 1 us.
 * According to the data sheet, the pulse width must be at least 450 ns,
 * and the maximum cycle time at least 1 us.
 *
 * @param lcd definition of the LCD display to be used. 
 */ 
static void enable(const struct hd44780 *lcd)
{
     // Send pulse of 1 us width.
     PIN_SET(lcd->pin_e);
     DELAY_US(1);
     // Clear pin for 1 us.
     PIN_CLR(lcd->pin_e);
     DELAY_US(1);
}

/**
 * Set nibble of 4 bits on data pins. The four bits are the 4 LSBs of
 * parameter "bits".
 *
 * @param lcd definition of the LCD display to be used.
 * @param bits the 4 LSBs of this byte will be used to set the nibble.
 */
static void set_nibble(const struct hd44780 *lcd, uint8_t bits)
{
     if (bits&0x1)
	  PIN_SET(lcd->pin_db4);
     else
	  PIN_CLR(lcd->pin_db4);

     if (bits&0x2)
	  PIN_SET(lcd->pin_db5);
     else
	  PIN_CLR(lcd->pin_db5);

     if (bits&0x4)
	  PIN_SET(lcd->pin_db6);
     else
	  PIN_CLR(lcd->pin_db6);

     if (bits&0x8)
	  PIN_SET(lcd->pin_db7);
     else
	  PIN_CLR(lcd->pin_db7);
}

/**
 * Send one byte output as two nibbles in 4 bit mode (first high nibble,
 * then low nibble).
 *
 * @param lcd definition of the LCD display to be used.
 * @param data the byte to be sent.
 */
static void send_byte(const struct hd44780 *lcd, uint8_t data)
{     
     uint8_t nibble = (data >> 4);
     set_nibble(lcd, nibble);
     enable(lcd);

     nibble = (data & 0x0f);
     set_nibble(lcd, nibble);
     enable(lcd);
}

/**
 * Initialize pins.
 *
 * All pins are set as outputs with low level.
 *
 * @param lcd definition of the LCD display to be used. 
 */
static void init_pins(const struct hd44780 *lcd)
{
     // All pins are outputs.
     PIN_CFG_OUTPOUT(lcd->pin_rs);
     PIN_CFG_OUTPOUT(lcd->pin_e);
     PIN_CFG_OUTPOUT(lcd->pin_db4);
     PIN_CFG_OUTPOUT(lcd->pin_db5);
     PIN_CFG_OUTPOUT(lcd->pin_db6);
     PIN_CFG_OUTPOUT(lcd->pin_db7);

     // Set all pins low.
     PIN_CLR(lcd->pin_rs);
     PIN_CLR(lcd->pin_e);
     PIN_CLR(lcd->pin_db4);
     PIN_CLR(lcd->pin_db5);
     PIN_CLR(lcd->pin_db6);
     PIN_CLR(lcd->pin_db7);
}

/**
 * Send function set command.
 *
 * This command sets two pieces of information:
 *
 * The number of lines (1 or 2). This information is taken from parameter
 * lcd.
 *
 * The character font (5x8 dots or 5x10 dots). Since 5x10 dot fonts are
 * seldomly implemented by displays, we always use the 5x8 dots font. 
 *
 * @param lcd definition of the LCD display to be used.
 */
static void cmd_function_set(const struct hd44780 *lcd) 
{
     PIN_CLR(lcd->pin_rs);

     // Byte pattern: 0 0 1 DL N F * *
     // DL = 0: 4 bits data length; DL = 1: 8 bit data length
     // N = 0: 1 line; N = 1: 2 lines
     // F = 0: 5x8 dot font; F = 1: 5 x 10 font (seldomly implemented)
     uint8_t data = 0x20;
     data |= (lcd->rows == 1 ? 0x00 : 0x08);
     send_byte(lcd, data);

     short_instr_wait();
}

/**
 * Send display on/off command.
 *
 * @param lcd definition of the LCD display to be used.
 * @param display_on if true, turn on the display; otherwise turn display off.
 * @param cursor_on if true, show cursors; otherwise turn cursor off.
 * @param cursor_blinking if true, let cursor bling; otherwise don't blink.
 */
static void cmd_display_on_off(const struct hd44780 *lcd, bool display_on,
			       bool cursor_on, bool cursor_blinking) 
{
     PIN_CLR(lcd->pin_rs);

     // Byte pattern: 0 0 0 0 1 D C B
     // D: 0 = display off; 1 = display on
     // C: 0 = cursor off; 1 = cursor on
     // B: 0 = cursor not blinking; 1 = cursor blinking
     uint8_t data = 0x08;
     if (display_on)
	  data |= 0x04;
     if (cursor_on)
	  data |= 0x02;
     if (cursor_blinking)
	  data |= 0x01;
     send_byte(lcd, data);
     
     short_instr_wait();
}

/**
 * Send clear display command.
 *
 * @param lcd definition of the LCD display to be used. 
 */
static void cmd_clear_display(const struct hd44780 *lcd)
{
     PIN_CLR(lcd->pin_rs);

     // Byte pattern: 0 0 0 0 0 0 0 1
     uint8_t data = 0x01;
     send_byte(lcd, data);

     long_instr_wait();
}

/**
 * Set the DDRAM address for writing the next character.
 *
 * @param lcd definition of the LCD display to be used.
 * @param addr DDRAM address.
 */
static void cmd_set_ddram_addr(const struct hd44780 *lcd, uint8_t addr)
{
     PIN_CLR(lcd->pin_rs);

     uint8_t data = 0x80;
     data |= (addr & 0x7f);
     send_byte(lcd, data);

     short_instr_wait();
}

/**
 * Send set entry mode command.
 *
 * @param lcd definition of the LCD display to be used. 
 * @param incded if true, increase DDRAM address after writing a character, 
 * else decrease DDRAM address.
 * @param shift if true, shift entire display to left (incdec == false) or 
 * right (incdec = true); otherwise do not shift display. 
 */
static void cmd_set_entry_mode(const struct hd44780 *lcd, bool incdec,
     bool shift)
{
     PIN_CLR(lcd->pin_rs);

     // Byte pattern: 0 0 0 0 0 1 ID S
     // ID: increase (1) or decrease (0) DDRAM position after writing character.
     // S: shift entire display (1); do not shift display (0)
     uint8_t data = 0x04;
     if (incdec)
	  data |= 0x02;
     if (shift)
	  data |= 0x01;
     send_byte(lcd, data);

     short_instr_wait();
}

/**
 * Go through initialization sequence as described in HD44780 data sheet.
 *
 * After this sequence, the display is off and the cursor is off. The entry
 * mode is set to. DDRAM address increase after character write; no display
 * shift.
 *
 * @param lcd definition of the LCD display to be used. 
 */
static void init_sequence(const struct hd44780 *lcd)
{
     // According to HD44780 data sheet, need to wait 40 ms
     // after voltage rises to 2.7 V. We give some safety margin since
     // we do not know exactly whether right at this point we have
     // already reached 2.7 V (brown-out detection of the nRF51 engages at 
     // 1.7 V). 
     DELAY_MS(100);

     PIN_CLR(lcd->pin_rs);

     set_nibble(lcd, 0x03);
     enable(lcd);
     // Need to wait more than 4.1 ms.
     DELAY_MS(9);
     
     set_nibble(lcd, 0x03);
     enable(lcd);
     // Need to wait more than 100 us.
     DELAY_US(200);

     set_nibble(lcd, 0x03);
     enable(lcd);
     short_instr_wait();

     set_nibble(lcd, 0x02);
     enable(lcd);
     short_instr_wait();
     
     // Set number of rows, font, and 4-bit mode.
     cmd_function_set(lcd);

     // Turn display off, cursor off, no blinking cursor.
     cmd_display_on_off(lcd, false, false, false);

     // Clear the display
     cmd_clear_display(lcd);

     // Set the entry mode: DDRAM address increase after writing,
     // no display shifting.
     cmd_set_entry_mode(lcd, true, false);
}

void hd44780_init(const struct hd44780 *lcd)
{
     init_pins(lcd);
     
     init_sequence(lcd);
}

void hd44780_display_on_off(const struct hd44780 *lcd, bool display_on, 
		    bool cursor_on, bool cursor_blinking)
{
     cmd_display_on_off(lcd, display_on, cursor_on, cursor_blinking);
}

void hd44780_print_line(const struct hd44780 *lcd, const char *text, 
			unsigned int length, unsigned int line)
{
     if (line == 0)
	  cmd_set_ddram_addr(lcd, 0);
     else
	  cmd_set_ddram_addr(lcd, HD44780_2nd_LINE_OFFSET);

     PIN_SET(lcd->pin_rs);
     for (unsigned int i = 0; i < length; i++) {
	  uint8_t character = text[i];
	  send_byte(lcd, character);
	  short_instr_wait();
	  // After the write operation, the DDRAM address is automatically
	  // incremented by 1.
     }
}

void hd44780_clear_display(const struct hd44780 *lcd)
{
     cmd_clear_display(lcd);
}

void hd44780_clear_line(const struct hd44780 *lcd, unsigned int line)
{
     if (line == 0)
	  cmd_set_ddram_addr(lcd, 0);
     else
	  cmd_set_ddram_addr(lcd, HD44780_2nd_LINE_OFFSET);

     PIN_SET(lcd->pin_rs);
     for (unsigned int i = 0; i < lcd->columns; i++) {
	  uint8_t character = ' ';
	  send_byte(lcd, character);
	  short_instr_wait();
	  // After the write operation, the DDRAM address is automatically
	  // incremented by 1.
     }
}
