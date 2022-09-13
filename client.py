import sys
import time
import socket            
 
s = socket.socket()        
port = int(sys.argv[1])
 
# connect to the server on local computer
s.connect(('127.0.0.1', port))

print(s.recv(1024))

for i in range(10):
    s.send(b"put 70")
    print(s.recv(1024))
    s.send(b"get 68")
    print(s.recv(1024))
    time.sleep(10000)

s.close()
