from jsonrpc_redes import connect

conn = connect("192.168.1.9", 10103)
m = conn.multiplicacion(4, 5)
assert m == 20

m = conn.sum(6, 6)
assert m == 12

m = conn.sum(a=6, b=3, Notify=True)

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


