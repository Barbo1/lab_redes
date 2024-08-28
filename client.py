from socket import *
import json
import random

def generador_de_ids():
    return random.randint(0, 10000000000000)

class Client(object):
    sock = None
    jsonrpc_version = "2.0"
    buffer_receptor = 1024

    # Clase auxlilar para realizar el llamado al servidor
    class Client_call_ret(object):
        client_father = None
        method = None

        def __init__(self, client, method):
            self.client_father = client
            self.method = method

        def __call__(self, *args):
            if self.client_father is None:
                raise Exception("Cliente no encontrado.")
            elif self.client_father.sock is None:
                raise Exception("Socket no encontrado.")
            elif self.method == "":
                raise Exception("No hay nomber para el metodo.")
            else:
                data_a_enviar = {
                        "jsonrpc": self.client_father.jsonrpc_version,
                        "method": self.method,
                        "params": list(args),
                        "id": generador_de_ids()
                    }
                data_encoded = json.dumps(data_a_enviar).encode()
                self.client_father.sock.send(data_encoded)

                data_recivida = self.client_father.sock.recv(
                        self.client_father.buffer_receptor
                    )
                print(data_recivida)
                data_procesada = json.loads(data_recivida.decode())
                return data_procesada["result"]

    def __init__(self, address, port):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.connect((address, port))

    def __getattr__(self, method):
        return self.Client_call_ret(self, method)

    def __del__(self):
        self.sock.close()

def connect(address: str, port: int)-> Client:
    return Client(address, port)

conn = connect("127.0.0.1", 33334)
resultado = conn.multiplicacion(5, 4)
print(resultado)
resultado = conn.suma(2, 3)
print(resultado)
