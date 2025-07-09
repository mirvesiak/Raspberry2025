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

let lastDir = '';
pad.on('move', (_, data) => {
  const {angle = {}, distance = 0} = data;
  if (distance < 15) return;
  send(angle.degree + "#" + distance);
  // const deg = angle.degree;
  // const dir = (deg > 45 && deg <= 135)   ? 'forward'
  //          : (deg > 135 && deg <= 225)  ? 'left'
  //          : (deg > 225 && deg <= 315)  ? 'back'
  //          :                              'right';

  // if (dir !== lastDir) { send(dir); lastDir = dir; }
});

pad.on('end', () => { send('stop'); lastDir = ''; });
