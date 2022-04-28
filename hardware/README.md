# Hardware
This folder contains our hardware designs for the matrix.

## Room Controller
The room controller is a ESP32 PCB that features:  
- RS485 Transciever for the LEDs
- Power Switch and Sense
- Environment Sensors
- (Optional) LAN Interface
- (Optional) SD Card slot

## Window Controller
The window controller is a WS2811 PCB that features:
- 2x RS485 Transciever for Data In and Out
- MosFETs to drive ~6m of RGB LED Strip