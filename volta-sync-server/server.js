// VOLTA Sync Relay Server
// Relays pad-trigger events between plugin instances sharing a session code.
// Deploy on Fly.io: see fly.toml

'use strict';
const WebSocket = require('ws');

const PORT = process.env.PORT || 8080;
const wss  = new WebSocket.Server({ port: PORT });

// sessions: code (string) -> Set<WebSocket>
const sessions = new Map();

// Cleanup empty sessions every 60s
setInterval(() => {
  for (const [code, set] of sessions) {
    for (const ws of set) {
      if (ws.readyState !== WebSocket.OPEN) set.delete(ws);
    }
    if (set.size === 0) sessions.delete(code);
  }
}, 60_000);

function broadcast(code, exclude, message) {
  const set = sessions.get(code);
  if (!set) return;
  for (const ws of set) {
    if (ws !== exclude && ws.readyState === WebSocket.OPEN) {
      ws.send(message);
    }
  }
}

function peerCount(code) {
  return sessions.has(code) ? sessions.get(code).size : 0;
}

wss.on('connection', (ws) => {
  let code = null;

  // Keepalive pong
  ws.isAlive = true;
  ws.on('pong', () => { ws.isAlive = true; });

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw); } catch { return; }

    // ── JOIN ─────────────────────────────────────────────
    if (msg.action === 'join' && typeof msg.session === 'string') {
      // Leave previous session if re-joining
      if (code) {
        const prev = sessions.get(code);
        if (prev) {
          prev.delete(ws);
          if (prev.size === 0) sessions.delete(code);
          else broadcast(code, null, JSON.stringify({ action: 'peers', count: prev.size }));
        }
      }

      code = msg.session.toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 8);
      if (!sessions.has(code)) sessions.set(code, new Set());
      sessions.get(code).add(ws);

      const count = peerCount(code);
      ws.send(JSON.stringify({ action: 'joined', session: code, peers: count }));
      broadcast(code, ws, JSON.stringify({ action: 'peers', count }));
      return;
    }

    // ── TRIGGER / STEP / STEPS_SYNC relay ────────────────────
    const RELAY_ACTIONS = new Set(['trigger', 'step', 'steps_sync', 'user_info', 'sample_chunk']);
    if (RELAY_ACTIONS.has(msg.action) && code) {
      broadcast(code, ws, raw.toString());
      return;
    }

    // ── PING ─────────────────────────────────────────────
    if (msg.action === 'ping') {
      ws.send(JSON.stringify({ action: 'pong' }));
      return;
    }
  });

  ws.on('close', () => {
    if (!code) return;
    const set = sessions.get(code);
    if (!set) return;
    set.delete(ws);
    if (set.size === 0) sessions.delete(code);
    else broadcast(code, null, JSON.stringify({ action: 'peers', count: set.size }));
  });

  ws.on('error', () => ws.terminate());
});

// Ping all clients every 30s to detect dead connections
const heartbeat = setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    ws.ping();
  }
}, 30_000);

wss.on('close', () => clearInterval(heartbeat));

console.log(`VOLTA Sync relay listening on port ${PORT}`);
