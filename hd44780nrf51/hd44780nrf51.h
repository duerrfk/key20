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

#ifndef HD44780NRF51_H
#define HD44780NRF51_H

#include <stdbool.h>

#ifndef HD44780_2nd_LINE_OFFSET
// The DDRAM memory offset of the second line of the display.
#define HD44780_2nd_LINE_OFFSET 0x40
#endif

/**
 * Pins connected to the LCD.
 *
 * The LCD is operated in 4-bit mode, thus, pins DB0-3 are not required.
 * 
 * The LCD is time-driven using wait statements rather than testing the
 * busy flag. Thus, we do never read from the display and assume that the r/w 
 * pin of the display has been hard-wired to GND (fixed write mode).
 */
struct hd44780 {
     unsigned int pin_rs;  /**< Register select */
     unsigned int pin_e;   /**< Read/write start */
     unsigned int pin_db4; /**< Data pin 4 */ 
     unsigned int pin_db5; /**< Data pin 5 */
     unsigned int pin_db6; /**< Data pin 6 */
     unsigned int pin_db7; /**< Data pin 7 */
     unsigned int rows;    /**< Number of rows (1 or 2) */
     unsigned int columns; /**< Number of columns */
};

/**
 * Initialization of the LCD.
 *
 *  @param lcd definition of the LCD display to be used.
 */
void hd44780_init(const struct hd44780 *lcd);

/**
 * Turn the display, cursor, and cursor blinking on or off.  
 *
 * @param lcd definition of the LCD display to be used.
 * @param display_on if true, turn display on; otherwise turn display off.
 * @param cursor_on if true, turn cursor on; otherwise turn cursor off.
 * @param cursor_blinking if true, let cursor blink; otherwise turn blinking 
 * off.
 */
void hd44780_display_on_off(const struct hd44780 *lcd, bool display_on, 
		    bool cursor_on, bool cursor_blinking);

/**
 * Prints one line of text.
 *
 * @param lcd definition of the LCD display to be used.
 * @param text text to be printed.
 * @param length length of text.
 * @param line line (0 or 1) where the text is to be printed.
 */
void hd44780_print_line(const struct hd44780 *lcd, const char *text, 
			unsigned int length, unsigned int line);

/**
 * Clears the whole display.
 *
 * @param lcd definition of the LCD display to be used.
 */
void hd44780_clear_display(const struct hd44780 *lcd);

/**
 * Clears one line if the display by writing spaces to all characters.
 *
 * @param lcd definition of the LCD display to be used.
 * @param line line number (0 or 1) to be cleared.
 */
void hd44780_clear_line(const struct hd44780 *lcd, unsigned int line);

#endif
