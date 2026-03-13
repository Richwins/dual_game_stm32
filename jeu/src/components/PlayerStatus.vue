<template>
  <div
    class="player-status"
    :class="[`player-status--${side}`]"
  >
    <div class="ps-name">{{ player.name }}</div>

    <div class="ps-wins">
      <span v-for="w in player.wins" :key="w" class="win-star">★</span>
      <span v-for="e in Math.max(0, 2 - player.wins)" :key="'e' + e" class="win-empty">☆</span>
    </div>

    <div class="ps-hp-bar">
      <div
        class="ps-hp-fill"
        :class="hpClass"
        :style="{ width: hpPercent + '%' }"
      />
    </div>

    <div class="ps-lives">
      <span
        v-for="n in maxHp"
        :key="n"
        class="life-led"
        :class="{ 'life-on': n <= player.hp }"
      />
    </div>

    <div class="ps-score">{{ player.score }} / {{ maxHp * 10 }} pts</div>
  </div>
</template>

<script setup>
import { computed } from "vue";

const props = defineProps({
  player: { type: Object, required: true },
  maxHp:  { type: Number, required: true },
  side:   { type: String, default: "left" },
});

const maxScore = computed(() => props.maxHp * 10);

const hpPercent = computed(() => (props.player.score / maxScore.value) * 100);

const hpClass = computed(() => {
  if (hpPercent.value > 60) return "hp-high";
  if (hpPercent.value > 30) return "hp-mid";
  return "hp-low";
});
</script>
