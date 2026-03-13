/**
 * @file useGame.js
 * @brief Composable Vue.js contenant toute la logique du jeu de duel.
 * @ingroup grp_jeu_logic
 * @details Gère l'état réactif des joueurs, la physique des projectiles,
 *          le score basé sur les points de vie, les phases de jeu et
 *          la traduction des événements matériels STM32.
 * @author  Équipe SYS3046
 * @version 1.0
 */

import { reactive, computed } from "vue";

/** @brief Largeur de l'arène en pixels. */
export const ARENA_W = 800;
/** @brief Hauteur de l'arène en pixels. */
export const ARENA_H = 400;
/** @brief Rayon du personnage (cercle de collision) en pixels. */
export const PLAYER_R = 28;
/** @brief Déplacement en pixels par input joystick. */
export const MOVE_STEP = 22;
/** @brief Portée maximale de l'attaque mêlée en pixels. */
export const ATTACK_RANGE = 110;
/** @brief Nombre de LEDs de vie par joueur (aussi nb de divisions de 10 pts). */
export const MAX_HP = 3;
/** @brief Largeur totale de la zone neutre centrale en pixels (40 px de chaque côté). */
export const ZONE_GAP = 80;

/**
 * @brief Composable principal du jeu de duel.
 * @details Crée et retourne l'état réactif ainsi que toutes les fonctions
 *          permettant de piloter une partie : démarrage, déplacement,
 *          tir, gestion du score et des phases.
 * @return {Object} API publique du composable : state, winner, et fonctions de contrôle.
 */
export function useGame() {
  const state = reactive({
    players: [
      {
        id: 1,
        name: "Joueur 1",
        hp: MAX_HP,
        score: MAX_HP * 10,
        x: 160,
        y: ARENA_H / 2,
        facing: 1,
        attacking: false,
        hit: false,
        wins: 0,
      },
      {
        id: 2,
        name: "Joueur 2",
        hp: MAX_HP,
        score: MAX_HP * 10,
        x: ARENA_W - 160,
        y: ARENA_H / 2,
        facing: -1,
        attacking: false,
        hit: false,
        wins: 0,
      },
    ],
    phase: "waiting", // waiting | countdown | playing | finished | restarting
    round: 1,
    countdown: 0,
    winnerId: null,
    elapsedSeconds: 0,
    readyToRestart: false,
    messages: [],
    isConnected: false,
    projectiles: [], // { id, x, y, facing, playerId, hit }
  });

  let countdownTimer = null;

  const winner = computed(() =>
    state.players.find((p) => p.id === state.winnerId) ?? null
  );

  /**
   * @brief Ajoute un message horodaté au journal des événements.
   * @param {string} msg - Message à enregistrer.
   */
  function log(msg) {
    const t = new Date().toLocaleTimeString("fr-FR", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
    });
    state.messages.unshift(`[${t}] ${msg}`);
    if (state.messages.length > 40) state.messages.pop();
  }

  /**
   * @brief Contraint une valeur entre un minimum et un maximum.
   * @param {number} val - Valeur à contraindre.
   * @param {number} min - Borne inférieure.
   * @param {number} max - Borne supérieure.
   * @return {number} Valeur contrainte dans [min, max].
   */
  function clamp(val, min, max) {
    return Math.max(min, Math.min(max, val));
  }

  function dist(a, b) {
    return Math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2);
  }

  /**
   * @brief Retourne l'objet joueur adverse.
   * @param {number} playerId - Identifiant du joueur courant (1 ou 2).
   * @return {Object|undefined} L'objet joueur adverse.
   */
  function getOpponent(playerId) {
    return state.players.find((p) => p.id !== playerId);
  }

  /**
   * @brief Remet les deux joueurs à leur état initial de manche.
   * @details Réinitialise position, score (MAX_HP × 10), hp, orientation,
   *          supprime tous les projectiles en cours et remet le chronomètre à 0.
   */
  function resetPlayers() {
    clearAllProjectiles();
    state.elapsedSeconds = 0;
    Object.assign(state.players[0], {
      hp: MAX_HP,
      score: MAX_HP * 10,
      x: 160,
      y: ARENA_H / 2,
      facing: 1,
      attacking: false,
      hit: false,
    });
    Object.assign(state.players[1], {
      hp: MAX_HP,
      score: MAX_HP * 10,
      x: ARENA_W - 160,
      y: ARENA_H / 2,
      facing: -1,
      attacking: false,
      hit: false,
    });
  }

  /**
   * @brief Lance le compte à rebours 3-2-1 avant le début du combat.
   * @details Passe la phase en "countdown", décrémente chaque seconde,
   *          puis passe en "playing" quand le compteur atteint 0.
   */
  function startCountdown() {
    state.phase = "countdown";
    state.countdown = 3;
    if (countdownTimer) clearInterval(countdownTimer);
    countdownTimer = setInterval(() => {
      state.countdown--;
      if (state.countdown <= 0) {
        clearInterval(countdownTimer);
        countdownTimer = null;
        state.phase = "playing";
        log("COMBAT !");
      }
    }, 1000);
  }

  /**
   * @brief Initialise et lance la manche courante.
   * @details Remet les joueurs à zéro et démarre le compte à rebours.
   */
  function startRound() {
    resetPlayers();
    state.winnerId = null;
    log(`--- Manche ${state.round} ---`);
    startCountdown();
  }

  /**
   * @brief Termine la manche et désigne le vainqueur.
   * @details Passe la phase en "finished", incrémente les victoires du gagnant
   *          et active le prompt de relance après 2,5 secondes.
   * @param {number} winnerId - Identifiant du joueur vainqueur (1 ou 2).
   */
  function endRound(winnerId) {
    if (countdownTimer) {
      clearInterval(countdownTimer);
      countdownTimer = null;
    }
    state.phase = "finished";
    state.winnerId = winnerId;
    state.readyToRestart = false;
    const w = state.players.find((p) => p.id === winnerId);
    if (w) {
      w.wins++;
      log(`${w.name} remporte la manche ${state.round} !`);
    }
    // Affiche le prompt "Appuie sur 1" après 2,5 secondes
    setTimeout(() => {
      if (state.phase === "finished") state.readyToRestart = true;
    }, 2500);
  }

  /**
   * @brief Passe à la manche suivante (incrémente le numéro et relance).
   */
  function nextRound() {
    state.round++;
    startRound();
  }

  /**
   * @brief Déclenche le redémarrage de manche sur pression du bouton "1".
   * @details Ignoré si la phase n'est pas "finished" ou si readyToRestart
   *          est encore false (délai de 2,5 s non écoulé).
   */
  function triggerRestart() {
    if (state.phase !== "finished") return;
    if (!state.readyToRestart) return;
    nextRound();
  }

  /**
   * @brief Réinitialise complètement le match (manche 1, scores et victoires à zéro).
   */
  function resetMatch() {
    if (countdownTimer) clearInterval(countdownTimer);
    state.readyToRestart = false;
    state.round = 1;
    state.messages = [];
    state.players.forEach((p) => {
      p.wins = 0;
      p.score = MAX_HP * 10;
    });
    startRound();
  }

  const HALF = ARENA_W / 2;

  /**
   * @brief Déplace un joueur d'un pas dans la direction indiquée.
   * @details Ignoré si la phase n'est pas "playing". Le joueur est contraint
   *          dans sa demi-arène (séparée par ZONE_GAP) et oriente toujours
   *          son avion vers l'adversaire.
   * @param {number} playerId   - Identifiant du joueur (1 ou 2).
   * @param {string} direction  - Direction : "haut" | "bas" | "gauche" | "droite".
   */
  function movePlayer(playerId, direction) {
    if (state.phase !== "playing") return;
    const p = state.players.find((pl) => pl.id === playerId);
    if (!p || p.hp <= 0) return;

    // Chaque joueur est limité à sa zone — une zone neutre de ZONE_GAP sépare les deux
    const xMin = playerId === 1 ? PLAYER_R + 2                        : HALF + ZONE_GAP / 2 + PLAYER_R + 2;
    const xMax = playerId === 1 ? HALF - ZONE_GAP / 2 - PLAYER_R - 2 : ARENA_W - PLAYER_R - 2;

    switch (direction) {
      case "haut":
        p.y = clamp(p.y - MOVE_STEP, PLAYER_R + 2, ARENA_H - PLAYER_R - 2);
        break;
      case "bas":
        p.y = clamp(p.y + MOVE_STEP, PLAYER_R + 2, ARENA_H - PLAYER_R - 2);
        break;
      case "gauche":
        p.x = clamp(p.x - MOVE_STEP, xMin, xMax);
        break;
      case "droite":
        p.x = clamp(p.x + MOVE_STEP, xMin, xMax);
        break;
    }
    // toujours face à l'adversaire
    const opp = getOpponent(playerId);
    if (opp) p.facing = opp.x >= p.x ? 1 : -1;
  }

  const PROJ_SPEED = 12; // px par tick
  const PROJ_TICK  = 14; // ms par tick (~70 fps)
  const PROJ_HIT_R = PLAYER_R + 10; // rayon de collision

  let projCounter = 0;
  const projIntervals = []; // pour tout nettoyer au reset

  /**
   * @brief Supprime tous les projectiles actifs et annule leurs intervalles.
   */
  function clearAllProjectiles() {
    projIntervals.forEach(clearInterval);
    projIntervals.length = 0;
    state.projectiles.length = 0;
  }

  /**
   * @brief Crée et anime un projectile tiré par l'attaquant.
   * @details Le projectile se déplace à PROJ_SPEED px/tick. À l'impact,
   *          le score de la cible est decrementé de 1 et son hp recalculé.
   *          Si le score atteint 0, endRound() est appelé.
   * @param {Object} atk - Objet joueur attaquant (possède id, x, y, facing, name).
   */
  function spawnProjectile(atk) {
    const id = ++projCounter;
    // Le tir part du bord du personnage, dans la direction qu'il regarde
    const startX = atk.x + atk.facing * (PLAYER_R + 6);
    state.projectiles.push({
      id,
      x: startX,
      y: atk.y,
      facing: atk.facing,
      playerId: atk.id,
      exploding: false,
    });

    const def = getOpponent(atk.id);

    const interval = setInterval(() => {
      const proj = state.projectiles.find((p) => p.id === id);
      if (!proj) { clearInterval(interval); return; }

      // Déplacement du projectile
      proj.x += atk.facing * PROJ_SPEED;

      // Collision avec l'adversaire
      if (def && def.hp > 0 && !proj.exploding) {
        const dx = Math.abs(proj.x - def.x);
        const dy = Math.abs(proj.y - def.y);
        if (dx <= PROJ_HIT_R && dy <= PROJ_HIT_R) {
          proj.exploding = true;
          clearInterval(interval);

          def.score = Math.max(0, def.score - 1);
          def.hp = Math.ceil(def.score / 10);
          def.hit = true;
          log(`${atk.name} touche ${def.name} ! Score: ${def.score}/${MAX_HP * 10} (${def.hp} LED${def.hp > 1 ? "s" : ""})`);
          setTimeout(() => { def.hit = false; }, 450);
          if (def.score <= 0) {
            setTimeout(() => endRound(atk.id), 350);
          }

          // Laisser l'explosion visible brièvement puis supprimer
          setTimeout(() => {
            const idx = state.projectiles.findIndex((p) => p.id === id);
            if (idx !== -1) state.projectiles.splice(idx, 1);
          }, 320);
          return;
        }
      }

      // Hors de l'arène → supprimer
      if (proj.x < -60 || proj.x > ARENA_W + 60) {
        clearInterval(interval);
        const idx = state.projectiles.findIndex((p) => p.id === id);
        if (idx !== -1) state.projectiles.splice(idx, 1);
        log(`${atk.name} attaque... raté !`);
      }
    }, PROJ_TICK);

    projIntervals.push(interval);
  }

  /**
   * @brief Déclenche l'attaque d'un joueur (tir d'un projectile).
   * @details Ignoré si la phase n'est pas "playing" ou si le joueur est éliminé.
   *          L'animation d'attaque dure 350 ms.
   * @param {number} attackerId - Identifiant du joueur qui tire (1 ou 2).
   */
  function attack(attackerId) {
    if (state.phase !== "playing") return;
    const atk = state.players.find((p) => p.id === attackerId);
    if (!atk || atk.hp <= 0) return;

    atk.attacking = true;
    setTimeout(() => { atk.attacking = false; }, 350);

    spawnProjectile(atk);
  }

  /**
   * @brief Table de traduction des directions STM32 (anglais) vers le jeu (français).
   * @type {Object.<string, string>}
   */
  const DIRECTION_MAP = {
    UP:    "haut",
    DOWN:  "bas",
    LEFT:  "gauche",
    RIGHT: "droite",
  };

  /**
   * @brief Point d'entrée unique pour les événements reçus depuis le STM32.
   * @details Traite les événements de type "fire", "joystick" et "tick".
   *          Pendant la phase "finished", "fire" déclenche triggerRestart().
   *          Les directions sont traduites via DIRECTION_MAP avant d'appeler movePlayer().
   *          "tick" du joueur 2 incrémente le chronomètre de 1 seconde.
   * @param {Object} event            - Événement WebSocket désérialisé.
   * @param {string} event.type       - Type : "fire" | "joystick" | "tick".
   * @param {number} event.playerId   - Identifiant de la carte émettrice (1 ou 2).
   * @param {string} [event.direction]- Direction joystick : "UP"|"DOWN"|"LEFT"|"RIGHT".
   */
  function handleHardwareEvent(event) {
    switch (event.type) {
      case "fire":
        if (state.phase === "finished") {
          triggerRestart();
          break;
        }
        attack(event.playerId);
        break;
      case "tick":
        if (event.playerId === 2) {
          state.elapsedSeconds++;
        }
        break;
      case "joystick": {
        const dir = DIRECTION_MAP[event.direction] ?? event.direction;
        movePlayer(event.playerId, dir);
        break;
      }
      default:
        log(`[STM32] Événement inconnu: ${JSON.stringify(event)}`);
    }
  }

  return {
    state,
    winner,
    startRound,
    nextRound,
    triggerRestart,
    resetMatch,
    movePlayer,
    attack,
    handleHardwareEvent,
  };
}
