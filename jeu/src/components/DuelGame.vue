<template>
  <div class="game-root">

    <!-- ── HUD supérieur ── -->
    <div class="hud-top">
      <PlayerStatus :player="state.players[0]" :maxHp="MAX_HP" side="left" />

      <div class="hud-center">
        <div class="timer-label">{{ formattedTime }}</div>
        <div class="round-label">MANCHE {{ state.round }}</div>
        <div class="phase-label">{{ phaseLabel }}</div>
      </div>

      <PlayerStatus :player="state.players[1]" :maxHp="MAX_HP" side="right" />
    </div>

    <!-- ── Arène ── -->
    <div class="arena-wrapper">
      <GameArena
        :players="state.players"
        :projectiles="state.projectiles"
        :phase="state.phase"
        :countdown="state.countdown"
        :winner="winner"
        :readyToRestart="state.readyToRestart"
        :arenaW="ARENA_W"
        :arenaH="ARENA_H"
        :playerR="PLAYER_R"
        :attackRange="ATTACK_RANGE"
        :zoneGap="ZONE_GAP"
      />
    </div>

    <!-- ── Bas de l'écran ── -->
    <div class="game-bottom">

      <!-- Contrôles clavier -->
      <div class="controls-info">
        <div class="ctrl-section">
          <strong class="ctrl-p1">Joueur 1</strong>
          <span>WASD → déplacer</span>
          <span>E → tirer</span>
        </div>

        <div class="ctrl-center">
          <button
            v-if="state.phase === 'waiting'"
            class="btn-primary"
            @click="startRound"
          >
            Démarrer
          </button>
          <button
            v-else-if="state.phase === 'finished'"
            class="btn-primary"
            @click="triggerRestart"
          >
            Manche suivante
          </button>
          <button class="btn-secondary" @click="resetMatch">
            Reset complet
          </button>
          <p :class="['connection-badge', state.isConnected ? 'ok' : 'ko']">
            {{ state.isConnected ? "🟢 STM32 connecté" : "🔴 Mode simulation" }}
          </p>
        </div>

        <div class="ctrl-section ctrl-right">
          <strong class="ctrl-p2">Joueur 2</strong>
          <span>↑ ↓ ← → → déplacer</span>
          <span>/ → tirer</span>
        </div>
      </div>

      <!-- Journal des événements -->
      <div class="event-log">
        <div class="log-title">Journal des événements</div>
        <ul>
          <li v-for="(msg, i) in state.messages" :key="i">{{ msg }}</li>
        </ul>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, watch, onMounted, onUnmounted } from "vue";
import {
  useGame,
  ARENA_W,
  ARENA_H,
  PLAYER_R,
  ATTACK_RANGE,
  MAX_HP,
  ZONE_GAP,
} from "../composables/useGame.js";
import GameArena from "./GameArena.vue";
import PlayerStatus from "./PlayerStatus.vue";

const {
  state,
  winner,
  startRound,
  nextRound,
  triggerRestart,
  resetMatch,
  movePlayer,
  attack,
  handleHardwareEvent,
} = useGame();

const phaseLabel = computed(() => {
  switch (state.phase) {
    case "waiting":   return "EN ATTENTE";
    case "countdown": return state.countdown > 0 ? String(state.countdown) : "GO !";
    case "playing":   return "EN COURS";
    case "finished":  return "TERMINÉ";
    default:          return state.phase.toUpperCase();
  }
});

const formattedTime = computed(() => {
  const total = state.elapsedSeconds;
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  return [h, m, s].map(n => String(n).padStart(2, "0")).join(":");
});

// ── Contrôles clavier ──────────────────────────────────────────
const MOVE_KEYS = new Set([
  "w", "W", "s", "S", "a", "A", "d", "D",
  "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight",
]);

function onKeyDown(e) {
  if (e.repeat && !MOVE_KEYS.has(e.key)) return;

  switch (e.key) {
    // Joueur 1 — WASD + E pour tirer
    case "w": case "W": movePlayer(1, "haut");   break;
    case "s": case "S": movePlayer(1, "bas");    break;
    case "a": case "A": movePlayer(1, "gauche"); break;
    case "d": case "D": movePlayer(1, "droite"); break;
    case "e": case "E": attack(1);               break;

    // Joueur 2 — Flèches + / pour tirer
    case "ArrowUp":    movePlayer(2, "haut");   e.preventDefault(); break;
    case "ArrowDown":  movePlayer(2, "bas");    e.preventDefault(); break;
    case "ArrowLeft":  movePlayer(2, "gauche"); e.preventDefault(); break;
    case "ArrowRight": movePlayer(2, "droite"); e.preventDefault(); break;
    case "/":          attack(2);               break;

    // Contrôles de partie
    case "Enter":
      if (state.phase === "waiting")  startRound();
      else if (state.phase === "finished") triggerRestart();
      break;
    case "Backspace":
      resetMatch();
      break;
  }
}

// Envoie au STM32 l'ordre de biper quand la manche est terminée
watch(
  () => state.phase,
  (phase) => {
    if (phase === "finished") {
      sendToServer({ type: "buzz" });
    }
  }
);

// Envoie 'L' au STM32 quand le hp d'un joueur diminue (pas au reset)
watch(
  () => state.players.map(p => p.hp),
  (newHps, oldHps) => {
    newHps.forEach((hp, i) => {
      if (hp < oldHps[i]) {
        sendLedsUpdate(state.players[i].id, hp);
      }
    });
  }
);

function sendToServer(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

let ws = null;

function connectWebSocket() {
  ws = new WebSocket("ws://localhost:3000");

  ws.onopen = () => {
    state.isConnected = true;
  };

  ws.onclose = () => {
    state.isConnected = false;
    // Reconnexion automatique toutes les 3s si le serveur est coupé
    setTimeout(connectWebSocket, 3000);
  };

  ws.onerror = () => {
    ws.close();
  };

  ws.onmessage = (e) => {
    const msg = JSON.parse(e.data);
    // Synchronisation de l'état de connexion série
    if (msg.type === "serial_status") {
      state.isConnected = msg.connected;
      return;
    }
    handleHardwareEvent(msg);
  };
}

// Envoyer la mise à jour des LEDs au STM32 via le serveur
function sendLedsUpdate(playerId, hp) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: "leds", playerId, hp }));
  }
}

onMounted(() => {
  window.addEventListener("keydown", onKeyDown);
  startRound();
  connectWebSocket();
});

onUnmounted(() => {
  window.removeEventListener("keydown", onKeyDown);
  if (ws) ws.close();
});
</script>
