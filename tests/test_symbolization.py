import hashlib,json,pathlib,struct,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    path=pathlib.Path(d)/'llvm-pie.elf'
    data=bytearray(120); data[:6]=b'\x7fELF\x02\x01'
    struct.pack_into('<Q',data,32,64);struct.pack_into('<HH',data,54,56,1)
    struct.pack_into('<IIQQQQQQ',data,64,1,5,0,0x400000,0,120,0x1000,0x1000)
    path.write_bytes(data)
    (path.parent/'build.json').write_text(json.dumps({'files':{path.name:hashlib.sha256(data).hexdigest()}}))
    for address in ('0x7ffcb90c0','0x8000003ac','0x80000cc3f'):
        r=subprocess.run(['python3',str(root/'tools/symbolize-crash.py'),str(path),address],capture_output=True,text=True)
        assert r.returncode and 'outside matching ELF' in r.stderr
    path.write_bytes(data+b'tampered')
    r=subprocess.run(['python3',str(root/'tools/symbolize-crash.py'),str(path),'0x400001'],capture_output=True,text=True)
    assert r.returncode and 'checksum' in r.stderr
print('Crash symbolization guards passed: wrong-module addresses and mismatched ELF rejected')
