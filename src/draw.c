
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

#pragma mark - Drawing Patterns

// XORs the data modules in this QR Code with the given mask pattern. Due to XOR's mathematical
// properties, calling applyMask(m) twice with the same value is equivalent to no change at all.
// This means it is possible to apply a mask, undo it, and try another mask. Note that a final
// well-formed QR Code symbol needs exactly one mask applied (not zero, not two, etc.).
static void applyMask(BitBucket *modules, BitBucket *isFunction, uint8_t mask) {
    uint8_t size = modules->bitOffsetOrWidth;
    
    for (uint8_t y = 0; y < size; y++) {
        for (uint8_t x = 0; x < size; x++) {
            if (bb_getBit(isFunction, x, y)) { continue; }
            
            bool invert = 0;
            switch (mask) {
                case 0:  invert = (x + y) % 2 == 0;                    break;
                case 1:  invert = y % 2 == 0;                          break;
                case 2:  invert = x % 3 == 0;                          break;
                case 3:  invert = (x + y) % 3 == 0;                    break;
                case 4:  invert = (x / 3 + y / 2) % 2 == 0;            break;
                case 5:  invert = x * y % 2 + x * y % 3 == 0;          break;
                case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;    break;
                case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;  break;
            }
            bb_invertBit(modules, x, y, invert);
        }
    }
}

static void setFunctionModule(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y, bool on) {
    bb_setBit(modules, x, y, on);
    bb_setBit(isFunction, x, y, true);
}

// Draws a 9*9 finder pattern including the border separator, with the center module at (x, y).
static void drawFinderPattern(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y) {
    uint8_t size = modules->bitOffsetOrWidth;

    for (int8_t i = -4; i <= 4; i++) {
        for (int8_t j = -4; j <= 4; j++) {
            uint8_t dist = max(abs(i), abs(j));  // Chebyshev/infinity norm
            int16_t xx = x + j, yy = y + i;
            if (0 <= xx && xx < size && 0 <= yy && yy < size) {
                setFunctionModule(modules, isFunction, xx, yy, dist != 2 && dist != 4);
            }
        }
    }
}

// Draws a 5*5 alignment pattern, with the center module at (x, y).
static void drawAlignmentPattern(BitBucket *modules, BitBucket *isFunction, uint8_t x, uint8_t y) {
    for (int8_t i = -2; i <= 2; i++) {
        for (int8_t j = -2; j <= 2; j++) {
            setFunctionModule(modules, isFunction, x + j, y + i, max(abs(i), abs(j)) != 1);
        }
    }
}

// Draws two copies of the format bits (with its own error correction code)
// based on the given mask and this object's error correction level field.
static void drawFormatBits(BitBucket *modules, BitBucket *isFunction, uint8_t ecc, uint8_t mask) {
    
    uint8_t size = modules->bitOffsetOrWidth;

    // Calculate error correction code and pack bits
    uint32_t data = ecc << 3 | mask;  // errCorrLvl is uint2, mask is uint3
    uint32_t rem = data;
    for (int i = 0; i < 10; i++) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    
    data = data << 10 | rem;
    data ^= 0x5412;  // uint15
    
    // Draw first copy
    for (uint8_t i = 0; i <= 5; i++) {
        setFunctionModule(modules, isFunction, 8, i, ((data >> i) & 1) != 0);
    }
    
    setFunctionModule(modules, isFunction, 8, 7, ((data >> 6) & 1) != 0);
    setFunctionModule(modules, isFunction, 8, 8, ((data >> 7) & 1) != 0);
    setFunctionModule(modules, isFunction, 7, 8, ((data >> 8) & 1) != 0);
    
    for (int8_t i = 9; i < 15; i++) {
        setFunctionModule(modules, isFunction, 14 - i, 8, ((data >> i) & 1) != 0);
    }
    
    // Draw second copy
    for (int8_t i = 0; i <= 7; i++) {
        setFunctionModule(modules, isFunction, size - 1 - i, 8, ((data >> i) & 1) != 0);
    }
    
    for (int8_t i = 8; i < 15; i++) {
        setFunctionModule(modules, isFunction, 8, size - 15 + i, ((data >> i) & 1) != 0);
    }
    
    setFunctionModule(modules, isFunction, 8, size - 8, true);
}


// Draws two copies of the version bits (with its own error correction code),
// based on this object's version field (which only has an effect for 7 <= version <= 40).
static void drawVersion(BitBucket *modules, BitBucket *isFunction, uint8_t version) {
    
    int8_t size = modules->bitOffsetOrWidth;

#if LOCK_VERSION != 0 && LOCK_VERSION < 7
    return;
    
#else
    if (version < 7) { return; }
    
    // Calculate error correction code and pack bits
    uint32_t rem = version;  // version is uint6, in the range [7, 40]
    for (uint8_t i = 0; i < 12; i++) {
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    }
    
    uint32_t data = version << 12 | rem;  // uint18
    
    // Draw two copies
    for (uint8_t i = 0; i < 18; i++) {
        bool bit = ((data >> i) & 1) != 0;
        uint8_t a = size - 11 + i % 3, b = i / 3;
        setFunctionModule(modules, isFunction, a, b, bit);
        setFunctionModule(modules, isFunction, b, a, bit);
    }
    
#endif
}

static void drawFunctionPatterns(BitBucket *modules, BitBucket *isFunction, uint8_t version, uint8_t ecc) {
    
    uint8_t size = modules->bitOffsetOrWidth;

    // Draw the horizontal and vertical timing patterns
    for (uint8_t i = 0; i < size; i++) {
        setFunctionModule(modules, isFunction, 6, i, i % 2 == 0);
        setFunctionModule(modules, isFunction, i, 6, i % 2 == 0);
    }
    
    // Draw 3 finder patterns (all corners except bottom right; overwrites some timing modules)
    drawFinderPattern(modules, isFunction, 3, 3);
    drawFinderPattern(modules, isFunction, size - 4, 3);
    drawFinderPattern(modules, isFunction, 3, size - 4);
    
#if LOCK_VERSION == 0 || LOCK_VERSION > 1

    if (version > 1) {

        // Draw the numerous alignment patterns
        
        uint8_t alignCount = version / 7 + 2;
        uint8_t step;
        if (version != 32) {
            step = (version * 4 + alignCount * 2 + 1) / (2 * alignCount - 2) * 2;  // ceil((size - 13) / (2*numAlign - 2)) * 2
        } else { // C-C-C-Combo breaker!
            step = 26;
        }
        
        uint8_t alignPositionIndex = alignCount - 1;
        uint8_t alignPosition[alignCount];
        
        alignPosition[0] = 6;
        
        uint8_t size = version * 4 + 17;
        for (uint8_t i = 0, pos = size - 7; i < alignCount - 1; i++, pos -= step) {
            alignPosition[alignPositionIndex--] = pos;
        }
        
        for (uint8_t i = 0; i < alignCount; i++) {
            for (uint8_t j = 0; j < alignCount; j++) {
                if ((i == 0 && j == 0) || (i == 0 && j == alignCount - 1) || (i == alignCount - 1 && j == 0)) {
                    continue;  // Skip the three finder corners
                } else {
                    drawAlignmentPattern(modules, isFunction, alignPosition[i], alignPosition[j]);
                }
            }
        }
    }
    
#endif
    
    // Draw configuration data
    drawFormatBits(modules, isFunction, ecc, 0);  // Dummy mask value; overwritten later in the constructor
    drawVersion(modules, isFunction, version);
}


// Draws the given sequence of 8-bit codewords (data and error correction) onto the entire
// data area of this QR Code symbol. Function modules need to be marked off before this is called.
static void drawCodewords(BitBucket *modules, BitBucket *isFunction, BitBucket *codewords) {
    
    uint32_t bitLength = codewords->bitOffsetOrWidth;
    uint8_t *data = codewords->data;
    
    uint8_t size = modules->bitOffsetOrWidth;
    
    // Bit index into the data
    uint32_t i = 0;
    
    // Do the funny zigzag scan
    for (int16_t right = size - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
        if (right == 6) { right = 5; }
        
        for (uint8_t vert = 0; vert < size; vert++) {  // Vertical counter
            for (int j = 0; j < 2; j++) {
                uint8_t x = right - j;  // Actual x coordinate
                bool upwards = ((right & 2) == 0) ^ (x < 6);
                uint8_t y = upwards ? size - 1 - vert : vert;  // Actual y coordinate
                if (!bb_getBit(isFunction, x, y) && i < bitLength) {
                    bb_setBit(modules, x, y, ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0);
                    i++;
                }
                // If there are any remainder bits (0 to 7), they are already
                // set to 0/false/white when the grid of modules was initialized
            }
        }
    }
}


