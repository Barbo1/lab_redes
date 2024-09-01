from socket import *
import json


# funcion que retorna un json rpc el resultado de una operacion.
def get_json_rpc_right(jsonrpc: dict, result, id: int = None) -> dict:
    if id is not None:
        return {"jsonrpc": jsonrpc, "result": result, "id": id}
    else:
        return {"jsonrpc": jsonrpc, "result": result}


# funcion que retorna un json rpc erroneo con el codigo y mensaje
# de error correspondiente.
def get_json_rpc_error(jsonrpc: dict, code: int = None) -> dict:
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
        "jsonrpc": jsonrpc,
        "error": {"code": code, "message": message},
        "id": "null"
    }


# valida que el objeto json es json rpc correctamente estructurado.
def validate_json_rpc(json: dict, json_rpc_version: str) -> bool:
    if type(json) is dict:
        if "jsonrpc" in json and json["jsonrpc"] == json_rpc_version and "method" in json and type(json["method"]) is str:
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
    sock = None
    exit = True
    queue_length = 5
    buffer_receptor = 1024
    json_rpc_version = "2.0"

    def __init__(self, info: tuple[str, int]):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind(info)
        self.sock.listen(self.queue_length)

    def serve(self):
        while exit:

            try:
                conn, _ = self.sock.accept()
            except KeyboardInterrupt:
                return

            # recepcion de datos
            data = conn.recv(self.buffer_receptor)
            data = data.decode()

            try:
                try:
                    json_rpc = json.loads(data)
                except json.decoder.JSONDecodeError:
                    data = get_json_rpc_error(self.json_rpc_version, -32700)
                else:
                    if validate_json_rpc(json_rpc, self.json_rpc_version):
                        try:
                            method = json_rpc["method"]
                            method = getattr(self, method)
                        except AttributeError:
                            data = get_json_rpc_error(self.json_rpc_version, -32601)
                        else:
                            try:
                                args = json_rpc["params"] if "params" in json_rpc else []
                                result = method(*args) if type(args) is list else method(**args)
                            except TypeError:
                                data = get_json_rpc_error(self.json_rpc_version, -32602)
                            else:
                                if "id" in json_rpc:
                                    data = get_json_rpc_right(self.json_rpc_version, result, json_rpc["id"])
                                else:
                                    data = None
                    else:
                        data = get_json_rpc_error("2.0", -32600)
            except Exception:
                data = get_json_rpc_error(self.json_rpc_version, -32603)

            # envio de resultado
            if data is not None:
                data = json.dumps(data)
                data = data.encode()
                conn.sendall(data)
            conn.close()

    def add_method(self, function: callable, nombre: str = None):
        if nombre is None:
            setattr(self, function.__name__, function)
        else:
            setattr(self, nombre, function)

    def shutdown(self):
        self.sock.shutdown(SHUT_RDWR)
        self.exit = False
