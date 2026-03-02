import { execSync } from "node:child_process";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, URL } from "node:url";

import vue from "@vitejs/plugin-vue";
import devtoolsJson from "vite-plugin-devtools-json";
import { defineConfig } from "vite";

// https://vitejs.dev/config/

const __dirname = fileURLToPath(new URL(".", import.meta.url));
const rootDirectory = __dirname;
const distributionDirectory = path.resolve(rootDirectory, "dist");
const headerPath = path.resolve(rootDirectory, "../../include/static_assets.h");

function runCommand(command: string, arguments_: string[], cwd: string) {
  try {
    execSync(`${command} ${arguments_.join(" ")}`, { cwd, stdio: "inherit" });
  } catch {
    // Error already printed to console via stdio: "inherit"
  }
}

function getFileHash(filePath: string): string | undefined {
  if (!fs.existsSync(filePath)) return undefined;
  const content = fs.readFileSync(filePath);
  return crypto.createHash("md5").update(content).digest("hex");
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
        "DAISYUI_STATIC_ASSETS",
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

export default defineConfig({
  base: "",
  plugins: [vue(), devtoolsJson(), webFilesPlugin],
  resolve: {
    alias: {
      "@": fileURLToPath(new URL("./src", import.meta.url)),
    },
  },
  build: {
    outDir: "dist", // Change from 'public' to 'dist'
  },
});
