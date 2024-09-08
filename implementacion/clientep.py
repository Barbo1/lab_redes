from jsonrpc_redes import connect

conn = connect("127.0.0.1", 10106)
m = conn.multiplicacion(4, 5)
assert m == 20

m = conn.sum(6, 6)
assert m == 12

conn.sum(operando_1=6, operando_2=3, Notify=True)

try:
    m = conn.sum(6, "3")
except Exception:
    pass
else:
    raise Exception()

try:
    m = conn.suma(6, 3)
except Exception:
    pass
else:
    raise Exception()

try:
    m = conn.ms([0], 1.2)
except Exception:
    pass
else:
    raise Exception()

del conn

conn = connect("192.168.1.9", 10105)

# Prueba de función de regresión lineal. 
m = conn.approximation([1, 3.1, 8.3], [10, 2, 4], 2)
assert abs(m - 6.6) < 0.2

# Pruebas de función para multiplicación de matrices cuadradas
m = conn.matrix_mult([[1, 3], [2, 4]], [[5, 7], [6, 8]])
assert m == [[23, 31], [34, 46]]

# Pruebas de función triangulo
m = conn.triangulo(3, 1)
print(m)
m = conn.triangulo(5, 2)
print(m)

# Fin de los tests
print("Todos los tests se han ejecutado de manera satisfactoria.")
