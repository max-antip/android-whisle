#ifndef ANDROID_WHISLE_BASE32_H


#define ANDROID_WHISLE_BASE32_H
#endif //ANDROID_WHISLE_BASE32_H


#include <string.h>


/*
	Base32 encoding / decoding.
	Encode32 outputs at out bytes with values from 0 to 32 that can be mapped to 32 signs.
	Decode32 input is the output of Encode32. The out parameters should be unsigned char[] of
	length GetDecode32Length(inLen) and GetEncode32Length(inLen) respectively.
    To map the output of Encode32 to an alphabet of 32 characters use Map32.
	To unmap back the output of Map32 to an array understood by Decode32 use Unmap32.
	Both Map32 and Unmap32 do inplace modification of the inout32 array.
	The alpha32 array must be exactly 32 chars long.
*/

struct Base32 {

    static char* toWhistle(const char* text, size_t len);

    static char* fromWhistle(char* text, size_t len);

    static int decode32(const uint8_t *in, size_t inLen, char *out);

    static int encode32(const char *in, size_t inLen, uint8_t *out);

    static size_t getDecode32Length(size_t bytes);

    static size_t getEncode32Length(size_t bytes);

};

