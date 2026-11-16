"""Exercise the actual localhost build of usb_files.c with real sockets and files."""
from pathlib import Path
import http.client, json, os, socket, struct, subprocess, tempfile, time, unittest

ROOT=Path(__file__).resolve().parent
PORT=18082

class Files(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.root=Path(cls.temp.name)
  (cls.root/'Projects').mkdir();(cls.root/'Samples').mkdir()
  cls.flag=cls.root/'enabled';cls.flag.touch()
  (cls.root/'Projects'/'test.xpj').write_bytes(b'project')
  (cls.root/'Samples'/'quote"&<.wav').write_bytes(b'example')
  (cls.root/'Projects'/'escape').symlink_to('/etc',target_is_directory=True)
  cls.proc=subprocess.Popen([str(ROOT/'build/usb-files-host'),str(cls.root),str(cls.flag)])
  for _ in range(100):
   if cls.proc.poll() is not None:raise RuntimeError('file server exited')
   try:
    with socket.create_connection(('127.0.0.1',PORT),timeout=.1):break
   except OSError:time.sleep(.02)
  else:raise RuntimeError('file server did not listen')
 @classmethod
 def tearDownClass(cls):
  cls.proc.terminate();cls.proc.wait(timeout=5);cls.temp.cleanup()
 def request(self,method,path,body=None,headers=None):
  conn=http.client.HTTPConnection('127.0.0.1',PORT,timeout=8)
  conn.request(method,path,body=body,headers=headers or {})
  r=conn.getresponse();result=r.status,r.read();conn.close();return result
 def test_browse_download(self):
  code,data=self.request('GET','/list/Projects');self.assertEqual(code,200)
  self.assertEqual(json.loads(data),[{'name':'test.xpj','dir':False}])
  self.assertEqual(self.request('GET','/get/Projects/test.xpj'),(200,b'project'))
  code,data=self.request('GET','/list/Samples');self.assertEqual(code,200)
  self.assertIn({'name':'quote"&<.wav','dir':False},json.loads(data))
 def test_upload_no_overwrite(self):
  wav=b'RIFF'+struct.pack('<I',40)+b'WAVEfmt '+b'\x00'*40
  self.assertEqual(self.request('PUT','/upload/new.wav',wav)[0],201)
  self.assertEqual((self.root/'Samples/User/USB Inbox/new.wav').read_bytes(),wav)
  self.assertEqual(self.request('PUT','/upload/new.wav',wav+b'changed')[0],409)
  self.assertEqual((self.root/'Samples/User/USB Inbox/new.wav').read_bytes(),wav)
  self.assertEqual(self.request('PUT','/upload/not.wav',b'not a wave')[0],400)
  self.assertFalse((self.root/'Samples/User/USB Inbox/not.wav').exists())
  self.assertEqual(list((self.root/'Samples/User/USB Inbox').glob('.usb-upload-*')),[])
 def test_paths(self):
  for path in ['/get/Projects/../etc/passwd','/get/Projects/%2e%2e/etc/passwd','/get/Projects/escape/passwd','/get/Samples/%00.wav','/get/Other/test','/list/Projects/%2f..','/get/Projects/test.xpj%5canything']:
   self.assertIn(self.request('GET',path)[0],(400,404),path)
  for name in ['..%2Fevil.wav','%2Fevil.wav','.hidden.wav','evil.exe','a%00.wav']:
   self.assertEqual(self.request('PUT','/upload/'+name,b'anything')[0],400,name)
 def test_origin_host_and_off(self):
  self.assertEqual(self.request('PUT','/upload/a.wav',b'anything',{'Origin':'https://example.com'})[0],403)
  self.assertEqual(self.request('GET','/',headers={'Host':'evil.example'})[0],403)
  self.flag.unlink()
  try:
   try:self.assertEqual(self.request('GET','/list/Projects')[0],403)
   except OSError:pass # Off also closes the listener.
  finally:self.flag.touch()
  for _ in range(100):
   try:
    if self.request('GET','/list/Projects')[0]==200:break
   except OSError:time.sleep(.02)
  else:self.fail('listener did not return after enabling')
 def test_partial_upload(self):
  with socket.create_connection(('127.0.0.1',PORT)) as sock:
   sock.sendall(b'PUT /upload/partial.wav HTTP/1.1\r\nHost: 127.0.0.1:18082\r\nContent-Length: 100\r\n\r\nRIFF')
   sock.shutdown(socket.SHUT_WR)
   while sock.recv(4096):pass
  self.assertFalse((self.root/'Samples/User/USB Inbox/partial.wav').exists())
  self.assertEqual(list((self.root/'Samples/User/USB Inbox').glob('.usb-upload-*')),[])

if __name__=='__main__':unittest.main(verbosity=2)
