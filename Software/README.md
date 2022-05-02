# Software
This folder contains the software for the matrix

## Mesh_Root
This is the root node of the mesh. It has a UDP Server to recieve packages from a user inside a wifi network and provides the relay to the mesh.

### UDP Protocol
The UDP protocol is still WIP, currently the structure is as follows:

```
 0  1  2  3  4  5  6  7  8  9 10
 
AA BB CC DD EE FF                   Target MAC
                  01                Command
                     00 01 02 ..    Payload
```

#### Commands
Command | Description | Payload | Example
--- | --- | --- | ---
0x01 | Set LED Data | Raw RGB LED data | `01 02 03 04 05 06 01 FF 00 00 00 FF 00 00 00 FF`
0x64 | Reset target node | N/A | `01 02 03 04 05 06 64`
0x65 | Initiate OTA (MAC is currently ignored) | Null terminated string of update URL | `00 00 00 00 00 00 65 http://192.168.1.100:8080/mesh_node.bin 00`

Commands can be easily sent with bash for testing, using echo and nc like this:  
`echo -ne "\x00\x00\x00\x00\x00\x00\x65http://192.168.1.100:8080/mesh_root.bin\x00" | nc -u -w0 192.168.1.42 7715`  


#### OTA Procedure

1. Build firmwares
2. **Upload node firmware to a local test node to see if it's still functional, before deploying to all nodes!**
3. Run `ota_fileserver.py`
4. Execute OTA UDP command with URL of OTA fileserver and path of firmware to update.
5. Depending of the filename of the update file, either the root or all nodes will be updated*

\* This behaviour will probably change in the future

## Mesh_Node
This is a mesh node. It can receive data from the mesh and parse them for the led control.

## PixelBridge
The [pixel bridge](https://github.com/crexodon/PixelBridge) is a fork of [LeoDJ's PixelBridge](https://github.com/LeoDJ/PixelBridge) that contains our contributions for this project. It is responsible for mapping the nodes to the rooms and also is a ArtNET endpoint for animations.

## Trinity
This is a management software that lets users control the matrix easier. It works together with the pixelbridge and also recieves data from the nodes' sensors
To Be Determined