from jsonrpc_redes import connect

conn = connect("192.168.1.9", 10101)
m = conn.multiplicacion(4, 5)
assert m == 20
print("Test simple completado.")

m = conn.sum(6, 6)
assert m == 12
print("Test simple completado.")

m = conn.sum(a=6, b=3, Notify=True)
print("Test de notificación completado.")

try:
    m = conn.sum(6, "3")
except Exception as e:
    print("Llamada a método inexistente. Genera excepción necesaria.")
    print(e.code, e.message)
