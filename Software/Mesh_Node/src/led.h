#include <stdint.h>
#include <stddef.h>

static const int ledTimeout = 1000000; // us

void ledInit();
void ledLoop();
void ledSetData(uint8_t *data, size_t len);
// void ledSetBrightness(uint8_t brightness);