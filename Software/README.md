# Software
This folder contains the software for the matrix

## Mesh_Root
This is the root node of the mesh. It has a UDP Server to recieve packages from a user inside a wifi network and provides the relay to the mesh.

## Mesh_Node
This is a mesh node. It can receive data from the mesh and parse them for the led control.

## PixelBridge
The [pixel bridge](https://github.com/crexodon/PixelBridge) is a fork of [LeoDJ's PixelBridge](https://github.com/LeoDJ/PixelBridge) that contains our contributions for this project. It is responsible for mapping the nodes to the rooms and also is a ArtNET endpoint for animations.

## Trinity
This is a management software that lets users control the matrix easier. It works together with the pixelbridge and also recieves data from the nodes' sensors
To Be Determined