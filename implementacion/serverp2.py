from jsonrpc_redes import Server
from statistics import linear_regression
from math import floor

port = 10113
host = "192.168.1.9"


# evalua el punto en la funcion lineal de la regrecion lineal, obtenida
# mediante la lista de puntos.
def approximation(lista_de_x: list, lista_de_y: list, punto: float):
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


# retorna un string con un triangulo, ejemplo:
# parametro: h=3, s=1
# salida:
#   *
#  ***
# *****
def triangulo(h: int, s: int):
    ret = ""
    c = 1
    h_1 = h-1
    for i in range(0, h):
        ret += s*h_1*" " + c*"*" + "\n"
        h_1 -= 1
        c += 2*s
    return ret


server = Server((host, port))
server.add_method(matrix_mult)
server.add_method(approximation)
server.add_method(triangulo)
server.serve()
