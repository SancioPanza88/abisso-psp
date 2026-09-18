#!/usr/bin/env node
/*
 * ABISSO — PC <-> PSP bridge
 * --------------------------------------------------------------------
 * Collega il gioco su browser alla partita PSP.
 *
 * La PSP usa il protocollo binario definito in src/net.h (UDP, porta 34567).
 * Questo bridge fa da relay:
 *   - ascolta i pacchetti UDP della/delle PSP;
 *   - li inoltra via WebSocket al browser;
 *   - riceve gli input del browser e li rimanda alle PSP.
 *
 * Uso:
 *   node tools/pc_bridge.js                 # ascolta su 0.0.0.0:34567 / ws://:34569
 *   node tools/pc_bridge.js 192.168.1.50    # inoltra alla PSP all'indirizzo dato
 *
 * Lato PSP: selezionare "Multiplayer Rete (PSP-PC)" e avviare la partita.
 * Lato PC:  aprire tools/pc_client.html (si connette a ws://localhost:34569).
 */

const dgram = require('dgram');
const http = require('http');
const crypto = require('crypto');

const UDP_PORT = 34567;
const WS_PORT = 34569;
const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

/* Se non specificato, il bridge impara l'indirizzo sorgente della prima PSP. */
let pspAddr = process.argv[2] || null;
let pspPort = 34567;

function acceptKey(key) {
  return crypto.createHash('sha1').update(key + GUID).digest('base64');
}

function encodeFrame(payload) {
  const len = payload.length;
  let header;
  if (len < 126) {
    header = Buffer.alloc(2);
    header[0] = 0x82; header[1] = len;
  } else if (len < 65536) {
    header = Buffer.alloc(4);
    header[0] = 0x82; header[1] = 126; header.writeUInt16BE(len, 2);
  } else {
    header = Buffer.alloc(10);
    header[0] = 0x82; header[1] = 127;
    header.writeUInt32BE(0, 2); header.writeUInt32BE(len, 6);
  }
  return Buffer.concat([header, payload]);
}

function decodeFrames(buf) {
  const frames = [];
  let off = 0;
  while (off + 2 <= buf.length) {
    const opcode = buf[off] & 0x0f;
    let len = buf[off + 1] & 0x7f;
    let p = off + 2;
    if (len === 126) { len = buf.readUInt16BE(p); p += 2; }
    else if (len === 127) { len = buf.readUInt32BE(p + 4); p += 8; }
    if (p + len > buf.length) break;
    frames.push({ opcode, payload: buf.slice(p, p + len) });
    off = p + len;
  }
  return frames;
}

const wss = [];
const server = http.createServer((req, res) => { res.writeHead(200); res.end('ABISSO bridge'); });
server.on('upgrade', (req, socket) => {
  const key = req.headers['sec-websocket-key'];
  if (!key) { socket.destroy(); return; }
  socket.write(
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Upgrade: websocket\r\n' +
    'Connection: Upgrade\r\n' +
    'Sec-WebSocket-Accept: ' + acceptKey(key) + '\r\n\r\n'
  );
  wss.push(socket);
  console.log('[ws] client browser connesso');
  socket.on('data', (data) => {
    for (const f of decodeFrames(data)) {
      if (f.opcode === 8) { socket.destroy(); return; }
      if (f.opcode === 1 || f.opcode === 2) relayToPsp(f.payload);
    }
  });
  socket.on('close', () => { const i = wss.indexOf(socket); if (i >= 0) wss.splice(i, 1); });
  socket.on('error', () => {});
});

function broadcastToBrowser(data) {
  const frame = encodeFrame(data);
  for (const s of wss) { try { s.write(frame); } catch (e) {} }
}

function relayToPsp(data) {
  if (!pspAddr) { console.log('[udp] nessuna PSP nota: pacchetto browser ignorato'); return; }
  udp.send(data, pspPort, pspAddr, (err) => { if (err) console.log('[udp] errore:', err.message); });
}

const udp = dgram.createSocket('udp4');
udp.on('message', (msg, rinfo) => {
  if (!pspAddr) {
    pspAddr = rinfo.address; pspPort = rinfo.port;
    console.log('[udp] PSP scoperta: ' + pspAddr + ':' + pspPort);
  }
  broadcastToBrowser(msg);
});
udp.bind(UDP_PORT, () => console.log('[udp] in ascolto su 0.0.0.0:' + UDP_PORT));

server.listen(WS_PORT, () => {
  console.log('[ws ] in ascolto su ws://0.0.0.0:' + WS_PORT);
  console.log('Avvia la partita sulla PSP e collega il browser per il co-op.');
});