import { readFile, readdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const currentDirectory = dirname(fileURLToPath(import.meta.url));
const projectDirectory = resolve(currentDirectory, '..');
const distributionDirectory = resolve(projectDirectory, 'outputs/standalone-dist');
const assetDirectory = resolve(distributionDirectory, 'assets');
const outputFile = resolve(projectDirectory, 'outputs/starry-post-mainland.html');

const assetFiles = await readdir(assetDirectory);
const scriptFile = assetFiles.find((file) => file.endsWith('.js'));
const styleFile = assetFiles.find((file) => file.endsWith('.css'));

if (!scriptFile || !styleFile) throw new Error('Standalone build assets are incomplete.');

let html = await readFile(resolve(distributionDirectory, 'index.html'), 'utf8');
const script = (await readFile(resolve(assetDirectory, scriptFile), 'utf8')).replaceAll('</script', '<\\/script');
const style = await readFile(resolve(assetDirectory, styleFile), 'utf8');

html = html.replace(
  /<script type="module" crossorigin src="[^"]+"><\/script>/,
  () => `<script type="module">${script}</script>`,
);
html = html.replace(
  /<link rel="stylesheet" crossorigin href="[^"]+">/,
  () => `<style>${style}</style>`,
);

await writeFile(outputFile, html, 'utf8');
console.log(outputFile);
