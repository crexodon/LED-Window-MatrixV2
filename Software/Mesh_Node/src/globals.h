
static const char *TAG = "mesh_node";


#define LED_PIN         16
#define LED_NUM         32
#define RMT_TX_CHANNEL  RMT_CHANNEL_0


enum LedPayloadCommand {
    CMD_LedData = 1,
};