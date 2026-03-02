import "./assets/styles/vendor/tailwind.css";
import "./assets/styles/main.scss";

import { createApp } from "vue";

import router from "@/router/index";

import App from "./App.vue";

createApp(App).use(router).mount("#app");
