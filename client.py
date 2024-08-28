from socket import *
import json
import random


def generador_de_ids():
    return random.randint(0, 10000000000000)


# Clase auxlilar para realizar el llamado al servidor
class Client_call_ret(object):
    client_father = None
    method = None

    def __init__(self, client, method: str):
        self.client_father = client
        self.method = method

    def __call__(self, *args):
        if self.client_father is None:
            raise Exception("Cliente no encontrado.")
        elif self.method == "":
            raise Exception("No hay nomber para el metodo.")
        else:
            self.client_father.sock = socket(AF_INET, SOCK_STREAM)
            self.client_father.sock.connect(
                    (self.client_father.address, self.client_father.port)
                )
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
            data_procesada = json.loads(data_recivida.decode())
            data_procesada = data_procesada["result"]
            self.client_father.sock.close()

            return data_procesada


class Client(object):
    sock = None
    jsonrpc_version = "2.0"
    buffer_receptor = 1024
    address = 0
    port = 0

    def __init__(self, address: str, port: int):
        self.address = address
        self.port = port

    def __getattr__(self, method: str) -> Client_call_ret:
        return Client_call_ret(self, method)


def connect(address: str, port: int) -> Client:
    return Client(address, port)


conn = connect("127.0.0.1", 33333)
resultado = conn.multiplicacion(5, 4)
print(resultado)
resultado = conn.suma(2, 3)
print(resultado)
