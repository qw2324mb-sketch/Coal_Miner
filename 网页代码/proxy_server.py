# proxy_server.py
from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.request
import urllib.parse
import ssl

class ProxyHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        print(f"[{self.address_string()}] {format % args}")

    def do_GET(self):
        if self.path.startswith('/api/'):
            # 提取真实 URL: /api/ 后面的部分
            target_url = self.path[5:]
            print(f"代理请求: {target_url}")
            try:
                ctx = ssl.create_default_context()
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
                req = urllib.request.Request(target_url)
                req.add_header('User-Agent', 'Mozilla/5.0')
                with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
                    data = resp.read()
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()
                    self.wfile.write(data)
            except Exception as e:
                print(f"代理错误: {e}")
                self.send_error(500, f'代理请求失败: {str(e)}')
        else:
            # 提供静态文件
            try:
                if self.path == '/':
                    self.path = '/sensor_dashboard.html'
                with open(self.path[1:], 'rb') as f:
                    self.send_response(200)
                    if self.path.endswith('.html'):
                        self.send_header('Content-Type', 'text/html')
                    self.end_headers()
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.send_error(404)

if __name__ == '__main__':
    port = 8000
    server = HTTPServer(('', port), ProxyHandler)
    print(f"代理服务器已启动，端口 {port}")
    print(f"请访问 http://localhost:{port}")
    server.serve_forever()