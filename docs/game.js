(() => {
  const canvas=document.querySelector('#game-canvas');
  const status=document.querySelector('#status');
  const detail=document.querySelector('#detail');
  const progress=document.querySelector('#progress');
  const manifestPath=document.querySelector('script[data-resource-manifest]')?.dataset.resourceManifest;
  let resourceManifest=null;
  let runtime=null;

  const setStatus=(text,sub)=>{status.textContent=text;if(sub!==undefined)detail.textContent=sub;};
  addEventListener('error',event=>{console.error('TH20 page error',event.error?.stack||event.message);setStatus('游戏运行错误',event.error?.stack||event.message);});
  addEventListener('unhandledrejection',event=>{console.error('TH20 rejected promise',event.reason?.stack||event.reason);setStatus('游戏运行错误',event.reason?.stack||String(event.reason));});
  async function loadManifest(){
    if(!resourceManifest)resourceManifest=(async()=>{
      const response=await fetch(manifestPath);
      if(!response.ok)throw new Error(`资源清单: HTTP ${response.status}`);
      const manifest=await response.json();
      if(manifest.version!==1||!Array.isArray(manifest.files))throw new Error('资源清单格式不支持');
      return manifest;
    })();
    return resourceManifest;
  }
  async function fetchChunkedData(name,index,total){
    const manifest=await loadManifest();
    const entry=manifest.files.find(file=>file.name===name);
    if(!entry||!Number.isSafeInteger(entry.size)||entry.size<=0||!Array.isArray(entry.chunks)||!entry.chunks.length)throw new Error(`${name}: 资源清单不完整`);
    const output=new Uint8Array(entry.size);let offset=0;
    const base=new URL(manifestPath,document.baseURI);
    for(const chunk of entry.chunks){
      if(!/^[a-zA-Z0-9_.-]+$/.test(chunk.path)||!Number.isSafeInteger(chunk.size)||chunk.size<=0||offset+chunk.size>entry.size||!/^[a-f0-9]{64}$/.test(chunk.sha256))throw new Error(`${name}: 无效的资源分块`);
      const url=new URL(chunk.path,base);
      let bytes;
      for(let attempt=0;attempt<3;++attempt){
        try{
          const response=await fetch(url,{cache:attempt?'reload':'default'});
          if(!response.ok)throw new Error(`HTTP ${response.status}`);
          bytes=new Uint8Array(await response.arrayBuffer());
          if(bytes.length!==chunk.size)throw new Error('下载大小不匹配');
          const digest=await crypto.subtle.digest('SHA-256',bytes);
          const hash=Array.from(new Uint8Array(digest),byte=>byte.toString(16).padStart(2,'0')).join('');
          if(hash!==chunk.sha256)throw new Error('资源校验失败');
          break;
        }catch(error){if(attempt===2)throw new Error(`${name}: ${chunk.path}: ${error.message}`);}
      }
      output.set(bytes,offset);offset+=bytes.length;
      progress.value=(index-1+offset/entry.size)/total;
      detail.textContent=`${(offset/1048576).toFixed(1)} / ${(entry.size/1048576).toFixed(1)} MiB`;
    }
    if(offset!==entry.size)throw new Error(`${name}: 资源分块总长度不匹配`);
    return output;
  }
  async function fetchData(name,index,total){
    setStatus(`载入 ${name}…`,`游戏数据 ${index}/${total}`);
    if(manifestPath)return fetchChunkedData(name,index,total);
    const response=await fetch(`game-data/${name}`);if(!response.ok)throw new Error(`${name}: HTTP ${response.status}`);
    const expected=Number(response.headers.get('content-length'))||0;
    if(!response.body){const bytes=new Uint8Array(await response.arrayBuffer());progress.value=index/total;return bytes;}
    const reader=response.body.getReader();const chunks=[];let received=0;
    for(;;){const {done,value}=await reader.read();if(done)break;chunks.push(value);received+=value.length;progress.value=(index-1+(expected?received/expected:0))/total;detail.textContent=`${(received/1048576).toFixed(1)} / ${expected?(expected/1048576).toFixed(1):'?'} MiB`;}
    const output=new Uint8Array(received);let offset=0;for(const chunk of chunks){output.set(chunk,offset);offset+=chunk.length;}return output;
  }
  function mountData(){
    const {FS,IDBFS}=options;
    options.addRunDependency('th20-browser-data');
    (async()=>{
      try{
        try{FS.mkdir('/th20-data')}catch(_){}
        FS.mount(IDBFS,{},'/th20-data');
        await new Promise((resolve,reject)=>FS.syncfs(true,error=>error?reject(error):resolve()));
        const files=['th20.dat','thbgm.dat'];
        for(let index=0;index<files.length;++index){const bytes=await fetchData(files[index],index+1,files.length);FS.createDataFile('/',files[index],bytes,true,false,true);}
        progress.value=1;setStatus('启动恢复版游戏…','方向键移动，Z 确认/射击，X 取消/符卡，Shift 低速，Esc 暂停。');
      }catch(error){console.error(error);setStatus('游戏数据载入失败',String(error));throw error;}
      finally{options.removeRunDependency('th20-browser-data');}
    })();
  }

  const options={
    canvas,
    locateFile:path=>path,
    preRun:[mountData],
    print:text=>console.log(text),
    printErr:text=>console.error(text),
    onRuntimeInitialized(){runtime=options;window.th20Runtime=options;document.body.classList.add('running');setStatus('游戏运行中','点击画面后可使用键盘或手柄。');canvas.focus();}
  };
  // The browser page supplies its own responsive window. Render the original
  // 1280x960 windowed mode instead of deriving a desktop-sized letterbox.
  addEventListener('th20-open-settings',()=>options.ccall('th20_web_set_graphics_options',null,['number','number'],[5,1]));
  createTH20Game(options).then(module=>{runtime=module;window.th20Runtime=module;}).catch(error=>setStatus('WASM 启动失败',String(error)));

  const keyCodes={ArrowLeft:37,ArrowUp:38,ArrowRight:39,ArrowDown:40,Enter:13,Escape:27,ShiftLeft:16,ShiftRight:16,ControlLeft:17,ControlRight:17,KeyZ:90,KeyX:88,KeyC:67,KeyV:86,Space:32,Tab:9};
  function sendKey(event,down){const code=keyCodes[event.code];if(code===undefined||!runtime)return;if(event.altKey&&event.code==='Enter'&&down){canvas.requestFullscreen?.();event.preventDefault();return;}runtime.ccall('th20_web_key_event',null,['number','number'],[code,down?1:0]);event.preventDefault();}
  addEventListener('keydown',event=>sendKey(event,true));addEventListener('keyup',event=>sendKey(event,false));
  addEventListener('blur',()=>runtime?.ccall('th20_web_clear_keys'));addEventListener('focus',()=>runtime?.ccall('th20_web_window_focus',null,['number'],[1]));
  document.addEventListener('visibilitychange',()=>runtime?.ccall('th20_web_window_focus',null,['number'],[document.hidden?0:1]));
  addEventListener('beforeunload',()=>{runtime?.ccall('th20_web_window_close');try{runtime?.FS.syncfs(false,()=>{})}catch(_){}});
  addEventListener('th20-error',event=>setStatus('游戏运行错误',event.detail));

  function pollGamepads(){
    if(runtime){const pads=navigator.getGamepads?navigator.getGamepads():[];for(let index=0;index<4;++index){const pad=pads[index];let buttons=0;if(pad){const map=[[12,1],[13,2],[14,4],[15,8],[9,0x10],[8,0x20],[4,0x100],[5,0x200],[0,0x1000],[1,0x2000],[2,0x4000],[3,0x8000]];for(const [button,mask] of map)if(pad.buttons[button]?.pressed)buttons|=mask;}runtime.ccall('th20_web_set_gamepad',null,['number','number','number','number','number','number','number'],[index,pad?1:0,buttons,pad?Math.round((pad.axes[0]||0)*32767):0,pad?Math.round(-(pad.axes[1]||0)*32767):0,pad?Math.round((pad.buttons[6]?.value||0)*255):0,pad?Math.round((pad.buttons[7]?.value||0)*255):0]);}}
    requestAnimationFrame(pollGamepads);
  }
  requestAnimationFrame(pollGamepads);
})();
