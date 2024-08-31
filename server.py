from socket import *
import json
import threading

class Server(object):
    sock = None
    exit = True
    queue_length = 5
    buffer_receptor = 1024

    def __init__(self, address, port):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind((address, port))
        self.sock.listen(self.queue_length)

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
        return {"jsonrpc": jsonrpc, "error": {"code": code, "message": message}, "id": "null"}

    def validate_json_rpc(self, json):
        if type(json) == dict:
            if "jsonrpc" in json and json["jsonrpc"] == "2.0" and "method" in json and type(json["method"]) is str:
                args = list(json.keys())
                args.remove("jsonrpc")
                args.remove("method")
                if len(args) == 2:
                    return "id" in args and ("params" in args and type(json["params"]) is list)
                if len(args) == 1:
                    return "id" in args or ("params" in args and type(json["params"]) is list)
                if len(args) == 0:
                    return True
        return False

    def serve(self):
        while exit:
            conn, _ = self.sock.accept()

            # recepcion de datos
            data = conn.recv(self.buffer_receptor)
            data = data.decode()
 
            try:
                try:
                    json_rpc = json.loads(data)
                except json.decoder.JSONDecodeError:
                    data_to_send = self.get_json_rpc_error("2.0", -32700)
                else:
                    if self.validate_json_rpc(json_rpc):
                        args = json_rpc["params"]
                        method = json_rpc["method"]

                        try:
                            method = getattr(self, method)
                        except AttributeError:
                            data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32601)
                        else:
                            try:
                                result = method(*args)
                            except TypeError:
                                data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32602)
                            else:
                                if "id" in json_rpc:
                                    data_to_send = self.get_json_rpc_right(json_rpc["jsonrpc"], result, json_rpc["id"])
                                else:
                                    data_to_send = None
                    else:
                        data_to_send = self.get_json_rpc_error("2.0", -32600)    
            except Exception:
                data_to_send = self.get_json_rpc_error(json_rpc["jsonrpc"], -32603)

            # envio de resultado
            if data_to_send is not None:
                data_to_send = json.dumps(data_to_send).encode()
                conn.sendall(data_to_send)
                
            conn.close()
            
    def add_method(self, function):
        setattr(self, function.__name__, function)

    def shutdown(self):
        self.sock.close()
        self.exit = False


def suma(a, b):
    if type(a) != int or type(b) != int: raise TypeError("")
    return a + b


def multiplicacion(a, b):
    return a * b


serv = Server("127.0.0.1", 33352)
serv.add_method(suma)
serv.add_method(multiplicacion)
serv.serve()
