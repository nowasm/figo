# Web test server: serves build_web plus /slow (25s) to hold the page's load
# event open, so headless `msedge --dump-dom` captures output printed long
# after load (wasm boot, timers). Usage:
#   copy tools\web_test\clicktest.html build_web\
#   python tools\web_test\serve.py
#   msedge --headless=new --timeout=45000 --dump-dom http://127.0.0.1:8123/clicktest.html
import functools
import http.server
import pathlib
import time

ROOT = pathlib.Path(__file__).resolve().parents[2] / 'build_web'

class H(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/slow':
            time.sleep(25)
            self.send_response(200)
            self.send_header('Content-Type', 'application/javascript')
            self.end_headers()
            self.wfile.write(b'//ok')
        else:
            super().do_GET()

http.server.ThreadingHTTPServer(
    ('127.0.0.1', 8123),
    functools.partial(H, directory=str(ROOT)),
).serve_forever()
