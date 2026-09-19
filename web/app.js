(() => {
  'use strict';

  const runtimeStatus = document.querySelector('#runtime-status');
  const archiveStatus = document.querySelector('#archive-status');
  const fileInput = document.querySelector('#archive-file');
  const filterInput = document.querySelector('#filter');
  const progress = document.querySelector('#load-progress');
  const summary = document.querySelector('#archive-summary');
  const list = document.querySelector('#entry-list');
  let module;
  let entries = [];

  const humanSize = bytes => {
    const units = ['B', 'KiB', 'MiB', 'GiB'];
    let value = Number(bytes);
    let unit = 0;
    while (value >= 1024 && unit < units.length - 1) {
      value /= 1024;
      unit++;
    }
    return `${value.toFixed(unit === 0 ? 0 : 2)} ${units[unit]}`;
  };

  const renderEntries = () => {
    const query = filterInput.value.trim().toLowerCase();
    const visible = query ? entries.filter(item => item.name.toLowerCase().includes(query)) : entries;
    const fragment = document.createDocumentFragment();
    for (const entry of visible.slice(0, 5000)) {
      const item = document.createElement('li');
      item.dataset.index = String(entry.index);
      item.tabIndex = 0;
      item.title = '点击后在 WASM 中解密并解压此条目';
      const name = document.createElement('span');
      const size = document.createElement('span');
      name.textContent = entry.name;
      size.textContent = humanSize(entry.size);
      item.append(name, size);
      fragment.append(item);
    }
    list.replaceChildren(fragment);
    if (visible.length > 5000) {
      const item = document.createElement('li');
      item.textContent = `还有 ${visible.length - 5000} 个条目，请用筛选框缩小范围。`;
      list.append(item);
    }
  };

  const loadArchive = async file => {
    progress.hidden = false;
    progress.value = 0.1;
    archiveStatus.textContent = `正在读取 ${file.name}（${humanSize(file.size)}）……`;
    await new Promise(resolve => requestAnimationFrame(resolve));
    const buffer = await file.arrayBuffer();
    progress.value = 0.35;
    archiveStatus.textContent = '正在把资源交给 WASM 解密并解析……';
    await new Promise(resolve => requestAnimationFrame(resolve));

    const pointer = module._malloc(buffer.byteLength);
    if (!pointer) throw new Error('WASM 内存分配失败');
    try {
      module.HEAPU8.set(new Uint8Array(buffer), pointer);
      progress.value = 0.55;
      const ok = module.ccall('th20_load_archive', 'number', ['number', 'number'], [pointer, buffer.byteLength]);
      if (!ok) {
        throw new Error(module.UTF8ToString(module._th20_last_error()));
      }
    } finally {
      module._free(pointer);
    }

    const count = module.ccall('th20_archive_entry_count', 'number', [], []);
    entries = new Array(count);
    for (let index = 0; index < count; index++) {
      const namePointer = module.ccall('th20_archive_entry_name', 'number', ['number'], [index]);
      entries[index] = {
        index,
        name: module.UTF8ToString(namePointer),
        size: module.ccall('th20_archive_entry_size', 'number', ['number'], [index]) >>> 0
      };
      if ((index & 1023) === 0) progress.value = 0.55 + 0.4 * index / Math.max(1, count);
    }
    progress.value = 1;
    const catalogOffset = module.ccall('th20_archive_catalog_offset', 'number', [], []) >>> 0;
    summary.replaceChildren();
    for (const [label, value] of [
      ['归档条目', count.toLocaleString('zh-CN')],
      ['目录偏移', `0x${catalogOffset.toString(16).toUpperCase()}`],
      ['资源大小', humanSize(file.size)]
    ]) {
      const metric = document.createElement('div');
      const strong = document.createElement('strong');
      const span = document.createElement('span');
      strong.textContent = value;
      span.textContent = label;
      metric.append(strong, span);
      summary.append(metric);
    }
    filterInput.disabled = false;
    renderEntries();
    archiveStatus.textContent = `THA1 资源解析成功：${count.toLocaleString('zh-CN')} 个条目。`;
    setTimeout(() => { progress.hidden = true; }, 500);
  };

  filterInput.addEventListener('input', renderEntries);
  const extractEntry = item => {
    const index = Number(item.dataset.index);
    if (!Number.isInteger(index)) return;
    const ok = module.ccall('th20_extract_archive_entry', 'number', ['number'], [index]);
    if (!ok) {
      archiveStatus.textContent = `解压失败：${module.UTF8ToString(module._th20_last_error())}`;
      return;
    }
    const size = module.ccall('th20_extracted_size', 'number', [], []);
    const hash = module.ccall('th20_extracted_fnv1a', 'number', [], []) >>> 0;
    archiveStatus.textContent = `${entries[index].name} 已由 WASM 解压：${humanSize(size)}，FNV-1a 0x${hash.toString(16).padStart(8, '0').toUpperCase()}。`;
  };
  list.addEventListener('click', event => {
    const item = event.target.closest('li[data-index]');
    if (item) extractEntry(item);
  });
  list.addEventListener('keydown', event => {
    if (event.key !== 'Enter' && event.key !== ' ') return;
    const item = event.target.closest('li[data-index]');
    if (item) {
      event.preventDefault();
      extractEntry(item);
    }
  });
  fileInput.addEventListener('change', async event => {
    const file = event.target.files?.[0];
    if (!file) return;
    fileInput.disabled = true;
    try {
      await loadArchive(file);
    } catch (error) {
      archiveStatus.textContent = `载入失败：${error instanceof Error ? error.message : String(error)}`;
      progress.hidden = true;
      entries = [];
      renderEntries();
    } finally {
      fileInput.disabled = false;
    }
  });

  createTH20Module({
    printErr: message => console.error(message),
    locateFile: path => path
  }).then(instance => {
    module = instance;
    const version = module.UTF8ToString(module._th20_runtime_version());
    runtimeStatus.textContent = `WASM 运行时已就绪 · ${version}`;
    runtimeStatus.classList.add('ready');
    fileInput.disabled = false;
  }).catch(error => {
    runtimeStatus.textContent = `WASM 初始化失败：${error instanceof Error ? error.message : String(error)}`;
    runtimeStatus.classList.add('error');
  });
})();
