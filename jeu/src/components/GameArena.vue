<template>
  <div class="arena" :style="{ width: arenaW + 'px', height: arenaH + 'px' }">
    <div class="arena-grid" />

    <!-- Zone neutre centrale -->
    <div
      class="arena-neutral-zone"
      :style="{
        left:  (arenaW / 2 - zoneGap / 2) + 'px',
        width: zoneGap + 'px',
      }"
    >
      <div class="neutral-dashes" />
    </div>

    <div class="arena-corner arena-corner--tl" />
    <div class="arena-corner arena-corner--tr" />
    <div class="arena-corner arena-corner--bl" />
    <div class="arena-corner arena-corner--br" />

    <!-- Projectiles -->
    <div
      v-for="proj in projectiles"
      :key="proj.id"
      class="projectile"
      :class="{
        'proj--right':     proj.facing === 1,
        'proj--left':      proj.facing === -1,
        'proj--exploding': proj.exploding,
        [`proj--p${proj.playerId}`]: true,
      }"
      :style="{
        left: (proj.facing === 1 ? proj.x - 54 : proj.x) + 'px',
        top:  proj.y - 10 + 'px',
      }"
    >
      <template v-if="!proj.exploding">
        <!-- Flèche droite : ━━━━▶ -->
        <template v-if="proj.facing === 1">
          <span class="proj-shaft">━━━━</span><span class="proj-head">▶</span>
        </template>
        <!-- Flèche gauche : ◀━━━━ -->
        <template v-else>
          <span class="proj-head">◀</span><span class="proj-shaft">━━━━</span>
        </template>
      </template>
      <!-- Explosion sur impact -->
      <span v-else class="proj-explosion">✸</span>
    </div>

    <div
      v-for="player in players"
      :key="player.id"
      class="character"
      :class="{
        'char--attacking': player.attacking,
        'char--hit':       player.hit,
        'char--dead':      player.hp <= 0,
        [`char--p${player.id}`]: true,
      }"
      :style="{
        left:   player.x - playerR + 'px',
        top:    player.y - playerR + 'px',
        width:  playerR * 2 + 'px',
        height: playerR * 2 + 'px',
      }"
    >
      <!-- Anneau d'attaque -->
      <div
        v-if="player.attacking"
        class="attack-ring"
        :style="{
          width:  attackRange * 2 + 'px',
          height: attackRange * 2 + 'px',
          left:   -(attackRange - playerR) + 'px',
          top:    -(attackRange - playerR) + 'px',
        }"
      />

      <!-- Avion SVG, orienté vers l'adversaire -->
      <svg
        class="char-plane"
        :class="{ 'char-plane--flip': player.facing === -1 }"
        xmlns="http://www.w3.org/2000/svg"
        viewBox="0 0 80 56"
        aria-hidden="true"
      >
        <path d="M76 28 C66 23 50 20 34 20 L12 22 L8 28 L12 34 L34 36 C50 36 66 33 76 28 Z"
              fill="currentColor"/>
        <path d="M40 22 L28 3 L15 19 Z"
              fill="currentColor" opacity="0.88"/>
        <path d="M40 34 L28 53 L15 37 Z"
              fill="currentColor" opacity="0.88"/>
        <path d="M13 22 L8 12 L18 21 Z"
              fill="currentColor" opacity="0.75"/>
        <path d="M13 34 L8 44 L18 35 Z"
              fill="currentColor" opacity="0.75"/>
        <ellipse cx="57" cy="28" rx="9" ry="4"
                 fill="rgba(100,220,255,0.78)" stroke="rgba(255,255,255,0.3)" stroke-width="0.6"/>
        <ellipse cx="9" cy="28" rx="4" ry="5.5"
                 fill="rgba(255,140,30,0.65)"/>
      </svg>

      <!-- Étiquette nom -->
      <div class="char-label">{{ player.name }}</div>
    </div>

    <!-- Overlay countdown -->
    <transition name="fade">
      <div v-if="phase === 'countdown'" class="overlay overlay--countdown">
        <span class="countdown-number" :key="countdown">
          {{ countdown > 0 ? countdown : "GO !" }}
        </span>
      </div>
    </transition>

    <!-- Overlay winner -->
    <transition name="fade">
      <div v-if="phase === 'finished' && winner" class="overlay overlay--winner">
        <div class="winner-title">VICTOIRE !</div>
        <div class="winner-name">{{ winner.name }}</div>
        <transition name="fade">
          <div v-if="readyToRestart" class="winner-prompt">Voulez-vous rejouer ?<br><span class="winner-prompt-hint">Appuie sur 1</span></div>
        </transition>
      </div>
    </transition>

    <!-- Overlay waiting -->
    <transition name="fade">
      <div v-if="phase === 'waiting'" class="overlay overlay--waiting">
        <div class="waiting-text">PRÊT ?</div>
        <div class="waiting-sub">Appuie sur Entrée pour démarrer</div>
      </div>
    </transition>
  </div>
</template>

<script setup>
defineProps({
  players:        { type: Array,   required: true },
  projectiles:    { type: Array,   default: () => [] },
  phase:          { type: String,  required: true },
  countdown:      { type: Number,  default: 0 },
  winner:         { type: Object,  default: null },
  readyToRestart: { type: Boolean, default: false },
  arenaW:         { type: Number,  required: true },
  arenaH:         { type: Number,  required: true },
  playerR:        { type: Number,  required: true },
  attackRange:    { type: Number,  required: true },
  zoneGap:        { type: Number,  default: 80 },
});

</script>
