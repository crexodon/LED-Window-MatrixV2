#include <stdint.h>

typedef struct {
    uint8_t mac[6];
    uint8_t command;
    uint8_t data[];
} ledPayload_t;

static const uint8_t ledPayloadHeaderSize = sizeof(ledPayload_t);

enum LedPayloadCommand {
    CMD_LedData = 1,
    // CMD_Brightness // implement in PixelBridge? Don't have global brightness feature in LEDs anyways
    CMD_Restart = 100,
    CMD_OTA,
};

void udpInit();