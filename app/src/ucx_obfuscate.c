#include "ucx_obfuscate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OBFUSCATION_KEY "uBloxUcxClient"

void obfuscatePassword(const char *input, char *output, size_t outputSize)
{
    size_t inputLen = strlen(input);
    size_t keyLen = strlen(OBFUSCATION_KEY);

    if (outputSize == 0) {
        return;
    }
    if (inputLen == 0 || outputSize < 3) {
        output[0] = '\0';
        return;
    }

    size_t outIdx = 0;
    for (size_t i = 0; i < inputLen && outIdx < outputSize - 3; i++) {
        unsigned char c = (unsigned char)input[i] ^ (unsigned char)OBFUSCATION_KEY[i % keyLen];
        snprintf(&output[outIdx], outputSize - outIdx, "%02x", c);
        outIdx += 2;
    }
    output[outIdx] = '\0';
}

void deobfuscatePassword(const char *input, char *output, size_t outputSize)
{
    size_t inputLen = strlen(input);
    size_t keyLen = strlen(OBFUSCATION_KEY);

    if (outputSize == 0) {
        return;
    }
    if (inputLen == 0 || inputLen % 2 != 0) {
        output[0] = '\0';
        return;
    }

    size_t outIdx = 0;
    for (size_t i = 0; i < inputLen && outIdx < outputSize - 1; i += 2) {
        char hexByte[3] = { input[i], input[i + 1], '\0' };
        unsigned char c = (unsigned char)strtol(hexByte, NULL, 16);
        output[outIdx] = (char)(c ^ (unsigned char)OBFUSCATION_KEY[outIdx % keyLen]);
        outIdx++;
    }
    output[outIdx] = '\0';
}
