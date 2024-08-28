from socket import *

class Server(object):
    sock = None
    exit = True

    def __init__(self, address, port):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind((address, port)) 
        self.sock.listen(1)
    
    def serve(self):
        while exit:
            conn, _ = self.sock.accept()
            ret = conn.recv(1024).decode()
            x = "0"
            if (ret == "1"):
                x = "2"
            else:
                print(ret)
            
            conn.send(x.encode())
            conn.close()
        self.sock.close()

    def shutdown():
        exit = False

serv = Server("127.0.0.1", 8080)
serv.serve()