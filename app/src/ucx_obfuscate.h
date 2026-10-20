#ifndef UCX_OBFUSCATE_H
#define UCX_OBFUSCATE_H

#include <stddef.h>

/* XOR + hex obfuscation for credentials stored in the settings file.
 * Not cryptographically secure; it only avoids plaintext passwords on disk. */

void obfuscatePassword(const char *input, char *output, size_t outputSize);
void deobfuscatePassword(const char *input, char *output, size_t outputSize);

#endif /* UCX_OBFUSCATE_H */
