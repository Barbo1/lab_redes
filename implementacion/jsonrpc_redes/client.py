from socket import socket, AF_INET, SOCK_STREAM
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
    buffer_receptor = 64
    address = None
    port = None
    C1 = 0.0009
    C2 = 0.1236

    def __init__(self, address: str, port: int):
        self.address = address
        self.port = port

    def __getattr__(self, method: str) -> callable:
        if method == "":
            raise AttributeError("No hay nombre para el metodo.")

        def ret(*args, **keys):
            noti = False

            # validación de parametros
            if self.address is None:
                raise TypeError("Direccion no encontrada.")

            elif self.port is None:
                raise TypeError("Puerto no encontrada.")

            elif len(args) > 0 and len(keys) > 0:
                raise TypeError("Los parametros no estan correctamente ingresados.")

            elif len(keys) != 0:

                # Validación de notificación.
                if "Notify" in keys:
                    if type(keys["Notify"]) is not bool:
                        raise TypeError("Notify debe ser booleano.")
                    else:
                        noti = keys["Notify"]
                        keys.pop("Notify", None)

                params = dict(keys)

            elif len(args) != 0:
                params = list(args)

            elif len(args) == 0 and len(keys) == 0:
                params = []

            # Creación del jsonrpc.
            data = {}
            data["jsonrpc"] = self.jsonrpc_version
            data["method"] = method
            if len(params) > 0:
                data["params"] = params
            if not noti:
                data["id"] = generador_de_ids()

            # Creación del socket cliente.
            sock = socket(AF_INET, SOCK_STREAM)
            sock.connect((self.address, self.port))

            # procesamiento y envio de datos.
            try:
                sock.settimeout(self.C1)
                data = json.dumps(data)
                data_e = data.encode()
                size = 0
                msglen = len(data_e)
                while size < msglen:
                    size += sock.send(data_e[size:])
            except Exception:
                pass

            print("CLIENT | REQUEST: " + data)

            # recepcion de datos.
            data = ""
            sock.settimeout(self.C2)
            try:
                while True:
                    res = sock.recv(self.buffer_receptor)
                    if not res: break 
                    data += res.decode()
            except Exception:
                pass

            # muestra información en caso de que no sea una notificación.
            if data != "":
                print("CLIENT | RESPONSE: " + data)
                data = json.loads(data)
            else:
                data = None

            # cierre de socket cliente.
            sock.close()

            if data is None and noti:
                return None
            elif data is not None and "error" in data:
                raise ClientException(data["error"]["message"], data["error"]["code"])
            elif data is not None and "result" in data:
                return data["result"]
            else:
                raise ClientException("Ha ocurrido un error inesperado.", 0)
        return ret


def connect(address: str, port: int) -> Client:
    return Client(address, port)
