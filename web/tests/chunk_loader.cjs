// Run with: node web/tests/chunk_loader.cjs
// Executes the production page script with only an export hook and browser mocks.
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { createHash, webcrypto } = require('node:crypto');

const sourcePath = path.resolve(__dirname, '../game.js');
const source = readFileSync(sourcePath, 'utf8');
const exportPoint = '  function mountData(){';
assert.equal(source.split(exportPoint).length, 2, 'production export hook must be unique');
const instrumented = source.replace(exportPoint,
  '  globalThis.loaderUnderTest={loadManifest,fetchChunkedData,fetchData};\n' + exportPoint);
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const parts = [Uint8Array.from([0, 255, 17]), Uint8Array.from([128, 9, 0, 42])];
const expectedBytes = parts.flatMap(bytes => Array.from(bytes));
const defaultManifest = () => ({
  version: 1,
  files: [{
    name: 'th20.dat', size: expectedBytes.length,
    chunks: parts.map((bytes, index) => ({ path: `th20.${index}.bin`, size: bytes.length, sha256: hash(bytes) }))
  }]
});

function harness({
  manifestPath = 'assets/resource-manifest.json',
  baseURI = 'https://example.github.io/touhou20-web/game.html',
  manifest = defaultManifest(),
  chunkResponse,
  directResponse,
  manifestStatus = 200
} = {}) {
  const requests = [];
  const progress = { value: 0 };
  const detail = { textContent: '' };
  const status = { textContent: '' };
  const canvas = { focus() {} };
  const manifestURL = manifestPath ? new URL(manifestPath, baseURI).href : null;
  const nodes = { '#game-canvas': canvas, '#status': status, '#detail': detail, '#progress': progress };
  const sandbox = {
    URL, Uint8Array, crypto: webcrypto, console,
    document: {
      baseURI, body: { classList: { add() {} } }, addEventListener() {},
      querySelector(selector) {
        if (selector === 'script[data-resource-manifest]') {
          return manifestPath ? { dataset: { resourceManifest: manifestPath } } : null;
        }
        assert.ok(Object.hasOwn(nodes, selector), `unexpected DOM selector: ${selector}`);
        return nodes[selector];
      }
    },
    window: {}, navigator: {}, addEventListener() {}, requestAnimationFrame() {},
    createTH20Game: async options => options,
    async fetch(input, options = {}) {
      const url = new URL(input, baseURI).href;
      const request = { input: String(input), url, cache: options.cache };
      requests.push(request);
      if (url === manifestURL) {
        return { ok: manifestStatus === 200, status: manifestStatus, json: async () => manifest };
      }
      if (!manifestPath && url === new URL('game-data/th20.dat', baseURI).href && directResponse) {
        return directResponse(request);
      }
      const index = parts.findIndex((_, i) => url === new URL(`th20.${i}.bin`, manifestURL).href);
      assert.notEqual(index, -1, `unexpected resource request: ${url}`);
      const attempt = requests.filter(item => item.url === url).length;
      return chunkResponse ? chunkResponse(index, attempt, request) : binaryResponse(parts[index]);
    }
  };
  vm.runInNewContext(instrumented, sandbox, { filename: sourcePath });
  return { ...sandbox.loaderUnderTest, requests, progress, detail, status, manifestURL };
}

function binaryResponse(bytes, { status = 200, streaming = false, contentLength = true } = {}) {
  const copy = Uint8Array.from(bytes);
  let read = false;
  return {
    ok: status === 200, status,
    headers: { get: name => name === 'content-length' && contentLength ? String(copy.length) : null },
    arrayBuffer: async () => copy.buffer,
    body: streaming ? { getReader: () => ({ read: async () => {
      if (read) return { done: true };
      read = true;
      return { done: false, value: copy };
    } }) } : null
  };
}

function streamResponse(pieces, { onRead = () => {}, onCancel = () => {} } = {}) {
  let next = 0;
  return {
    ok: true, status: 200,
    arrayBuffer: async () => { throw new Error('stream response must use its reader'); },
    body: { getReader: () => ({
      async read() {
        onRead(next);
        if (next === pieces.length) return { done: true };
        return { done: false, value: Uint8Array.from(pieces[next++]) };
      },
      async cancel() { onCancel(); }
    }) }
  };
}

const cases = [];
const check = (name, run) => cases.push({ name, run });

check('production fetchData joins verified chunks without changing bytes', async () => {
  const test = harness();
  assert.deepEqual(Array.from(await test.fetchData('th20.dat', 2, 2)), expectedBytes);
  assert.equal(test.progress.value, 1);
  assert.equal(test.requests.length, 3);
});

check('streamed chunks report intermediate progress and reassemble verified bytes', async () => {
  const observed = [];
  const test = harness({ chunkResponse: index => streamResponse([
    parts[index].subarray(0, 1), parts[index].subarray(1)
  ], { onRead: () => observed.push(test.progress.value) }) });
  assert.deepEqual(Array.from(await test.fetchData('th20.dat', 1, 1)), expectedBytes);
  // The reader sees progress from the preceding network piece before completion.
  assert.deepEqual(observed, [0, 1 / 7, 3 / 7, 3 / 7, 4 / 7, 1]);
  assert.equal(test.progress.value, 1);
  assert.equal(test.requests.length, 3);
});

check('streamed chunks still reject bad checksums after three attempts', async () => {
  const test = harness({ chunkResponse: () => streamResponse([[1], [2, 3]]) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /资源校验失败/);
  assert.deepEqual(test.requests.slice(1).map(request => request.cache), ['default', 'reload', 'reload']);
  assert.equal(test.requests.length, 4);
});

check('streamed short reads reject after three attempts without downloading the next chunk', async () => {
  let reads = 0;
  const test = harness({ chunkResponse: () => streamResponse([[0], [255]], { onRead: () => ++reads }) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /下载大小不匹配/);
  assert.equal(reads, 9);
  assert.equal(test.requests.length, 4);
  assert.ok(test.requests.slice(1).every(request => request.url.endsWith('/th20.0.bin')));
  assert.equal(test.progress.value, 2 / 7);
});

check('streamed oversized pieces cancel each reader before rejecting', async () => {
  let reads = 0;
  let cancelled = 0;
  const test = harness({ chunkResponse: () => streamResponse([[0], [255, 17, 42], [99]], {
    onRead: () => ++reads, onCancel: () => ++cancelled
  }) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /下载大小不匹配/);
  assert.equal(cancelled, 3);
  assert.equal(reads, 6, 'must stop before reading the remainder of an oversized response');
  assert.equal(test.requests.length, 4);
  assert.equal(test.progress.value, 1 / 7);
});

check('chunk paths stay relative to the manifest on nested GitHub Pages routes', async () => {
  const test = harness({ manifestPath: '../data/manifest.json', baseURI: 'https://example.github.io/touhou20-web/play/game.html' });
  await test.fetchData('th20.dat', 1, 2);
  assert.deepEqual(test.requests.map(request => request.url), [
    'https://example.github.io/touhou20-web/data/manifest.json',
    'https://example.github.io/touhou20-web/data/th20.0.bin',
    'https://example.github.io/touhou20-web/data/th20.1.bin'
  ]);
  assert.equal(test.progress.value, 0.5);
});

check('parallel and subsequent consumers reuse one manifest download', async () => {
  const test = harness();
  await Promise.all([test.loadManifest(), test.loadManifest()]);
  await test.fetchData('th20.dat', 1, 1);
  assert.equal(test.requests.filter(request => request.url === test.manifestURL).length, 1);
});

check('bad checksum retries exactly three times, then rejects', async () => {
  const test = harness({ chunkResponse: () => binaryResponse([1, 2, 3]) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /资源校验失败/);
  assert.deepEqual(test.requests.slice(1).map(request => request.cache), ['default', 'reload', 'reload']);
  assert.equal(test.progress.value, 0);
});

check('a verified retry recovers and resumes remaining chunks', async () => {
  const test = harness({ chunkResponse: (index, attempt) => binaryResponse(index === 0 && attempt < 3 ? [1, 2, 3] : parts[index]) });
  assert.deepEqual(Array.from(await test.fetchData('th20.dat', 1, 1)), expectedBytes);
  assert.equal(test.requests.length, 5);
});

check('short chunks retry three times and never enter the output', async () => {
  const test = harness({ chunkResponse: () => binaryResponse([0, 255]) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /下载大小不匹配/);
  assert.equal(test.requests.length, 4);
  assert.equal(test.progress.value, 0);
});

check('missing file entries fail before any chunk download', async () => {
  const test = harness();
  await assert.rejects(test.fetchData('missing.dat', 1, 1), /资源清单不完整/);
  assert.equal(test.requests.length, 1);
});

for (const badPath of ['../part.bin', 'nested/part.bin', 'https://other.example/part.bin', '%2e%2e.bin']) {
  check(`invalid chunk path is rejected: ${badPath}`, async () => {
    const manifest = defaultManifest();
    manifest.files[0].chunks[0].path = badPath;
    const test = harness({ manifest });
    await assert.rejects(test.fetchData('th20.dat', 1, 1), /无效的资源分块/);
    assert.equal(test.requests.length, 1);
  });
}

check('chunk total shorter than the declared file size rejects', async () => {
  const manifest = defaultManifest();
  manifest.files[0].size += 1;
  const test = harness({ manifest });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /资源分块总长度不匹配/);
});

check('chunk total exceeding the declared file size rejects before overflow', async () => {
  const manifest = defaultManifest();
  manifest.files[0].size -= 1;
  const test = harness({ manifest });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /无效的资源分块/);
  assert.equal(test.requests.length, 2);
});

check('unsupported manifest version rejects', async () => {
  const manifest = defaultManifest();
  manifest.version = 2;
  await assert.rejects(harness({ manifest }).fetchData('th20.dat', 1, 1), /资源清单格式不支持/);
});

check('manifest HTTP errors retain the status code', async () => {
  await assert.rejects(harness({ manifestStatus: 404 }).fetchData('th20.dat', 1, 1), /资源清单: HTTP 404/);
});

for (const streaming of [false, true]) {
  check(`no manifest uses the original direct download (${streaming ? 'streaming' : 'arrayBuffer'})`, async () => {
    const test = harness({ manifestPath: null, directResponse: () => binaryResponse(expectedBytes, { streaming }) });
    assert.deepEqual(Array.from(await test.fetchData('th20.dat', 1, 1)), expectedBytes);
    assert.deepEqual(test.requests.map(request => request.input), ['game-data/th20.dat']);
    assert.equal(test.progress.value, 1);
  });
}

check('direct streaming still works without a content-length header', async () => {
  const test = harness({ manifestPath: null, directResponse: () => binaryResponse(expectedBytes, { streaming: true, contentLength: false }) });
  assert.deepEqual(Array.from(await test.fetchData('th20.dat', 1, 1)), expectedBytes);
  assert.equal(test.requests.length, 1);
  assert.match(test.detail.textContent, /\? MiB$/);
});

check('direct download HTTP failure remains visible', async () => {
  const test = harness({ manifestPath: null, directResponse: () => binaryResponse([], { status: 404 }) });
  await assert.rejects(test.fetchData('th20.dat', 1, 1), /th20\.dat: HTTP 404/);
  assert.equal(test.requests.length, 1);
});

(async () => {
  let passed = 0;
  for (const { name, run } of cases) {
    try { await run(); ++passed; }
    catch (error) { console.error(`FAIL: ${name}\n${error.stack}`); }
  }
  console.log(`Chunk loader: ${passed}/${cases.length} checks passed (production web/game.js).`);
  if (passed !== cases.length) process.exitCode = 1;
})();
