import { FlatCompat } from "@eslint/eslintrc";
import js from "@eslint/js";
import typescriptEslint from "@typescript-eslint/eslint-plugin";
import tsParser from "@typescript-eslint/parser";
import simpleImportSort from "eslint-plugin-simple-import-sort";
import unicorn from "eslint-plugin-unicorn";
import vue from "eslint-plugin-vue";
import globals from "globals";
import vueParser from "vue-eslint-parser";

const __dirname = import.meta.dirname;
const compat = new FlatCompat({
  baseDirectory: __dirname,
  recommendedConfig: js.configs.recommended,
  allConfig: js.configs.all,
});

export default [
  {
    ignores: [
      "**/*.cjs",
      "**/.DS_Store",
      "**/node_modules",
      "**/dist",
      "**/.env",
      "**/.env.*",
      "!**/.env.example",
      "**/pnpm-lock.yaml",
      "**/package-lock.json",
      "**/yarn.lock",
    ],
  },
  ...compat.extends(
    "eslint:recommended",
    "plugin:@typescript-eslint/recommended",
    "prettier",
  ),
  ...vue.configs["flat/recommended"],
  unicorn.configs.all,
  {
    plugins: {
      "@typescript-eslint": typescriptEslint,
      "simple-import-sort": simpleImportSort,
    },

    languageOptions: {
      globals: {
        ...globals.browser,
        ...globals.node,
      },

      parser: tsParser,
      ecmaVersion: 2020,
      sourceType: "module",

      parserOptions: {
        extraFileExtensions: [".vue"],
      },
    },

    rules: {
      "simple-import-sort/imports": "error",
      "simple-import-sort/exports": "error",
      "unicorn/filename-case": "off",
      "unicorn/prefer-global-this": "off",
      "no-alert": "error",
      "no-console": "error",
      "no-debugger": "error",
    },
  },
  {
    files: ["**/*.vue"],

    languageOptions: {
      parser: vueParser,
      ecmaVersion: 2020,
      sourceType: "module",

      parserOptions: {
        parser: "@typescript-eslint/parser",
      },
    },
  },
];
