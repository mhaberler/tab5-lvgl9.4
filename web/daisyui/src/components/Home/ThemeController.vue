<script setup lang="ts">
import { Themes } from "@/utils/Themes";

const availableThemes = Themes.availableThemes;
const theme = Themes.currentTheme;

const setTheme = (newTheme: string) => {
  Themes.apply(newTheme);
};
</script>

<template>
  <div class="mb-8">
    <h2>Theme management implemented. You can change the theme:</h2>
    <div class="flex flex-wrap gap-2 items-center justify-center">
      <div
        v-for="t in availableThemes"
        :key="t.name"
        class="flex"
      >
        <label
          class="btn btn-ghost"
          :class="{ 'btn-active': theme === t.name }"
        >
          <span
            v-if="t.icon"
            v-html="t.icon"
          />
          {{ t.name }}
          <input
            type="radio"
            name="theme-buttons"
            class="theme-controller hidden"
            :aria-label="t.name"
            :value="t.name"
            :checked="theme === t.name"
            @change="setTheme(t.name)"
          >
        </label>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss"></style>
