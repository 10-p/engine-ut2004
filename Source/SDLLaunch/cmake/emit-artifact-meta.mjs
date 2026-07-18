#!/usr/bin/env node
// emit-artifact-meta.mjs — write <out>/meta.json for the just-linked WASM artifact (docs/05 §4).
// Run by SDLLaunch's POST_BUILD, AFTER the .js is finalized (the MAX_TEMP_BUFFER sed), so the js
// checksum is of the served file. Self-contained (node stdlib only — no monorepo dep; the artifact
// + its descriptor travel together across the public/private seam). The monorepo's packages/build
// VALIDATES this against the glue ABI_VERSION + the host loader contract before assembling serve-web.
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join } from 'node:path';

const args = Object.fromEntries(
  process.argv.slice(2).map((a) => { const i = a.indexOf('='); return [a.slice(2, i), a.slice(i + 1)]; })
);
const dir = args.out;
const sha = (f) => existsSync(join(dir, f))
  ? 'sha256-' + createHash('sha256').update(readFileSync(join(dir, f))).digest('hex')
  : null;

const checksums = { wasm: sha('SDLLaunch.wasm'), js: sha('SDLLaunch.js') };
const dataSha = sha('SDLLaunch.data');           // optional (only if --preload-file produced it)
if (dataSha) checksums.data = dataSha;

const meta = {
  targetId: args.target || 'ut2004',
  engineFamily: 'UE2',
  buildConfig: 'wasm-integrated',
  abiVersion: Number(args.abi),                  // from the vendored frozen ue_abi.h UE_ABI_VERSION
  loaderStyle: 'global-module',                  // today's shell loads a classic <script> + bare globals
  provenance: { repo: 'engine-ut2004', gitSha: args.gitSha || 'unknown', builtAt: args.builtAt || 'unknown' },
  toolchain: { emscripten: args.emcc || 'unknown', gl4es: args.gl4es || '10-p/gl4es@fix/cubemap-bind-active-unit' },
  buildFlags: ['-sASYNCIFY', '-sFULL_ES2', '-sALLOW_MEMORY_GROWTH', '-sFORCE_FILESYSTEM'],
  exports: { symbolContract: 1 },
  checksums
};

writeFileSync(join(dir, 'meta.json'), JSON.stringify(meta, null, 2) + '\n');
console.log(`[meta] wrote ${join(dir, 'meta.json')} (abi ${meta.abiVersion}, ${meta.buildConfig}, ${meta.loaderStyle})`);
