from socket import *
import json
import random


def generador_de_ids():
    return random.randint(0, 10000000000000)


class Client(object):
    jsonrpc_version = "2.0"
    buffer_receptor = 1024
    address = None
    port = None

    def __init__(self, address: str, port: int):
        self.address = address
        self.port = port

    def __getattr__(self, method: str) -> callable:
        def ret(*args):
            if method == "":
                raise AttributeError("No hay nombre para el metodo.")
            elif self.address is None:
                raise TypeError("Direccion no encontrada.")
            elif self.address is None:
                raise TypeError("Puerto no encontrada.")
            else:
                sock = socket(AF_INET, SOCK_STREAM)
                sock.connect((self.address, self.port))
                data_a_enviar = {
                        "jsonrpc": self.jsonrpc_version,
                        "method": method,
                        "params": list(args),
                        "id": generador_de_ids()
                    }
                data_encoded = json.dumps(data_a_enviar).encode()
                sock.sendall(data_encoded)

                data_recibida = sock.recv(self.buffer_receptor)
                data_procesada = json.loads(data_recibida.decode())
                # data_procesada = data_procesada["result"]
                sock.close()

                return data_procesada
        return ret


def connect(address: str, port: int) -> Client:
    return Client(address, port)


conn = connect("127.0.0.1", 33352)
resultado = conn.multiplicacion(5, 4)
print(resultado)