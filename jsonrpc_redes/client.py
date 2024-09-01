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
        def ret(*args, **keys):
            if method == "":
                raise AttributeError("No hay nombre para el metodo.")
            elif self.address is None:
                raise TypeError("Direccion no encontrada.")
            elif self.address is None:
                raise TypeError("Puerto no encontrada.")
            else:

                # tirar error en caso de que no se tengan los argumentos
                # keyword correctamente escritos.
                if len(keys) > 1 or (len(keys) == 1 and "Notify" not in keys):
                    raise KeyError("Los argumentos Keyword estan mal escritos.")

                # validacion de notificacion
                noti = False
                if "Notify" in keys:
                    if type(keys["Notify"]) is not bool:
                        raise TypeError("Notify debe ser booleano.")
                    else:
                        noti = keys["Notify"]

                # creacion del jsonrpc
                data = {}
                data["jsonrpc"] = self.jsonrpc_version
                data["method"] = method
                args = list(args)
                if len(args) > 0:
                    data["params"] = args
                if not noti:
                    data["id"] = generador_de_ids()

                # creacion del socket cliente
                sock = socket(AF_INET, SOCK_STREAM)
                sock.connect((self.address, self.port))

                # procesamiento y envio de datos.
                data = json.dumps(data)
                data = data.encode()
                sock.sendall(data)

                # retorno de datos.
                if not noti:
                    data = sock.recv(self.buffer_receptor)
                    data = data.decode()
                    data = json.loads(data)
                else:
                    data = None

                # cierre de socket cliente.
                sock.close()

                return data
        return ret


def connect(address: str, port: int) -> Client:
    return Client(address, port)
