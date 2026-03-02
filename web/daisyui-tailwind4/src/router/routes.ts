import { RouteRecordRaw } from "vue-router";

import HomeView from "@/views/HomeView.vue";

const routes: RouteRecordRaw[] = [
  {
    path: "/",
    name: "home",
    component: HomeView,
    meta: { title: "Home" },
  },
  {
    path: "/preview",
    name: "componentsPreview",
    // Lazy-loaded route
    component: () => import("@/views/PreviewView.vue"),
    meta: { title: "Components Preview" },
  },
  {
    // 404 fallback
    path: "/:pathMatch(.*)*",
    name: "notFound",
    component: () => import("@/views/NotFoundView.vue"),
    meta: { title: "404 Not Found" },
  },
];

export default routes;
