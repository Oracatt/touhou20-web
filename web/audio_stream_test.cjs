// Tests the production EM_JS functions extracted from web_dsound.cpp. The mock
// AudioContext inspects every scheduled sample, without speakers or a browser.
// Refill uses the recovered MusicStream guard and original track loop metadata.
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const assert=require('node:assert/strict');
const crypto=require('node:crypto');
const root=path.resolve(__dirname,'..');
const cpp=fs.readFileSync(path.join(__dirname,'src/web_dsound.cpp'),'utf8');
const functions=[...cpp.matchAll(/EM_JS\(\w+,\s*(\w+),\s*\(([^]*?)\),\s*\{([^]*?)\n\}\);/g)];
assert.equal(functions.length,12,'all production audio bridges must be exercised');

function harness(check=()=>{}){
    const intervals=new Map(),events=new Map();let nextInterval=1;
    const heap=new ArrayBuffer(2*1024*1024);
    class MockAudioContext{
        constructor(){this.currentTime=0;this.state='running';this.destination={};this.resumeCalls=0;}
        resume(){this.resumeCalls++;this.state='running';return Promise.resolve();}
        createBuffer(channels,frames,rate){const data=Array.from({length:channels},()=>new Float32Array(frames));return {data,length:frames,sampleRate:rate,duration:frames/rate,getChannelData:c=>data[c]};}
        createBufferSource(){return {connect(){return this;},disconnect(){},stop(){this.stopped=true;},start(when){this.when=when;check(this);}};}
        createGain(){return {gain:{value:1},connect(){return this;},disconnect(){}};}
        createStereoPanner(){return {pan:{value:0},connect(){return this;},disconnect(){}};}
    }
    const environment={Module:{},HEAPU8:new Uint8Array(heap),HEAPU32:new Uint32Array(heap),
        window:{AudioContext:MockAudioContext,addEventListener:(event,listener)=>events.set(event,listener)},
        document:{documentElement:{dataset:{}}},
        setInterval:callback=>{const id=nextInterval++;intervals.set(id,callback);return id;},
        clearInterval:id=>intervals.delete(id)};
    vm.createContext(environment);
    for(const [,name,args,body] of functions){
        const names=args.split(',').filter(Boolean).map(arg=>arg.trim().match(/(\w+)$/)[1]);
        vm.runInContext(`function ${name}(${names.join(',')}){${body}}`,environment);
    }
    environment.tick=(time)=>{environment.Module.th20Audio.context.currentTime=time;for(const callback of intervals.values())callback();};
    environment.events=events;return environment;
}

const report=JSON.parse(fs.readFileSync(path.join(root,'reports/bgm_recovery.json')));
const archivePath=process.argv[2]||path.join(root,'build_web/game-data/thbgm.dat');
const archive=fs.openSync(archivePath,'r');
const results=[];
for(const track of report.tracks.slice(0,2)){
    const pcm=Buffer.alloc(track.pcm_bytes);
    assert.equal(fs.readSync(archive,pcm,0,pcm.length,track.archive_offset),pcm.length);
    assert.equal(crypto.createHash('sha256').update(pcm).digest('hex'),track.pcm_sha256,'original PCM hash');
    const rate=track.sample_rate,channels=track.channels,frameBytes=channels*2;
    const chunkBytes=rate*frameBytes/4,ringBytes=chunkBytes*16,ringFrames=ringBytes/frameBytes;
    let checkedFrames=0,scheduledEnd=0,readPosition=0,nextWrite=0,refills=0;
    const expectedFrame=frame=>frame<track.loop_end_frame_exclusive?frame:
        track.loop_start_frame+(frame-track.loop_end_frame_exclusive)%(track.loop_end_frame_exclusive-track.loop_start_frame);
    const env=harness(source=>{
        const frame=Math.round(source.when*rate),buffer=source.buffer;
        assert.equal(frame,scheduledEnd,'scheduled blocks are contiguous');
        for(let i=0;i<buffer.length;++i){
            const at=expectedFrame(frame+i)*frameBytes;
            for(let channel=0;channel<channels;++channel){
                const wanted=pcm.readInt16LE(at+channel*2)/32768;
                if(buffer.data[channel][i]!==wanted)assert.fail(`${track.name}: output mismatch at frame ${frame+i}, channel ${channel}`);
            }
        }
        scheduledEnd=frame+buffer.length;checkedFrames+=buffer.length;
    });
    const readInto=(offset,length)=>{
        let remaining=length;
        while(remaining){
            if(readPosition===pcm.length)readPosition=track.intro_bytes;
            const take=Math.min(remaining,pcm.length-readPosition);
            env.HEAPU8.set(pcm.subarray(readPosition,readPosition+take),offset);
            offset+=take;readPosition+=take;remaining-=take;
        }
    };
    readInto(0,ringBytes);
    env.web_audio_store(1,0,ringBytes,channels,rate,16);
    const positionsPointer=1024*1024;
    for(let n=0;n<16;++n)env.HEAPU32[(positionsPointer>>2)+n]=(n+1)*chunkBytes-1;
    env.web_audio_notifications(1,positionsPointer,16);
    env.web_audio_play(1,1,0,0);
    const context=env.Module.th20Audio.context;
    const seconds=track.duration_seconds+8;
    for(let tick=1;tick<=Math.ceil(seconds*120);++tick){
        context.currentTime=tick/120;
        // This is the original handle_notification guard, with unsigned DWORD
        // subtraction and signed comparison preserved. Its C++ remains unchanged.
        for(let pending=0;pending<16&&env.web_audio_pending_notifications(1,0)>0;++pending){
            const write=env.web_audio_position(1,1),delta=(write-chunkBytes)>>>0;
            if(!((nextWrite<delta||write<=nextWrite)&&((delta|0)>=0||nextWrite<ringBytes-chunkBytes)))break;
            readInto(nextWrite,chunkBytes);env.web_audio_upload(1,nextWrite,nextWrite,chunkBytes);
            nextWrite=(nextWrite+chunkBytes)%ringBytes;refills++;
            env.web_audio_pending_notifications(1,1);
        }
        if(tick%3===0)env.tick(context.currentTime);
        assert.equal(env.web_audio_playing(1),1,'Unlock must preserve playing state');
    }
    const item=env.Module.th20Audio.buffers.get(1);
    assert.equal(item.underruns,0);
    assert(checkedFrames>track.loop_end_frame_exclusive+rate*7);
    assert(refills>16*20,'at least twenty complete ring rotations');
    results.push({track:track.name,seconds_checked:checkedFrames/rate,frames_checked:checkedFrames,
        ring_seconds:ringFrames/rate,refills,underruns:item.underruns,
        loop_start_frame:track.loop_start_frame,loop_end_frame_exclusive:track.loop_end_frame_exclusive,
        pcm_sha256:track.pcm_sha256,all_scheduled_samples_equal_original_pcm:true});
    env.web_audio_remove(1);
}
fs.closeSync(archive);

// DirectSound stop/resume/reposition and browser gesture semantics.
{
    const env=harness();env.web_audio_store(1,0,16000,1,8000,16);
    for(let n=0;n<4;++n)env.HEAPU32[262144+n]=(n+1)*4000-1;
    env.web_audio_notifications(1,1048576,4);env.web_audio_play(1,1,-600,2000);
    env.tick(.375);env.web_audio_stop(1);const stopped=env.web_audio_position(1,0);
    assert.equal(stopped,6000);env.tick(1.5);assert.equal(env.web_audio_position(1,0),stopped);
    env.web_audio_play(1,1,-600,2000);env.tick(1.625);assert.equal(env.web_audio_position(1,0),8000);
    assert.equal(env.web_audio_pending_notifications(1,0),2,'unconsumed notifications survive pause/resume');
    const item=env.Module.th20Audio.buffers.get(1),context=env.Module.th20Audio.context;
    const started=item.startedAt;env.web_audio_play(1,1,0,0);assert.equal(item.startedAt,started,'Play on playing buffer must not rewind');
    context.state='suspended';env.events.get('keydown')();assert.equal(context.state,'running');assert.equal(item.startedAt,started,'autoplay resume must not rewind');
    env.web_audio_set_position(1,2000);assert.equal(env.web_audio_position(1,0),2000);
    env.tick(1.75);assert.equal(env.web_audio_position(1,0),4000);
    env.web_audio_stop(1);env.web_audio_set_position(1,0);env.web_audio_play(1,0,0,0);env.tick(2.9);
    assert.equal(env.web_audio_playing(1),0,'non-looping effects must stop');
    env.web_audio_remove(1);
}
const output={status:'passed',test:'production WebAudio ring scheduler with original PCM and recovered refill guard',
    tracks:results,pause_resume_seek_autoplay_and_nonlooping_effects:'passed',
    limitation:'Mock AudioContext verifies scheduled PCM and timing; speaker/browser underrun checks require runtime validation.'};
fs.writeFileSync(path.join(root,'reports/web_audio_stream_validation.json'),JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify(output,null,2));
