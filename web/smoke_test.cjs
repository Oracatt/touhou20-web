'use strict';

const fs = require('node:fs');
const path = require('node:path');

async function main() {
  const buildDirectory = path.resolve(process.argv[2] || path.join(__dirname, '..', 'build_web'));
  const archivePath = path.resolve(process.argv[3] || 'th20.dat');
  const factory = require(path.join(buildDirectory, 'th20_web.js'));
  const module = await factory({ locateFile: name => path.join(buildDirectory, name) });
  const archive = fs.readFileSync(archivePath);
  const pointer = module._malloc(archive.length);
  if (!pointer) throw new Error('Unable to allocate the archive input in WASM memory');
  try {
    module.HEAPU8.set(archive, pointer);
    const loaded = module.ccall('th20_load_archive', 'number', ['number', 'number'], [pointer, archive.length]);
    if (!loaded) throw new Error(module.UTF8ToString(module._th20_last_error()));
  } finally {
    module._free(pointer);
  }

  const count = module.ccall('th20_archive_entry_count', 'number', [], []);
  if (count < 1) throw new Error('The recovered parser returned an empty archive');
  let probe = -1;
  for (let index = 0; index < count; index++) {
    const namePointer = module.ccall('th20_archive_entry_name', 'number', ['number'], [index]);
    const name = module.UTF8ToString(namePointer);
    if (probe < 0 && /\.(ecl|anm|msg)$/i.test(name)) probe = index;
  }
  if (probe < 0) throw new Error('The archive contains no recovered script resource for the extraction probe');
  const extracted = module.ccall('th20_extract_archive_entry', 'number', ['number'], [probe]);
  if (!extracted) throw new Error(module.UTF8ToString(module._th20_last_error()));
  const size = module.ccall('th20_extracted_size', 'number', [], []);
  if (!size) throw new Error('The extracted script resource is empty');
  const hash = module.ccall('th20_extracted_fnv1a', 'number', [], []) >>> 0;
  process.stdout.write(JSON.stringify({ archivePath, entries: count, probe, size, fnv1a: hash.toString(16).padStart(8, '0') }) + '\n');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
