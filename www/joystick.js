const ws = new WebSocket(`ws://${location.host}/ws`);
ws.onopen   = () => console.log("WS connected");
ws.onerror  = e  => console.error("WS error", e);
ws.onclose  = () => console.warn("WS closed");

function send(cmd) { if (ws.readyState === 1) ws.send(cmd); }

// ---- joystick via nipple.js ----
const PAD_SIZE   = 180;          // px
const PAD_RADIUS = PAD_SIZE / 2; // px

const pad = nipplejs.create({
  zone: document.getElementById('pad'),
  mode: 'static',
  position: { left: '50%', top: '50%' },
  color: '#39f',
  size: PAD_SIZE
});

const dbg = document.getElementById('joy‑debug');

pad.on('move', (_, data) => {
  let deg   = Math.round(data.angle.degree);            // 0‑359
  const norm = Math.min(1, data.distance / PAD_RADIUS);   // 0‑1
  let dst  = Math.round(norm * 100);                    // 0‑100 %

  if (dst < 20) {
    deg = 0;
    dst = 0;
  } 
  send(deg + '#' + dst);
  dbg.textContent = `Angle: ${deg} °     Distance: ${dst} %`;
});

pad.on('end', () => { 
  dbg.textContent = 'Angle: 0 °     Distance: 0 %';
  send("0#0");
});
