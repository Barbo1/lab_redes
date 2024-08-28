from socket import *
import json

class Server(object):
    sock = None
    exit = True
    buffer_receptor = 1024

    def __init__(self, address, port):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind((address, port))
        self.sock.listen(1)

    def serve(self):
        while exit:
            conn, _ = self.sock.accept()

            # estraccion de data
            data_devuelta = conn.recv(self.buffer_receptor).decode()
            data_procesada = json.loads(data_devuelta)
            args = data_procesada["params"]
            method = data_procesada["method"]

            # procesamiento de resultado
            result = getattr(self, method)(*args)

            # envio de resultado
            data_to_send = {
                    "jsonrpc": data_procesada["jsonrpc"],
                    "result": result,
                    "id": data_procesada["id"]
                }
            conn.send(json.dumps(data_to_send).encode())
            conn.close()
        self.sock.close()

    def add_method(self, function):
        setattr(self, function.__name__, function)

    def shutdown():
        self.exit = False


def suma(a, b):
    return a + b

def multiplicacion(a, b):
    return a * b

serv = Server("127.0.0.1", 33333)
serv.add_method(suma)
serv.add_method(multiplicacion)
serv.serve()
