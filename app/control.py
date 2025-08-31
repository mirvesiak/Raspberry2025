# control.py
import json
import websocket
from signals import send_signal, ws_connected_event

ws = None  # global websocket object

def start_websocket():
    global ws
    ws = websocket.WebSocketApp(
        "ws://raspberrypi.local:8080/ws",
        on_open=on_open,
        on_message=message_handler,
        on_error=lambda ws, err: print(f"[WS] Error: {err}"),
        on_close=lambda ws, code, msg: print("[WS] Closed")
    )
    ws.run_forever()

def on_open(ws):
    print("[WS] Connected")
    ws_connected_event.set()  # Signal that WebSocket is connected

def send_coords(x, y):
    if ws and ws.sock and ws.sock.connected:
        message = json.dumps({"type": "coords", "x": float(x), "y": float(y)})
        ws.send(message)
        return message
    return "WebSocket not connected."

def send_grabber(state):
    if ws and ws.sock and ws.sock.connected:
        message = json.dumps({"type": "grip", "state": "on" if state else "off"})
        ws.send(message)
        return message
    return "WebSocket not connected."

def message_handler(ws, message):
    send_signal(message)

def close_ws():
    if ws:
        ws.close()
