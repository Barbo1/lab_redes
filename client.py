from socket import *
import json

class Client(object):
    sock = None
    port = 80

    class Client_call_ret(object):
        sock = None

        def __init__(self, socket):
            self.sock = socket

        def __call__(self, *args):
            if self.sock != None:
                self.sock.send("1".encode())
                data = self.sock.recv(1024)
                print(data.decode())
            else:
                raise Exception("Socket no encontrado.")

    def __init__(self, address, port):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.connect((address, port))
        pass
    
    def __getattr__(self, method):
        return self.Client_call_ret(self.sock)

    def __del__(self):
        self.sock.close()
        
def connect(address: str, port: int) -> Client :
    return Client(address, port)

conn = connect("127.0.0.1", 8080)
a = conn.proc1()