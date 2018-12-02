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


#pragma mark - Penalty Calculation

#define PENALTY_N1      3
#define PENALTY_N2      3
#define PENALTY_N3     40
#define PENALTY_N4     10

  // Calculates and returns the penalty score based on state of this QR Code's current modules.
  // This is used by the automatic mask choice algorithm to find the mask pattern that yields the lowest score.
  // @TODO: This can be optimized by working with the bytes instead of bits.
uint32_t getPenaltyScore(BitBucket *modules) {
	uint32_t result = 0;

	uint8_t size = modules->bitOffsetOrWidth;

	// Adjacent modules in row having same color
	for (uint8_t y = 0; y < size; y++) {

		bool colorX = bb_getBit(modules, 0, y);
		for (uint8_t x = 1, runX = 1; x < size; x++) {
			bool cx = bb_getBit(modules, x, y);
			if (cx != colorX) {
				colorX = cx;
				runX = 1;

			}
			else {
				runX++;
				if (runX == 5) {
					result += PENALTY_N1;
				}
				else if (runX > 5) {
					result++;
				}
			}
		}
	}

	// Adjacent modules in column having same color
	for (uint8_t x = 0; x < size; x++) {
		bool colorY = bb_getBit(modules, x, 0);
		for (uint8_t y = 1, runY = 1; y < size; y++) {
			bool cy = bb_getBit(modules, x, y);
			if (cy != colorY) {
				colorY = cy;
				runY = 1;
			}
			else {
				runY++;
				if (runY == 5) {
					result += PENALTY_N1;
				}
				else if (runY > 5) {
					result++;
				}
			}
		}
	}

	uint16_t black = 0;
	for (uint8_t y = 0; y < size; y++) {
		uint16_t bitsRow = 0, bitsCol = 0;
		for (uint8_t x = 0; x < size; x++) {
			bool color = bb_getBit(modules, x, y);

			// 2*2 blocks of modules having same color
			if (x > 0 && y > 0) {
				bool colorUL = bb_getBit(modules, x - 1, y - 1);
				bool colorUR = bb_getBit(modules, x, y - 1);
				bool colorL = bb_getBit(modules, x - 1, y);
				if (color == colorUL && color == colorUR && color == colorL) {
					result += PENALTY_N2;
				}
			}

			// Finder-like pattern in rows and columns
			bitsRow = ((bitsRow << 1) & 0x7FF) | color;
			bitsCol = ((bitsCol << 1) & 0x7FF) | bb_getBit(modules, y, x);

			// Needs 11 bits accumulated
			if (x >= 10) {
				if (bitsRow == 0x05D || bitsRow == 0x5D0) {
					result += PENALTY_N3;
				}
				if (bitsCol == 0x05D || bitsCol == 0x5D0) {
					result += PENALTY_N3;
				}
			}

			// Balance of black and white modules
			if (color) { black++; }
		}
	}

	// Find smallest k such that (45-5k)% <= dark/total <= (55+5k)%
	uint16_t total = size * size;
	for (uint16_t k = 0; black * 20 < (9 - k) * total || black * 20 > (11 + k) * total; k++) {
		result += PENALTY_N4;
	}

	return result;
}