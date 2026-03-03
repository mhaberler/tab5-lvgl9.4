interface Theme {
  name: string;
  icon?: string;
}

export const defaultTheme: string = "system";
export const availableThemes: Theme[] = [
  {
    name: "system",
    icon: "auto",
  },
  {
    name: "light",
    icon: "sun",
  },
  {
    name: "dark",
    icon: "moon",
  },
  {
    name: "cupcake",
  },
  {
    name: "retro",
  },
  {
    name: "valentine",
  },
  {
    name: "business",
  },
  {
    name: "coffee",
  },
  {
    name: "nord",
  },
];
