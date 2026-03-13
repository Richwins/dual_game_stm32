/**
 * @file server.js
 * @brief Pont WebSocket ↔ Port série pour relier les cartes STM32 au navigateur.
 * @ingroup grp_jeu_server
 * @details Ce serveur Node.js ouvre un port série par carte STM32 et relaie
 *          les messages dans les deux sens :
 *          - STM32 → navigateur : directions joystick et pression du bouton.
 *          - Navigateur → STM32 : commandes LED ('L') et buzzer ('B').
 *
 * **Protocole série STM32 → serveur :**
 * | Message | Signification                  |
 * |---------|-------------------------------|
 * | "1"     | Bouton de tir pressé           |
 * | "UP"    | Joystick vers le haut          |
 * | "DOWN"  | Joystick vers le bas           |
 * | "LEFT"  | Joystick vers la gauche        |
 * | "RIGHT" | Joystick vers la droite        |
 *
 * **Protocole WebSocket navigateur → serveur :**
 * | type    | Effet                          |
 * |---------|-------------------------------|
 * | "leds"  | Envoie 'L' au STM32 ciblé      |
 * | "buzz"  | Envoie 'B' aux deux STM32      |
 *
 * @author  Équipe SYS3046
 * @version 1.0
 */

const { SerialPort } = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");
const { WebSocketServer } = require("ws");

/**
 * @brief Configuration des ports série pour chaque joueur.
 * @type {Array.<{id: number, path: string, baudRate: number}>}
 */
const PLAYERS = [
  { id: 1, path: "COM6", baudRate: 115200 },
  { id: 2, path: "COM7", baudRate: 115200 }
];

const WS_PORT = 3000;

// ===== WEBSOCKET SERVER =====
const wss = new WebSocketServer({ port: WS_PORT });
const clients = new Set();

console.log("Serveur WebSocket lancé : ws://localhost:" + WS_PORT);

wss.on("connection", (ws) => {
  clients.add(ws);
  console.log("Navigateur connecté");

  ws.on("message", (msg) => {
    const data = JSON.parse(msg);

    if (data.type === "leds") {
      // Envoie "L" à la carte concernée → éteint une LED
      const player = serialPorts.find(p => p.cfg.id === data.playerId);
      if (player) {
        player.port.write("L");
        console.log(`→ STM32 P${data.playerId}: L`);
      } else {
        console.log(`→ STM32 P${data.playerId}: Carte non trouvée`);
      }
    }

    if (data.type === "buzz") {
      // Buzz sur les deux cartes
      serialPorts.forEach(p => p.port.write("B"));
      console.log("→ STM32: BUZZ sur les deux cartes");
    }
  });

  ws.on("close", () => {
    clients.delete(ws);
    console.log("Navigateur déconnecté");
  });
});

// ===== SERIAL PORTS (2 cartes) =====
const serialPorts = PLAYERS.map(cfg => {
  const port = new SerialPort({ path: cfg.path, baudRate: cfg.baudRate });
  const parser = port.pipe(new ReadlineParser({ delimiter: "\n" }));
  const entry = { port, parser, cfg };

  // Quand une carte envoie des données
  parser.on("data", (line) => {
    const msg = line.trim();
    console.log(`STM32 P${cfg.id}:`, msg);

    // Envoyer les infos aux navigateurs connectés
    clients.forEach(ws => {
      switch (msg) {
        case "1":
          ws.send(JSON.stringify({ type: "fire", playerId: cfg.id }));
          break;
        case "T":
          ws.send(JSON.stringify({ type: "tick", playerId: cfg.id }));
          break;
        case "UP":
        case "DOWN":
        case "LEFT":
        case "RIGHT":
          ws.send(JSON.stringify({ type: "joystick", playerId: cfg.id, direction: msg }));
          break;
      }
    });
  });

  port.on("open", () => console.log(`STM32 P${cfg.id} connecté sur ${cfg.path}`));

  return entry;
});