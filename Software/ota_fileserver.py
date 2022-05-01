import http.server
import socketserver

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/mesh_root.bin':
            self.path = 'Mesh_Root/.pio/build/esp32dev/firmware.bin'
        elif self.path == '/mesh_node.bin':
            self.path = 'Mesh_Node/.pio/build/esp32dev/firmware.bin'
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

    def list_directory(self, path): 	# disable directory listing
        self.send_error(403)

# Create an object of the above class
handler_object = MyHttpRequestHandler

PORT = 8080
my_server = socketserver.TCPServer(("", PORT), handler_object)

# Star the server
print(f'Hosting OTA files on port {PORT}')
my_server.serve_forever()