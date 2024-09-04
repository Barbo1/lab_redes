from jsonrpc_redes import Server


def multiplicacion(a, b):
    if type(a) is not int and type(b) is not int:
        raise TypeError
    return a*b


def suma(a, b):
    if type(a) is not int and type(b) is not int:
        raise TypeError
    return a + b


server = Server(("192.168.1.9", 10101))
server.add_method(multiplicacion)
server.add_method(suma, "sum")
server.serve()
