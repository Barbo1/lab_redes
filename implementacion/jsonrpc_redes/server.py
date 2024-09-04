from socket import socket, SHUT_RDWR, AF_INET, SOCK_STREAM
import json
from threading import Thread

VERSION_JSON_RPC = "2.0"


# funcion que retorna un json rpc el resultado de una operacion.
def get_json_rpc_right(result, id: int) -> dict:
    return {"jsonrpc": VERSION_JSON_RPC, "result": result, "id": id}


# funcion que retorna un json rpc erroneo con el codigo y mensaje
# de error correspondiente.
def get_json_rpc_error(code: int = None) -> dict:
    if code == -32700:
        message = "Parse error"
    elif code == -32600:
        message = "Invalid Request"
    elif code == -32601:
        message = "Method not found"
    elif code == -32602:
        message = "Invalid params"
    elif code == -32603:
        message = "Internal error"

    return {
        "jsonrpc": VERSION_JSON_RPC,
        "error": {"code": code, "message": message},
        "id": "null"
    }


# valida que el objeto json es json rpc correctamente estructurado.
def validate_json_rpc(json: dict) -> bool:
    if type(json) is dict:
        if "jsonrpc" in json and json["jsonrpc"] == VERSION_JSON_RPC and "method" in json and type(json["method"]) is str:
            args = list(json.keys())
            args.remove("jsonrpc")
            args.remove("method")
            if len(args) == 2:
                return "id" in args and "params" in args and type(json["params"]) is list or dict
            if len(args) == 1:
                return "id" in args or "params" in args and type(json["params"]) is list or dict
            if len(args) == 0:
                return True
    return False


class Server(object):
    sock = None                 # socket mediante el cual acepta conexiones.
    queue_length = 5            # nombre de mensajes que pueden haber en la cola de entrada.
    buffer_receptor = 1024      # largo del buffer que acepta un mensaje.

    def __init__(self, info: tuple[str, int]):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind(info)
        self.sock.listen(self.queue_length)

    def handler(self, conn):

        # recepcion de datos
        data = conn.recv(self.buffer_receptor)
        data = data.decode()

        try:

            # validar que los datos recivios representen un json.
            try:
                json_rpc = json.loads(data)
            except json.decoder.JSONDecodeError:
                data = get_json_rpc_error(-32700)
            else:

                # validar que el json sea un json rpc.
                if validate_json_rpc(json_rpc):

                    # validar que el metodo exista.
                    try:
                        method = json_rpc["method"]
                        method = getattr(self, method)
                    except AttributeError:
                        data = get_json_rpc_error(-32601)
                    else:

                        # validar que se puede obtener el resultado.
                        try:
                            args = json_rpc["params"] if "params" in json_rpc else []
                            result = method(*args) if type(args) is list else method(**args)
                        except TypeError:
                            data = get_json_rpc_error(-32602)
                        else:

                            # retorna mensaje en caso de que no sea un notifiación.
                            if "id" in json_rpc:
                                data = get_json_rpc_right(result, json_rpc["id"])
                            else:
                                data = None

                else:
                    data = get_json_rpc_error(-32600)

        except Exception:
            data = get_json_rpc_error(-32603)

        # envio de resultado
        if data is not None:
            data = json.dumps(data)
            data = data.encode()
            conn.sendall(data)
        conn.close()

    def serve(self):
        while exit:
            try:
                conn, _ = self.sock.accept()
                Thread(target=self.handler, args=(conn, )).start()
            except KeyboardInterrupt:
                return

    def add_method(self, function: callable, nombre: str = None):
        if nombre is None:
            setattr(self, function.__name__, function)
        else:
            setattr(self, nombre, function)

    def shutdown(self):
        self.sock.shutdown(SHUT_RDWR)
