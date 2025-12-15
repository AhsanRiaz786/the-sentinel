/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      fontFamily: {
        sans: ["Inter", "system-ui", "sans-serif"],
      },
      colors: {
        surface: {
          900: "#0f172a",
          800: "#111827",
          700: "#1f2937",
        },
        accent: {
          500: "#22d3ee",
          400: "#38bdf8",
          300: "#7dd3fc",
        },
      },
    },
  },
  plugins: [],
};

