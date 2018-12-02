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


#pragma mark - Mode testing and conversion

static int8_t getAlphanumeric(char c) {

	if (c >= '0' && c <= '9') { return (c - '0'); }
	if (c >= 'A' && c <= 'Z') { return (c - 'A' + 10); }

	switch (c) {
	case ' ': return 36;
	case '$': return 37;
	case '%': return 38;
	case '*': return 39;
	case '+': return 40;
	case '-': return 41;
	case '.': return 42;
	case '/': return 43;
	case ':': return 44;
	}

	return -1;
}

bool isAlphanumeric(const char *text, uint16_t length) {
	while (length != 0) {
		if (getAlphanumeric(text[--length]) == -1) { return false; }
	}
	return true;
}


bool isNumeric(const char *text, uint16_t length) {
	while (length != 0) {
		char c = text[--length];
		if (c < '0' || c > '9') { return false; }
	}
	return true;
}
