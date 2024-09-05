from jsonrpc_redes import Server


def multiplicacion(a, b):
    return a*b


def suma(a, b):
    if type(a) is not int and type(b) is not int:
        raise TypeError
    return a + b


def muliplicar_second(a, b):
    return a[1]*b


port = 10103
host = "192.168.1.9"


server = Server((host, port))
server.add_method(multiplicacion)
server.add_method(suma, "sum")
server.add_method(muliplicar_second, "ms")
server.serve()
