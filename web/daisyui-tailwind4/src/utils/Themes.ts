import { Ref, ref } from "vue";

import iconAutoDetect from "@/assets/icons/auto-detect.svg?raw";
import iconMoon from "@/assets/icons/moon.svg?raw";
import iconSun from "@/assets/icons/sun.svg?raw";

interface Theme {
  name: string;
  icon?: string;
}

export class Themes {
  static readonly #storageKey = "theme";
  static readonly defaultTheme = "system"; // Automatická detekcia
  static readonly availableThemes: Theme[] = [
    { name: "system", icon: iconAutoDetect },
    { name: "light", icon: iconSun },
    { name: "dark", icon: iconMoon },
    { name: "cupcake" },
    { name: "retro" },
    { name: "valentine" },
    { name: "business" },
    { name: "coffee" },
    { name: "nord" },
  ];
  static currentTheme: Ref<string> = ref(Themes.detect());

  // Zistí, akú tému by mal použiť systém
  static detect(): string {
    const saved = localStorage.getItem(this.#storageKey) || this.defaultTheme;
    if (
      saved &&
      this.availableThemes.map((theme) => theme.name).includes(saved)
    ) {
      return saved;
    }
    return this.prefersDark() ? "dark" : "light";
  }

  // Aplikuje danú tému a uloží ju do localStorage
  static apply(newTheme: string): void {
    if (newTheme === this.defaultTheme) {
      // Uloží "system", ale vzhled na stránce nastaví podle preferencí
      localStorage.setItem(this.#storageKey, "system");
      document.documentElement.dataset.theme = this.prefersDark()
        ? "dark"
        : "light";
    } else {
      // Uloží zvolenou čitelnou hodnotu, např. "dark" nebo "light"
      document.documentElement.dataset.theme = newTheme;
      localStorage.setItem(this.#storageKey, newTheme);
    }
    this.currentTheme.value = newTheme;
  }

  static prefersDark(): boolean {
    return window.matchMedia("(prefers-color-scheme: dark)").matches;
  }
}
