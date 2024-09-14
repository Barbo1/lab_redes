from jsonrpc_redes import connect
from socket import socket, AF_INET, SOCK_STREAM
from json import loads

conn = connect("200.0.0.10", 8080)

# prueba de la funcion de multiplicación.
m = conn.multiplicacion(4, 5)
assert m == 20

# prueba de la funcion de suma.
m = conn.sum(6, 6)
assert m == 12

# prueba de las notificaciones.
conn.sum(operando_1=6, operando_2=3, Notify=True)

# falla por parametros incorrectos.
try:
    m = conn.sum(6, "3")
except Exception:
    pass
else:
    raise Exception()

# falla por función inexistente.
try:
    m = conn.suma(6, 3)
except Exception:
    pass
else:
    raise Exception()

# falla por error interno en la función.
try:
    m = conn.ms([0], 1.2)
except Exception:
    pass
else:
    raise Exception()

del conn

conn = connect("200.100.0.15", 8080)

# Prueba de función de regresión lineal. 
m = conn.roots_square(1, -3, 2)
assert m[0] == 1 or m[0] == 2
assert m[1] == 1 or m[1] == 2

# Pruebas de función para multiplicación de matrices cuadradas
m = conn.matrix_mult([[1, 3], [2, 4]], [[5, 7], [6, 8]])
assert m == [[23, 31], [34, 46]]

# Pruebas de función triangulo
m = conn.triangulo(3, 1)
print(m)
m = conn.triangulo(5, 2)
print(m)


def prueba_envio_erroneo(data, message, code):
    print("CLIENT | REQUEST: " + data)
    sock = socket(AF_INET, SOCK_STREAM)
    sock.connect(("200.0.0.10", 8080))
    sock.send(data.encode())
    data = ""
    try:
        while True:
            res = sock.recv(64)
            if not res: break
            data += res.decode()
    except Exception:
        pass
    # muestra antes de parsear el paquete.
    print("CLIENT | RESPONSE: " + data)

    packet = loads(data)
    assert "error" in packet
    assert packet["error"]["message"] == message
    assert packet["error"]["code"] == code


# falla por envio de un json invalido.
prueba_envio_erroneo("hola", "Parse error", -32700)

# falla por envio de un json rpc invalido.
prueba_envio_erroneo("{\"jsonrpc\": \"2.0\", \"method\": \"hola\", \"params\": [1,2], \"madre\": \"padre\"}", "Invalid Request", -32600)


# Fin de los tests
print("Todos los tests se han ejecutado de manera satisfactoria.")
