from socket import socket, AF_INET, SOCK_STREAM
import json
import random


def generador_de_ids():
    return random.randint(0, 10000000000000)


class ClientException(Exception):
    def __init__(self, message, code):
        self.message = message
        self.code = code
        super().__init__(message)


class Client(object):
    jsonrpc_version = "2.0"
    buffer_receptor = 2048
    address = None                      # IP mediante la cual se realiza la conexión.
    port = None                         # Puerto mediante el cual se realiza la conexión.
    SIMPLE_OP = 10
    COMPLEX_OP = 15

    def __init__(self, address, port):
        self.address = address
        self.port = port
    
    def receive_data(self, sock, buffer):
        try:
            length_prefix = sock.recv(4)
            if len(length_prefix) < 4:
                print("Client: Failed to receive message length.")
                return None

            message_length = int.from_bytes(length_prefix, byteorder='big')
            print(f"Client: Expected message length: {message_length}")

            data = ""
            while len(data) < message_length:
                chunk = sock.recv(min(buffer, message_length - len(data)))
                if not chunk:
                    print("Client: Connection closed before receiving full data.")
                    return None
                data += chunk

            print(f"Client: Received data: {data.decode()}")
            return json.loads(data.decode())
        except Exception as e:
            print(f"Client: An error occurred - {e}")
            return None

    def send_data(self, sock, data, timeout):
        try:
            sock.settimeout(timeout)
            data_str = json.dumps(data)
            data_e = data_str.encode()
            msglen = len(data_e)

            length_prefix = msglen.to_bytes(4, byteorder='big')
            sock.sendall(length_prefix)
            sock.sendall(data_e)
            return data_str
        except TimeoutError:
            print("Client: Timeout occurred.")
            return None
        except Exception as e:
            print(f"Client: An error occurred - {e}")
            return None


    def __getattr__(self, method):
        if method == "":
            raise AttributeError("No method name provided.")

        def ret(*args, **keys):
            noti = False

            if self.address is None or self.port is None:
                raise TypeError("Address or port not set.")

            if len(args) > 0 and len(keys) > 0:
                raise TypeError("Parameters should be either positional or keyword, not both.")

            if len(keys) != 0:
                if "Notify" in keys:
                    if not isinstance(keys["Notify"], bool):
                        raise TypeError("Notify must be a boolean.")
                    noti = keys["Notify"]
                    keys.pop("Notify", None)

                params = dict(keys)
            elif len(args) != 0:
                params = list(args)
            else:
                params = []

            sock = socket(AF_INET, SOCK_STREAM)
            try:
                sock.connect((self.address, self.port))
                data = {"jsonrpc": self.jsonrpc_version, "method": method}
                if params:
                    data["params"] = params
                if not noti:
                    data["id"] = generador_de_ids()

                json_data = self.send_data(sock, data, self.COMPLEX_OP)

                if json_data is None:
                    print("Client: No data sent.")
                    return None
                print("CLIENT | REQUEST: " + json_data)
                print(1)
                response_data = self.receive_data(sock, self.buffer_receptor)
                print("recibido")

                if response_data is None:
                    print("Client: No response received.")
                    return None
                print("CLIENT | RESPONSE: " + str(response_data))

                if response_data is not None and "error" in response_data:
                    raise ClientException(response_data["error"]["message"], response_data["error"]["code"])
                elif response_data is not None and "result" in response_data:
                    return response_data["result"]
                else:
                    raise ClientException("Unexpected error occurred.", -1)
            except Exception as e:
                print(f"Client: An error occurred - {e}")
                raise
            finally:
                sock.close()

        return ret


def connect(address, port):
    return Client(address, port)
