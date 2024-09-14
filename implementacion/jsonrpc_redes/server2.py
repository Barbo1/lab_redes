from socket import socket, SHUT_RDWR, AF_INET, SOCK_STREAM
import json
from threading import Thread
import logging

# Configure logging
logging.basicConfig(filename='server.log', level=logging.DEBUG,
                    format='%(asctime)s - %(levelname)s - %(message)s')


VERSION_JSON_RPC = "2.0"

def get_json_rpc_right(result, id):
    return {"jsonrpc": VERSION_JSON_RPC, "result": result, "id": id}

def get_json_rpc_error(code=None, id=None):
    messages = {
        -32700: "Parse error",
        -32600: "Invalid Request",
        -32601: "Method not found",
        -32602: "Invalid params",
        -32603: "Internal error"
    }
    message = messages.get(code, "Unknown error")
    return {"jsonrpc": VERSION_JSON_RPC, "error": {"code": code, "message": message}, "id": id}

def validate_json_rpc(json_obj):
    if isinstance(json_obj, dict):
        jsonrpc = json_obj.get("jsonrpc")
        method = json_obj.get("method")
        if jsonrpc == VERSION_JSON_RPC and isinstance(method, str):
            id_present = "id" in json_obj
            params_present = "params" in json_obj
            if id_present and params_present:
                return isinstance(json_obj["params"], (list, dict))
            elif id_present or params_present:
                if params_present:
                    return isinstance(json_obj["params"], (list, dict))
                return True
            else:
                return True
    return False


class Server(object):
    def __init__(self, info):
        self.sock = socket(AF_INET, SOCK_STREAM)
        self.sock.bind(info)
        self.sock.listen(1)
        self.queue_length = 5
        self.buffer_receptor = 2048
        self.SIMPLE_OP = 10

    def receive_data(self, conn, buffer):
        try:
            length_prefix = conn.recv(4)
            if len(length_prefix) < 4:
                logging.error("Server: Failed to receive message length.")
                return None

            message_length = int.from_bytes(length_prefix, byteorder='big')
            logging.info(f"Server: Expected message length: {message_length}")

            data = b""
            while len(data) < message_length:
                chunk = conn.recv(min(buffer, message_length - len(data)))
                if not chunk:
                    logging.warning("Server: Connection closed before receiving full data.")
                    return None
                data += chunk

            logging.info(f"Server: Received data: {data.decode()}")
            return data.decode()
        except Exception as e:
            logging.error(f"Server: An error occurred - {e}")
            return None
       

    def handler(self, conn):
        data = self.receive_data(conn, self.buffer_receptor)
        if data is None:
            return

        try:
            json_rpc = json.loads(data)
            logging.info(f"Funca {json_rpc}")
        except json.decoder.JSONDecodeError:
            response = get_json_rpc_error(-32700)
        else:
            if validate_json_rpc(json_rpc):
                id = json_rpc.get("id")
                method_name = json_rpc.get("method")
                try:
                    method = getattr(self, method_name)
                except AttributeError:
                    response = get_json_rpc_error(-32601, id)
                else:
                    try:
                        params = json_rpc.get("params", [])
                        result = method(*params) if isinstance(params, list) else method(**params)
                        response = get_json_rpc_right(result, id)
                    except TypeError:
                        response = get_json_rpc_error(-32602, id)
                    except Exception:
                        response = get_json_rpc_error(-32603, id)
            else:
                response = get_json_rpc_error(-32600)
        logging.info(f"Funca again {response}")

        if response is not None:
            try:
                response_data = json.dumps(response).encode()
                response_length = len(response_data).to_bytes(4, byteorder='big')
                conn.sendall(response_length + response_data)
            except Exception as e:
                logging.error(f"Server: An error occurred during response sending - {e}")

    def serve(self):
        while True:
            try:
                conn, _ = self.sock.accept()
                conn.settimeout(self.SIMPLE_OP)
                th = Thread(target=self.handler, args=(conn,))
                th.start()
            except Exception as e:
                logging.error(f"Server: An error occurred during connection handling - {e}")

    def add_method(self, function, nombre=None):
        setattr(self, nombre or function.__name__, function)

    def shutdown(self):
        try:
            self.sock.shutdown(SHUT_RDWR)
            self.sock.close()
        except Exception as e:
            logging.error(f"Server: Error during shutdown - {e}")
