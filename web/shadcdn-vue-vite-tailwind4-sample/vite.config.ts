import { execSync } from "node:child_process";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, URL } from "node:url";

import { defineConfig } from 'vite'
import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'
import vueJsx from '@vitejs/plugin-vue-jsx'
import VueDevTools from 'vite-plugin-vue-devtools'
import devtoolsJson from "vite-plugin-devtools-json";

const __dirname = import.meta.dirname;
const rootDirectory = __dirname;
const distributionDirectory = path.resolve(rootDirectory, "dist");
const headerPath = path.resolve(rootDirectory, "../../include/static_assets.h");
// const headerPath = path.resolve(rootDirectory, "static_assets.h");

function getFileHash(filePath: string): string | undefined {
  if (!fs.existsSync(filePath)) return undefined;
  const content = fs.readFileSync(filePath);
  return crypto.createHash("md5").update(content).digest("hex");
}
function runCommand(command: string, arguments_: string[], cwd: string) {
  try {
    execSync(`${command} ${arguments_.join(" ")}`, { cwd, stdio: "inherit" });
  } catch {
    // Error already printed to console via stdio: "inherit"
  }
}
const webFilesPlugin = {
  name: "web-files-plugin",
  async closeBundle() {
    const temporaryHeaderPath = path.resolve(
      rootDirectory,
      `../../include/.${path.basename(headerPath)}.tmp`,
    );
    const oldHash = getFileHash(headerPath);

    runCommand(
      "npx",
      [
        "svelteesp32",
        "-e",
        "webserver",
        "-s",
        path.relative(rootDirectory, distributionDirectory),
        "-o",
        path.relative(rootDirectory, temporaryHeaderPath),
        "--define",
        "SHADCDN_VUEJS_STATIC_ASSETS",
        "--espmethod",
        "initStaticAssets",
        "--gzip",
        "true",
        "--cachetime",
        "86400",
      ],
      rootDirectory,
    );

    const generatedHash = getFileHash(temporaryHeaderPath);

    if (oldHash === generatedHash && fs.existsSync(headerPath)) {
      // Content unchanged - remove temp file and skip
      fs.unlinkSync(temporaryHeaderPath);
      // eslint-disable-next-line no-console
      console.log("ℹ static_assets.h unchanged, skipping write");
    } else {
      // Content changed - replace original with temp file
      fs.renameSync(temporaryHeaderPath, headerPath);
      // eslint-disable-next-line no-console
      console.log("✓ static_assets.h updated");
    }
  },
};

export default defineConfig(({ mode }) => ({
  base: process.env.BASE_URL,

  plugins: [vue(), vueJsx(), tailwindcss(), VueDevTools(), devtoolsJson(), ...(mode === "development" ? [VueDevTools()] : []), webFilesPlugin],
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
  css: {
    devSourcemap: mode === "development",
  },
  resolve: {
    alias: {
//      '@': path.resolve(__dirname, './src'),
      "@": fileURLToPath(new URL("src", import.meta.url)),
    },
  },
}));