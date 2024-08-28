
class Client(object):
    def __init__(self, address, host):
        # pedirle al servidor los metodos que tiene
        # añadir los metodos al objeto conection resultante
        # 
        return self
    
    def __call__(self, method, param):
        print(param)
    
def connect(address: str, port: int) -> Client :
    return Client(address, port)

conn = connect("127.0.0.1", 32)
conn.proc1(1)

