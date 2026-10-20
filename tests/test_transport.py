#!/usr/bin/env python3
"""Real loopback requests to a local fixture, never a Plex account or real server."""
import http.server
import pathlib
import subprocess
import threading
import sys
import ssl
import tempfile

class Fixture(http.server.BaseHTTPRequestHandler):
    requests = []
    def log_message(self, *_):
        pass
    def do_GET(self):
        self.requests.append(self.path)
        if self.path == '/chunked':
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            self.wfile.write(b'F\r\n{"status":"ok"}\r\n0\r\n\r\n')
            return
        if self.path == '/broken-chunk':
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            self.wfile.write(b'ff\r\n{"status":"ok"}')
            return
        if self.path == '/truncated':
            self.send_response(200)
            self.send_header('Content-Length', '999')
            self.end_headers()
            self.wfile.write(b'{"status":"ok"}')
            return
        if self.path == '/ok':
            assert self.headers['X-Plex-Client-Identifier'] == 'test-device'
            assert self.headers['X-Plex-Token'] == 'test-only-token'
            self.send_response(200)
            body = b'{"status":"ok"}'
        elif self.path == '/denied':
            self.send_response(401)
            body = b'private response must not be logged'
        elif self.path == '/invalid':
            self.send_response(200)
            body = b'{invalid'
        elif self.path == '/redirect':
            self.send_response(302)
            self.send_header('Location', '/should-not-follow')
            body = b''
        elif self.path == '/huge':
            self.send_response(200)
            body = b'x' * (8*1024*1024 + 1)
        else:
            raise AssertionError('Unexpected route')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


with tempfile.TemporaryDirectory(prefix='plexstation-tls-') as directory:
    root = pathlib.Path(directory)
    cert, key = root/'cert.pem', root/'key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', str(key), '-out', str(cert), '-days', '1',
                    '-subj', '/CN=localhost'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Fixture) as server, \
         http.server.ThreadingHTTPServer(('127.0.0.1', 0), Fixture) as secure:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        secure.socket = context.wrap_socket(secure.socket, server_side=True)
        workers = [threading.Thread(target=s.serve_forever, daemon=True) for s in (server, secure)]
        for worker in workers:
            worker.start()
        try:
            repo = pathlib.Path(__file__).resolve().parents[1]
            executable = sys.argv[1] if len(sys.argv)>1 else 'transport-probe'
            subprocess.run([str(repo/'build'/executable), f'http://127.0.0.1:{server.server_port}',
                            f'https://127.0.0.1:{secure.server_port}'], check=True)
            assert '/should-not-follow' not in Fixture.requests
            assert set(Fixture.requests) == {'/ok', '/denied', '/invalid', '/redirect', '/huge', '/chunked', '/broken-chunk', '/truncated'}
        finally:
            server.shutdown()
            secure.shutdown()
            for worker in workers:
                worker.join()
