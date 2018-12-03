
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

bool isAlphanumeric(const char *text, uint16_t length);
bool isNumeric(const char *text, uint16_t length);
char getModeBits(uint8_t version, uint8_t mode);

#pragma mark - QrCode

int8_t encodeDataCodewords(BitBucket *dataCodewords, const uint8_t *text, uint16_t length, uint8_t version) {
	int8_t mode = MODE_BYTE;

	if (isNumeric((char*)text, length)) {
		mode = MODE_NUMERIC;
		bb_appendBits(dataCodewords, 1 << MODE_NUMERIC, 4);
		bb_appendBits(dataCodewords, length, getModeBits(version, MODE_NUMERIC));

		uint16_t accumData = 0;
		uint8_t accumCount = 0;
		for (uint16_t i = 0; i < length; i++) {
			accumData = accumData * 10 + ((char)(text[i]) - '0');
			accumCount++;
			if (accumCount == 3) {
				bb_appendBits(dataCodewords, accumData, 10);
				accumData = 0;
				accumCount = 0;
			}
		}

		// 1 or 2 digits remaining
		if (accumCount > 0) {
			bb_appendBits(dataCodewords, accumData, accumCount * 3 + 1);
		}

	}
	else if (isAlphanumeric((char*)text, length)) {
		mode = MODE_ALPHANUMERIC;
		bb_appendBits(dataCodewords, 1 << MODE_ALPHANUMERIC, 4);
		bb_appendBits(dataCodewords, length, getModeBits(version, MODE_ALPHANUMERIC));

		uint16_t accumData = 0;
		uint8_t accumCount = 0;
		for (uint16_t i = 0; i < length; i++) {
			accumData = accumData * 45 + getAlphanumeric((char)(text[i]));
			accumCount++;
			if (accumCount == 2) {
				bb_appendBits(dataCodewords, accumData, 11);
				accumData = 0;
				accumCount = 0;
			}
		}

		// 1 character remaining
		if (accumCount > 0) {
			bb_appendBits(dataCodewords, accumData, 6);
		}

	}
	else {
		bb_appendBits(dataCodewords, 1 << MODE_BYTE, 4);
		bb_appendBits(dataCodewords, length, getModeBits(version, MODE_BYTE));
		for (uint16_t i = 0; i < length; i++) {
			bb_appendBits(dataCodewords, (char)(text[i]), 8);
		}
	}

	//bb_setBits(dataCodewords, length, 4, getModeBits(version, mode));

	return mode;
}

void performErrorCorrection(uint8_t version, uint8_t ecc, BitBucket *data) {

	// See: http://www.thonky.com/qr-code-tutorial/structure-final-message

#if LOCK_VERSION == 0
	uint8_t numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecc][version - 1];
	uint16_t totalEcc = NUM_ERROR_CORRECTION_CODEWORDS[ecc][version - 1];
	uint16_t moduleCount = NUM_RAW_DATA_MODULES[version - 1];
#else
	uint8_t numBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecc];
	uint16_t totalEcc = NUM_ERROR_CORRECTION_CODEWORDS[ecc];
	uint16_t moduleCount = NUM_RAW_DATA_MODULES;
#endif

	uint8_t blockEccLen = totalEcc / numBlocks;
	uint8_t numShortBlocks = numBlocks - moduleCount / 8 % numBlocks;
	uint8_t shortBlockLen = moduleCount / 8 / numBlocks;

	uint8_t shortDataBlockLen = shortBlockLen - blockEccLen;

	uint8_t result[data->capacityBytes];
	memset(result, 0, sizeof(result));

	uint8_t coeff[blockEccLen];
	rs_init(blockEccLen, coeff);

	uint16_t offset = 0;
	uint8_t *dataBytes = data->data;


	// Interleave all short blocks
	for (uint8_t i = 0; i < shortDataBlockLen; i++) {
		uint16_t index = i;
		uint8_t stride = shortDataBlockLen;
		for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum++) {
			result[offset++] = dataBytes[index];

#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
			if (blockNum == numShortBlocks) { stride++; }
#endif
			index += stride;
		}
	}

	// Version less than 5 only have short blocks
#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
	{
		// Interleave long blocks
		uint16_t index = shortDataBlockLen * (numShortBlocks + 1);
		uint8_t stride = shortDataBlockLen;
		for (uint8_t blockNum = 0; blockNum < numBlocks - numShortBlocks; blockNum++) {
			result[offset++] = dataBytes[index];

			if (blockNum == 0) { stride++; }
			index += stride;
		}
	}
#endif

	// Add all ecc blocks, interleaved
	uint8_t blockSize = shortDataBlockLen;
	for (uint8_t blockNum = 0; blockNum < numBlocks; blockNum++) {

#if LOCK_VERSION == 0 || LOCK_VERSION >= 5
		if (blockNum == numShortBlocks) { blockSize++; }
#endif
		rs_getRemainder(blockEccLen, coeff, dataBytes, blockSize, &result[offset + blockNum], numBlocks);
		dataBytes += blockSize;
	}

	memcpy(data->data, result, data->capacityBytes);
	data->bitOffsetOrWidth = moduleCount;
}