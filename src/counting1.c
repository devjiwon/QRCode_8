/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Richard Moore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  *  Special thanks to Nayuki (https://www.nayuki.io/) from which this library was
  *  heavily inspired and compared against.
  *
  *  See: https://github.com/nayuki/QR-Code-generator/tree/master/cpp
  */

#include "qrcode.h"

#include <stdlib.h>
#include <string.h>

#pragma mark - Counting




  // We store the following tightly packed (less 8) in modeInfo
  //               <=9  <=26  <= 40
  // NUMERIC      ( 10,   12,    14);
  // ALPHANUMERIC (  9,   11,    13);
  // BYTE         (  8,   16,    16);
char getModeBits(uint8_t version, uint8_t mode) {
	// Note: We use 15 instead of 16; since 15 doesn't exist and we cannot store 16 (8 + 8) in 3 bits
	// hex(int("".join(reversed([('00' + bin(x - 8)[2:])[-3:] for x in [10, 9, 8, 12, 11, 15, 14, 13, 15]])), 2))
	unsigned int modeInfo = 0x7bbb80a;

#if LOCK_VERSION == 0 || LOCK_VERSION > 9
	if (version > 9) { modeInfo >>= 9; }
#endif

#if LOCK_VERSION == 0 || LOCK_VERSION > 26
	if (version > 26) { modeInfo >>= 9; }
#endif

	char result = 8 + ((modeInfo >> (3 * mode)) & 0x07);
	if (result == 15) { result = 16; }

	return result;
}