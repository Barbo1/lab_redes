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

    def get_json_rpc_right(self, jsonrpc, result, id=None):
        if id is not None:
            return {
                "jsonrpc": jsonrpc,
                "result": result,
                "id": id
            }
        else:
            return {
                "jsonrpc": jsonrpc,
                "result": result
            }

    def get_json_rpc_error(self, jsonrpc, code):
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
        elif code < -32099 and code > -32000:
            message = "Server error"
        return {"jsonrpc": jsonrpc, "error": {"code": code, "message": message}, "id": "null"}

    def validate_json_rpc(self, json):
        # evaluar si esta el campo jsonrpc con un string que representa una version
        # evaluar si esta el campo method con un string que representa el nombre
        # evaluar si esta el campo params con una lista con los parametros
        # evaluar que puede existir id
        # evaluar que no exista otro campo
        return True

    def serve(self):
        while exit:
            conn, _ = self.sock.accept()

            # recepcion de datos
            data = conn.recv(self.buffer_receptor)
            data = data.decode()

            try:
                json_rpc = json.loads(data)

                if self.validate_json_rpc(json_rpc):
                    args = json_rpc["params"]
                    method = json_rpc["method"]

                    try:
                        method = getattr(self, method)

                        try:
                            result = method(*args)
                            if json_rpc["id"] is not None:
                                data_to_send = self.get_json_rpc_right(json_rpc["jsonrpc"], result, json_rpc["id"])
                            else:
                                data_to_send = self.get_json_rpc_right(json_rpc["jsonrpc"], result)
                        except Exception:
                            data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32602)
                    except AttributeError:
                        data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32601)
                else:
                    data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32600)
            except json.decoder.JSONDecodeError:
                data_to_send = self.get_json_rpc_error("2.0", -32700)

            # envio de resultado
            data_to_send = json.dumps(data_to_send)
            data_to_send = data_to_send.encode()
            conn.send(data_to_send)
            conn.close()

    def add_method(self, function):
        setattr(self, function.__name__, function)

    def shutdown():
        self.exit = False


def suma(a, b):
    return a + b


def multiplicacion(a, b):
    return a * b


serv = Server("127.0.0.1", 33342)
serv.add_method(suma)
serv.add_method(multiplicacion)
serv.serve()
