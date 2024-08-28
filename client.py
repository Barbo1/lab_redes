from socket import *
import json
import random

def generador_de_ids():
    return random.randint(0, 10000000000000)

class Client:
    jsonrpc_version = "2.0"
    buffer_receptor = 1024
    
    def __init__(self, address, port):
        self.address = address
        self.port = port

        #Se deberia poner aca lo del socket? sino enrealidad no estas haciendo la conexion con conn = connect(...) que es lo que pide (?) o no? Preguntar?
        #igual poniendo aca solo deja hacer un procedimiento y el otro nunca llega a recibir el server (no me queda claro porque)
        #self.sock = socket(AF_INET, SOCK_STREAM)
        #self.sock.connect((self.address, self.port))

    def __getattr__(self, nombre):
        def method(*args):
            #hacer aca la conexion seria no persistente?????? 
            self.sock = socket(AF_INET, SOCK_STREAM)
            self.sock.connect((self.address, self.port))

            data_a_enviar = {
                    "jsonrpc": self.jsonrpc_version,
                    "method": nombre,
                    "params": args,
                    "id": generador_de_ids()
                }
            data_encoded = json.dumps(data_a_enviar).encode()
            
            self.sock.send(data_encoded)

            data_recibida = self.sock.recv(self.buffer_receptor)
            data_procesada = json.loads(data_recibida.decode())
            self.sock.close()
            return data_procesada
        return method

    def close(self):
        self.sock.close()


def connect(address: str, port: int) -> Client:
    return Client(address, port)

conn = connect('127.0.0.1', 5000)
resultado = conn.suma(5, 4)
print(resultado)
resultado = conn.multiplicacion(5, 4)
print(resultado)
conn.close() # Para que si cerras el sock en el mismo metodo???

























