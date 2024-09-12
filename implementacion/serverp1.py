from jsonrpc_redes import Server


# Esta función esta pensada para realizar multiplicaciones de dos numeros pero,
# en un principio puede utilizarse para cualquier par de operadores que puedan
# relizar esta operación.
def multiplicacion(operando_1, operando_2):
    return operando_1 * operando_2


# Realiza la suma de dos enteros. En caso de que los parametros no correspondan
# a enteros, ser retornara un error.
def suma(operando_1, operando_2):
    if type(operando_1) is not int:
        raise TypeError
    if type(operando_2) is not int:
        raise TypeError
    return operando_1 + operando_2


# Toma un arreglo de elementos y otro elemento aparte, y retorna la
# multiplicacion del segundo elemento de la lista por el elemento
def muliplicar_second(lista, operando):
    return lista[1] * operando


port = 8080
host = "127.0.0.1"

server = Server((host, port))
server.add_method(multiplicacion)
server.add_method(suma, "sum")
server.add_method(muliplicar_second, "ms")
server.serve()
