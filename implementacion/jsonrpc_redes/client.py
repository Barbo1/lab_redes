from socket import socket, AF_INET, SOCK_STREAM, timeout
import json
import random


def generador_de_ids():
    return random.randint(0, 10000000000000)


class ClientException(Exception):
    def __init__(self, message, code):
        self.message = message
        self.code = code
        super().__init__("")


class Client(object):
    jsonrpc_version = "2.0"     # Socket mediante el cual acepta conexiones.
    buffer_receptor = 128       # Largo del buffer que acepta un mensaje.
    address = None              # IP mediante la cual se realiza la conexión.
    port = None                 # Puerto mediante el cual se realiza la conexión.
    SIMPLE_OP = 0.575           # Tiempo en milisegundos que espera antes de retornar timeout(cuando espera para enviar). 
    COMPLEX_OP = 1.437          # Tiempo en milisegundos que espera antes de retornar timeout(cuando espera el resultado). 

    def __init__(self, address, port):
        self.address = address
        self.port = port

    def __getattr__(self, method):
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
                if "notify" in keys:
                    if type(keys["notify"]) is not bool:
                        raise TypeError("Notify debe ser booleano.")
                    else:
                        noti = keys["notify"]
                        keys.pop("notify", None)

                params = dict(keys)

            elif len(args) != 0:
                params = list(args)

            elif len(args) == 0 and len(keys) == 0:
                params = []

            # Creación del socket cliente.
            sock = socket(AF_INET, SOCK_STREAM)
            sock.connect((self.address, self.port))

            # Creación del jsonrpc.
            data = {}
            data["jsonrpc"] = self.jsonrpc_version
            data["method"] = method
            if len(params) > 0:
                data["params"] = params
            if not noti:
                data["id"] = generador_de_ids()

            # procesamiento y envio de datos.
            try:
                sock.settimeout(self.SIMPLE_OP)
                data = json.dumps(data)
                data_e = data.encode()
                size = 0
                msglen = len(data_e)
                while size < msglen:
                    size += sock.send(data_e[size:])
            except timeout:
                pass
            except Exception:
                print("ha ocurrido un error.")
                sock.close()
                return

            print("CLIENT | REQUEST: " + data)

            # recepcion de datos.
            data = ""
            try:
                sock.settimeout(self.COMPLEX_OP)
                while True:
                    res = sock.recv(self.buffer_receptor)
                    if not res: break
                    data += res.decode()
            except timeout:
                pass
            except Exception:
                print("ha ocurrido un error.")
                sock.close()
                return

            # muestra información en caso de que no sea una notificación.
            if data != "":
                print("CLIENT | RESPONSE: " + data)
                data = json.loads(data)
            else:
                data = None

            # cierre de socket cliente.
            sock.close()

            # retorno de información.
            if data is None and noti:
                return None
            elif data is not None and "error" in data:
                raise ClientException(data["error"]["message"], data["error"]["code"])
            elif data is not None and "result" in data:
                return data["result"]
            else:
                raise ClientException("Ha ocurrido un error inesperado.", -1)
        return ret


def connect(address, port):
    return Client(address, port)
