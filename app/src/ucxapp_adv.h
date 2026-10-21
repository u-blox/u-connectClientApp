#ifndef UCXAPP_ADV_H
#define UCXAPP_ADV_H

/* Decoding of BLE advertising and scan response data (a sequence of [length][type][value] structures). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Prints every structure on its own line with names for UUIDs, company ids and appearance. */
void ucxapp_adv_print(const uint8_t *data, size_t len, const char *indent);

/** One short line such as "Heart Rate, Battery | Apple iBeacon | Phone"; empty if nothing is known. */
void ucxapp_adv_summary(const uint8_t *data, size_t len, char *out, size_t size);

/** Complete or shortened local name from the data; false if there is none. */
bool ucxapp_adv_local_name(const uint8_t *data, size_t len, char *out, size_t size);

#endif /* UCXAPP_ADV_H */
