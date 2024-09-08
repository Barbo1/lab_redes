from jsonrpc_redes import Server
from statistics import linear_regression


# evalua el punto en la funcion lineal de la regrecion lineal, obtenida
# mediante los puntos representados por las listas.
def approximation(lista_de_x: list, lista_de_y: list, punto: float):
    if len(lista_de_x) != len(lista_de_y):
        raise TypeError("")
    pendiente, interseccion = linear_regression(lista_de_x, lista_de_y)
    return pendiente*punto + interseccion


# multiplicacion de matrices cuadradas
def matrix_mult(A: list[list], B: list[list]):
    if len(A) != len(B):
        raise TypeError("")

    m = len(A)
    i = 0
    while i < m and m == len(A[i]) and m == len(B[i]):
        i = i + 1
    if i != m:
        raise Exception()

    C = [[0.0 for i in range(0, m)] for i in range(0, m)]
    for i in range(0, m):
        for j in range(0, m):
            for k in range(0, m):
                C[j][i] += A[j][k] * B[k][i]

    return C


# Retorna un string con un triangulo. Toma como parametro la altura de
# la piramide y la cantidad de caracteres extra que tendrá cada nivel
# comparado con el anterior.
# Ejemplo:
#   parametro: height=3, extra=1
#   salida:
#     *
#    ***
#   *****
def triangulo(hegith: int, extra: int):
    ret = ""
    nivel = 1
    hegith_1 = hegith-1
    for i in range(0, hegith):
        ret += extra*hegith_1*" " + nivel*"*" + "\n"
        hegith_1 -= 1
        nivel += 2*extra
    return ret


port = 10105
host = "0.0.0.0"

server = Server((host, port))
server.add_method(matrix_mult)
server.add_method(approximation)
server.add_method(triangulo)
server.serve()
