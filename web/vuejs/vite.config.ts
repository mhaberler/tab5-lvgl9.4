import vue from '@vitejs/plugin-vue';
import vueJsx from '@vitejs/plugin-vue-jsx';
import tailwindcss from '@tailwindcss/vite';
import devtoolsJson from 'vite-plugin-devtools-json';
import path from 'path';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [vue(), vueJsx(), tailwindcss(), devtoolsJson()],
	build: {
		target: 'esnext',
		sourcemap: false,
		minify: true,
		cssMinify: true,
		copyPublicDir: true,
		emptyOutDir: true,
		outDir: 'dist',
		chunkSizeWarningLimit: 1500,
		assetsInlineLimit: 0
	},
	base: '',
	resolve: {
		alias: {
			$lib: path.resolve(__dirname, './src/lib')
		}
	}
});
