import path from "node:path";

import tailwindcss from "@tailwindcss/vite";
import vue from "@vitejs/plugin-vue";
import vueJsx from "@vitejs/plugin-vue-jsx";
import { defineConfig } from "vite";
import devtoolsJson from "vite-plugin-devtools-json";

const __dirname = import.meta.dirname;

export default defineConfig({
  plugins: [vue(), vueJsx(), tailwindcss(), devtoolsJson()],
  build: {
    target: "esnext",
    sourcemap: false,
    minify: true,
    cssMinify: true,
    copyPublicDir: true,
    emptyOutDir: true,
    outDir: "dist",
    chunkSizeWarningLimit: 1500,
    assetsInlineLimit: 0,
  },
  base: "",
  resolve: {
    alias: {
      $lib: path.resolve(__dirname, "./src/lib"),
    },
  },
});
