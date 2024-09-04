from socket import *
import json
import random


def generador_de_ids():
    return random.randint(0, 10000000000000)


class ClientException(Exception):
    def __init__(self, message: str, code: int):
        self.message = message
        self.code = code
        super().__init__("")


class Client(object):
    jsonrpc_version = "2.0"
    buffer_receptor = 1024
    address = None
    port = None

    def __init__(self, address: str, port: int):
        self.address = address
        self.port = port

    def __getattr__(self, method: str) -> callable:
        if method == "":
            raise AttributeError("No hay nombre para el metodo.")

        def ret(*args, **keys):
            if self.address is None:
                raise TypeError("Direccion no encontrada.")
            elif self.address is None:
                raise TypeError("Puerto no encontrada.")
            else:

                # validar que los argumentos sean todos nombrados o sin nombre
                if (len(args) == 0) == (len(keys) == 0):
                    raise TypeError("Los parametros no estan correctamente ingresados.")
                elif len(keys) != 0:
                    params = dict(keys)
                elif len(args) != 0:
                    params = list(args)

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
                if len(params) > 0:
                    data["params"] = params
                if not noti:
                    data["id"] = generador_de_ids()

                # creacion del socket cliente
                sock = socket(AF_INET, SOCK_STREAM)
                sock.connect((self.address, self.port))

                # procesamiento y envio de datos.
                data = json.dumps(data)

                print("CLIENT | REQUEST: " + data)

                data = data.encode()
                sock.sendall(data)

                # retorno de datos.
                raise_exep = False
                if not noti:
                    data = sock.recv(self.buffer_receptor)
                    data = data.decode()

                    print("CLIENT | RESPONSE: " + data)

                    data = json.loads(data)

                    if "error" in data:
                        raise_exep = True

                # cierre de socket cliente.
                sock.close()

                if raise_exep:
                    raise ClientException(data["error"]["message"], data["error"]["code"])
                elif not noti:
                    return data["result"]
                else:
                    return None
        return ret


def connect(address: str, port: int) -> Client:
    return Client(address, port)
