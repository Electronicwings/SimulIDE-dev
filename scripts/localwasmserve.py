import http.server
import os

PORT = 8080
HEX_FILE = "Blink.ino.hex"   # mock build response

class WasmHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def log_message(self, format, *args):
        print(f"[{self.address_string()}] {format % args}")

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers",
                         "Content-Type, X-Compiler, X-Device, X-Family, X-Extra-Args")
        self.end_headers()

    def do_POST(self):
        if self.path.split("?", 1)[0] == "/api/build":
            self._handle_build()
            return
        self.send_error(404, "Not Found")

    def _handle_build(self):
        length = int(self.headers.get("Content-Length", 0))
        source = self.rfile.read(length) if length else b""

        compiler = self.headers.get("X-Compiler", "")
        device   = self.headers.get("X-Device", "")
        family   = self.headers.get("X-Family", "")
        print(f"[build] compiler={compiler!r} device={device!r} family={family!r} "
              f"source_bytes={len(source)} -> {HEX_FILE}")

        hex_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), HEX_FILE)
        try:
            with open(hex_path, "rb") as f:
                hex_bytes = f.read()
        except FileNotFoundError:
            self.send_error(500, f"Precompiled hex not found: {hex_path}")
            return

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(hex_bytes)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("X-Build-Log", "ok (mock build, returning Blink.ino.hex)")
        self.end_headers()
        self.wfile.write(hex_bytes)

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    with http.server.HTTPServer(("", PORT), WasmHandler) as httpd:
        print(f"Serving at http://localhost:{PORT}/simulide.html")
        print(f"Mock build endpoint: POST http://localhost:{PORT}/api/build  (returns {HEX_FILE})")
        print("Press Ctrl+C to stop.")
        httpd.serve_forever()