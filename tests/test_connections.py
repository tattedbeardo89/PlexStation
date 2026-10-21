#!/usr/bin/env python3
"""Real production GnuTLS/socket client against controlled loopback HTTP/TLS peers."""
import pathlib, socket, socketserver, ssl, subprocess, tempfile, threading, time
repo=pathlib.Path(__file__).resolve().parents[1]
class Peer(socketserver.BaseRequestHandler):
    def handle(self):
        if getattr(self.server,'stall',False): time.sleep(.3); return
        try:
            request=b''
            while b'\r\n\r\n' not in request:
                chunk=self.request.recv(4096)
                if not chunk: return
                request+=chunk
                if request[0]!=ord('G'): return
                if len(request)>65536: return
            # A TLS ClientHello reaching plaintext is intentionally rejected.
            if not request.startswith(b'GET '): return
            path=request.split(b' ')[1]
            if path in (b'/identity',b'/library/sections',b'/fragmented'):
                assert b'X-Plex-Token: fixture-scoped-token' in request
            if path==b'/identity': body=b'<?xml version="1.0"?><MediaContainer machineIdentifier="fixture-server"/>'
            elif path==b'/library/sections': body=b'{"MediaContainer":{"Directory":[]}}'
            else: body=b'{"ok":true}'
            headers=b'HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: '+str(len(body)).encode()+b'\r\n'
            if path==b'/encoding': headers+=b'Content-Encoding: gzip\r\n'
            if path==b'/bad-length': headers=headers.replace(str(len(body)).encode(),b'999')
            response=headers+b'\r\n'+body
            if path==b'/fragmented':
                for i in range(0,len(response),3): self.request.sendall(response[i:i+3]); time.sleep(.001)
            else: self.request.sendall(response)
            # Force the client to use framing, not connection close. Also forces
            # repeated nonblocking E_AGAIN waits in the fragmented paths.
            time.sleep(.4 if path==b'/fragmented' else .03)
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError): pass
class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True; daemon_threads=True
class V6Server(Server): address_family=socket.AF_INET6

def run(cmd): subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
with tempfile.TemporaryDirectory(prefix='plex-connection-fixture-') as directory:
    root=pathlib.Path(directory); (root/'certs').mkdir()
    run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-subj','/CN=PlexStation test CA',
         '-keyout',str(root/'ca.key'),'-out',str(root/'certs/cacert.pem'),'-addext','basicConstraints=critical,CA:TRUE'])
    run(['openssl','req','-newkey','rsa:2048','-nodes','-subj','/CN=fixture.plex.direct',
         '-keyout',str(root/'leaf.key'),'-out',str(root/'leaf.csr')])
    (root/'extensions').write_text('subjectAltName=DNS:fixture.plex.direct,IP:127.0.0.1,IP:::1\nextendedKeyUsage=serverAuth\n')
    run(['openssl','x509','-req','-in',str(root/'leaf.csr'),'-CA',str(root/'certs/cacert.pem'),'-CAkey',str(root/'ca.key'),
         '-CAcreateserial','-days','1','-out',str(root/'leaf.pem'),'-extfile',str(root/'extensions')])
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(root/'leaf.pem',root/'leaf.key')
    sni=[]; context.set_servername_callback(lambda _socket,name,_context:sni.append(name))
    plain=Server(('127.0.0.1',0),Peer); tls=Server(('127.0.0.1',0),Peer); stall=Server(('127.0.0.1',0),Peer)
    stall.stall=True; tls.socket=context.wrap_socket(tls.socket,server_side=True)
    servers=[plain,tls,stall]; ipv6='none'
    try:
        v6=V6Server(('::1',0),Peer); servers.append(v6); ipv6=f'http://[::1]:{v6.server_address[1]}'
    except OSError: print('IPv6 loopback unavailable; IPv6 real-socket check skipped')
    # Bound but not listening: deterministic ECONNREFUSED with no external endpoint.
    refused=socket.socket(); refused.bind(('127.0.0.1',0))
    workers=[threading.Thread(target=s.serve_forever,daemon=True) for s in servers]
    for worker in workers: worker.start()
    try:
        subprocess.run([str(repo/'build/connection-transport-probe'),directory,
            f'http://127.0.0.1:{plain.server_address[1]}',f'https://fixture.plex.direct:{tls.server_address[1]}',
            f'http://127.0.0.1:{stall.server_address[1]}',f'http://127.0.0.1:{refused.getsockname()[1]}',ipv6],check=True,timeout=15)
        assert 'fixture.plex.direct' in sni, sni
    finally:
        refused.close()
        for server in servers: server.shutdown(); server.server_close()
        for worker in workers: worker.join()
