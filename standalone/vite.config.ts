import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

const currentDirectory = dirname(fileURLToPath(import.meta.url));
const projectDirectory = resolve(currentDirectory, '..');

export default defineConfig({
  root: currentDirectory,
  base: './',
  publicDir: resolve(projectDirectory, 'public'),
  plugins: [react()],
  build: {
    outDir: resolve(projectDirectory, 'outputs/standalone-dist'),
    emptyOutDir: true,
    assetsInlineLimit: 10_000_000,
    sourcemap: false,
  },
});
