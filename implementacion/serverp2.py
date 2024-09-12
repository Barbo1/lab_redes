from jsonrpc_redes import Server
from math import sqrt


# evalua el punto en la funcion lineal de la regrecion lineal, obtenida
# mediante los puntos representados por las listas.
def roots_square(a, b, c):
    r = sqrt(b**2 - 4*a*c)
    return [(-b + r)/(2*a), (-b - r)/(2*a)]


# multiplicacion de matrices cuadradas
def matrix_mult(A, B):
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
def triangulo(hegith, extra):
    ret = ""
    nivel = 1
    hegith_1 = hegith-1
    for i in range(0, hegith):
        ret += extra*hegith_1*" " + nivel*"*" + "\n"
        hegith_1 -= 1
        nivel += 2*extra
    return ret


port = 8081
host = "127.0.0.1"

server = Server((host, port))
server.add_method(matrix_mult)
server.add_method(roots_square)
server.add_method(triangulo)
server.serve()
