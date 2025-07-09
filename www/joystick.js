const ws = new WebSocket(`ws://${location.host}/ws`);
ws.onopen   = () => console.log("WS connected");
ws.onerror  = e  => console.error("WS error", e);
ws.onclose  = () => console.warn("WS closed");

function send(cmd) { if (ws.readyState === 1) ws.send(cmd); }

// ---- joystick via nipple.js ----
const pad = nipplejs.create({
  zone: document.getElementById('pad'),
  mode: 'static',
  position: { left: '50%', top: '50%' },
  color: '#39f',
  size: 180
});

pad.on('move', (_, data) => {
  const {angle = {}, distance = 0} = data;
  if (distance < 15) send("0#0");
  else send(angle.degree + "#" + distance);
});

pad.on('end', () => { send("0#0");});
