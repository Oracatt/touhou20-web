#include <dsound.h>
#include <emscripten.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
// AudioBufferSourceNode takes an immutable snapshot. DirectSound buffers are
// mutable rings, so queue short snapshots ahead of the audible cursor instead
// of looping the first snapshot forever. Unlock updates only the live ring;
// already queued samples and the playback clock remain uninterrupted.
EM_JS(void,web_audio_store,(int id,const std::uint8_t* bytes,int length,int channels,int rate,int bits),{
    if(!Module.th20Audio) {
        const audio=Module.th20Audio={buffers:new Map(),context:null,timer:null};
        audio.frame=(item)=>{
            if(!item.playing||!audio.context)return item.positionFrames;
            const frame=item.startFrame+Math.max(0,Math.floor((audio.context.currentTime-item.startedAt)*item.rate+1e-7));
            return item.looping?frame:Math.min(item.frames,frame);
        };
        audio.notificationCount=(item,frame)=>{
            if(!item.notifications.length||!item.frames)return 0;
            const turns=Math.floor(frame/item.frames),position=frame%item.frames;
            return turns*item.notifications.length+item.notifications.filter(offset=>offset<=position).length;
        };
        audio.stop=(item)=>{
            const frame=audio.frame(item);
            item.positionFrames=!item.looping&&frame>=item.frames?0:frame;
            item.playing=false;
            for(const source of item.sources){try{source.stop();}catch(_){}source.disconnect();}
            item.sources.clear();
        };
        audio.decode=(item,first,frames)=>{
            const buffer=audio.context.createBuffer(item.channels,frames,item.rate);
            const sampleBytes=item.bits>>3;
            for(let channel=0;channel<item.channels;++channel){
                const output=buffer.getChannelData(channel);
                for(let frame=0;frame<frames;++frame){
                    const at=(((first+frame)%item.frames)*item.channels+channel)*sampleBytes;
                    let sample;
                    if(item.bits===16){sample=item.bytes[at]|(item.bytes[at+1]<<8);if(sample&0x8000)sample-=0x10000;sample/=32768;}
                    else sample=(item.bytes[at]-128)/128;
                    output[frame]=sample;
                }
            }
            return buffer;
        };
        audio.pumpItem=(item)=>{
            if(!item.playing)return;
            const current=audio.frame(item);
            if(!item.looping&&current>=item.frames){audio.stop(item);return;}
            // A long browser stall must skip elapsed output, never replay an
            // old block late and gradually drift behind the DirectSound cursor.
            if(item.nextFrame<current){item.underruns++;item.nextFrame=current;}
            const horizon=current+Math.ceil(item.rate*0.15);
            const quantum=Math.max(1,Math.floor(item.rate*0.05));
            while(item.nextFrame<horizon&&(item.looping||item.nextFrame<item.frames)){
                const count=Math.min(quantum,item.looping?quantum:item.frames-item.nextFrame);
                const source=audio.context.createBufferSource();
                source.buffer=audio.decode(item,item.nextFrame,count);
                source.connect(item.gain);
                const when=item.startedAt+(item.nextFrame-item.startFrame)/item.rate;
                source.start(when);
                item.sources.add(source);
                source.onended=()=>{item.sources.delete(source);source.disconnect();};
                item.nextFrame+=count;
            }
            if(item.notifications.length&&typeof document!=='undefined'&&current-item.lastReportFrame>=item.rate/4){
                item.lastReportFrame=current;
                Object.assign(document.documentElement.dataset,{
                    th20AudioBuffer:String(item.id),th20AudioPlaying:String(item.playing),th20AudioFrames:String(current),
                    th20AudioRingFrames:String(item.frames),th20AudioRate:String(item.rate),
                    th20AudioWrites:String(item.writes),th20AudioUnderruns:String(item.underruns),
                    th20AudioContext:audio.context.state,th20AudioRingPosition:String(current%item.frames),
                    th20AudioPendingNotifications:String(audio.notificationCount(item,current)-item.notificationBase),
                    th20AudioWriteHash:item.lastWriteHash||""
                });
            }
        };
        audio.play=(item,looping,volume,pan)=>{
            if(!item.frames)return;
            const AudioContext=window.AudioContext||window.webkitAudioContext;
            if(!audio.context){
                audio.context=new AudioContext({sampleRate:item.rate,latencyHint:'interactive'});
                // Autoplay suspension holds currentTime at zero. Resume that
                // clock on a gesture without issuing Play again or resetting it.
                const resume=()=>{if(audio.context&&audio.context.state!=='running')audio.context.resume();};
                for(const event of ['pointerdown','keydown','touchstart'])window.addEventListener(event,resume,{passive:true});
            }
            audio.context.resume();
            item.looping=!!looping;item.volume=volume;item.pan=pan;
            if(!item.gain){
                item.gain=audio.context.createGain();item.panner=audio.context.createStereoPanner();
                item.gain.connect(item.panner).connect(audio.context.destination);
            }
            item.gain.gain.value=volume<=-10000?0:Math.pow(10,volume/2000);
            item.panner.pan.value=Math.max(-1,Math.min(1,pan/10000));
            if(item.playing)return;
            item.startFrame=item.positionFrames;item.nextFrame=item.startFrame;
            item.startedAt=audio.context.currentTime;item.playing=true;
            item.lastReportFrame=-item.rate;item.underruns=0;
            audio.pumpItem(item);
            if(audio.timer===null)audio.timer=setInterval(()=>{
                for(const buffer of audio.buffers.values())audio.pumpItem(buffer);
            },25);
        };
    }
    const audio=Module.th20Audio;
    let item=audio.buffers.get(id);
    if(!item){
        item={id:id,bytes:new Uint8Array(length),channels:channels,rate:rate,bits:bits,
            frames:Math.floor(length/(channels*(bits>>3))),positionFrames:0,playing:false,
            looping:false,sources:new Set(),notifications:[],notificationBase:0,writes:0,underruns:0};
        audio.buffers.set(id,item);
    } else if(item.bytes.length!==length||item.channels!==channels||item.rate!==rate||item.bits!==bits){
        audio.stop(item);item.bytes=new Uint8Array(length);item.channels=channels;item.rate=rate;item.bits=bits;
        item.frames=Math.floor(length/(channels*(bits>>3)));item.positionFrames=0;
    }
    item.bytes.set(HEAPU8.subarray(bytes,bytes+length));
});
EM_JS(void,web_audio_upload,(int id,int offset,const std::uint8_t* bytes,int length),{
    const item=Module.th20Audio&&Module.th20Audio.buffers.get(id);
    if(item&&length){
        const chunk=HEAPU8.subarray(bytes,bytes+length);item.bytes.set(chunk,offset);item.writes++;
        if(item.notifications.length){let hash=2166136261;for(const byte of chunk)hash=Math.imul(hash^byte,16777619);item.lastWriteHash=(hash>>>0).toString(16).padStart(8,'0');}
    }
});
EM_JS(void,web_audio_play,(int id,int looping,int volume,int pan),{
    const audio=Module.th20Audio;if(!audio)return;const item=audio.buffers.get(id);
    if(item)audio.play(item,looping,volume,pan);
});
EM_JS(void,web_audio_stop,(int id),{
    const audio=Module.th20Audio;const item=audio&&audio.buffers.get(id);if(item)audio.stop(item);
});
EM_JS(void,web_audio_remove,(int id),{
    const audio=Module.th20Audio;if(!audio)return;const item=audio.buffers.get(id);
    if(item){audio.stop(item);if(item.gain)item.gain.disconnect();if(item.panner)item.panner.disconnect();audio.buffers.delete(id);}
    if(!audio.buffers.size&&audio.timer!==null){clearInterval(audio.timer);audio.timer=null;}
});
EM_JS(int,web_audio_position,(int id,int write),{
    const audio=Module.th20Audio;if(!audio)return 0;const item=audio.buffers.get(id);if(!item||!item.frames)return 0;
    const frame=write&&item.playing?item.nextFrame:audio.frame(item);
    return (frame%item.frames)*item.channels*(item.bits>>3);
});
EM_JS(int,web_audio_playing,(int id),{
    const audio=Module.th20Audio;const item=audio&&audio.buffers.get(id);if(!item)return 0;
    if(item.playing&&!item.looping&&audio.frame(item)>=item.frames)audio.stop(item);
    return item.playing?1:0;
});
EM_JS(void,web_audio_set_position,(int id,int position),{
    const audio=Module.th20Audio;const item=audio&&audio.buffers.get(id);if(!item)return;
    const playing=item.playing;if(playing)audio.stop(item);
    item.positionFrames=Math.floor(position/(item.channels*(item.bits>>3)))%Math.max(1,item.frames);
    item.notificationBase=audio.notificationCount(item,item.positionFrames);
    if(playing)audio.play(item,item.looping,item.volume,item.pan);
});
EM_JS(void,web_audio_set_volume,(int id,int volume),{
    const item=Module.th20Audio&&Module.th20Audio.buffers.get(id);
    if(item){item.volume=volume;if(item.gain)item.gain.gain.value=volume<=-10000?0:Math.pow(10,volume/2000);}
});
EM_JS(void,web_audio_set_pan,(int id,int pan),{
    const item=Module.th20Audio&&Module.th20Audio.buffers.get(id);
    if(item){item.pan=pan;if(item.panner)item.panner.pan.value=Math.max(-1,Math.min(1,pan/10000));}
});
EM_JS(void,web_audio_notifications,(int id,const std::uint32_t* offsets,int count),{
    const audio=Module.th20Audio;const item=audio&&audio.buffers.get(id);if(!item)return;
    const frameBytes=item.channels*(item.bits>>3);item.notifications=[];
    for(let n=0;n<count;++n){const offset=HEAPU32[(offsets>>2)+n];if(offset<item.bytes.length)item.notifications.push(Math.floor(offset/frameBytes)+1);}
    item.notifications.sort((a,b)=>a-b);
    item.notificationBase=audio.notificationCount(item,audio.frame(item));
});
EM_JS(int,web_audio_pending_notifications,(int id,int consume),{
    const audio=Module.th20Audio;const item=audio&&audio.buffers.get(id);if(!item||!item.playing)return 0;
    const count=audio.notificationCount(item,audio.frame(item))-item.notificationBase;
    if(consume&&count>0)item.notificationBase++;
    return Math.max(0,count);
});

std::atomic<int> next_audio_id{1};
template<class Derived>struct RefCount{std::atomic<ULONG> references{1};ULONG add(){return ++references;}ULONG release(Derived* self){const auto count=--references;if(!count)delete self;return count;}};

struct WebSoundBuffer final:IDirectSoundBuffer,IDirectSoundNotify,RefCount<WebSoundBuffer>{
    int id=next_audio_id++;WAVEFORMATEX format{};std::vector<std::uint8_t> bytes;DWORD flags{};LONG volume{},pan{};std::vector<DSBPOSITIONNOTIFY> notifications;
    explicit WebSoundBuffer(const DSBUFFERDESC& description):flags(description.dwFlags),bytes(description.dwBufferBytes){if(description.lpwfxFormat)format=*description.lpwfxFormat;sync();}
    WebSoundBuffer(const WebSoundBuffer& other):id(next_audio_id++),format(other.format),bytes(other.bytes),flags(other.flags),volume(other.volume),pan(other.pan){sync();}
    ~WebSoundBuffer(){web_audio_remove(id);}
    void sync(){if(format.nChannels&&format.nSamplesPerSec&&format.wBitsPerSample)web_audio_store(id,bytes.data(),bytes.size(),format.nChannels,format.nSamplesPerSec,format.wBitsPerSample);}
    HRESULT QueryInterface(REFIID,void** output)override{if(!output)return E_POINTER;*output=static_cast<IDirectSoundNotify*>(this);AddRef();return S_OK;}
    ULONG AddRef()override{return add();}ULONG Release()override{return release(this);}
    HRESULT GetCaps(DSCAPS* caps)override{if(!caps)return E_POINTER;std::memset(caps,0,sizeof(*caps));caps->dwSize=sizeof(*caps);return S_OK;}
    HRESULT GetCurrentPosition(DWORD* play,DWORD* write)override{if(play)*play=web_audio_position(id,0);if(write)*write=web_audio_position(id,1);return S_OK;}
    HRESULT GetStatus(DWORD* status)override{if(!status)return E_POINTER;*status=web_audio_playing(id)?DSBSTATUS_PLAYING:0;return S_OK;}
    HRESULT Initialize(void*,const DSBUFFERDESC*)override{return S_OK;}
    HRESULT Lock(DWORD offset,DWORD count,void** first,DWORD* first_size,void** second,DWORD* second_size,DWORD flags_value)override{if(!first||!first_size)return E_POINTER;if(flags_value&DSBLOCK_ENTIREBUFFER){offset=0;count=bytes.size();}if(count==0)count=bytes.size();if(count>bytes.size())return E_INVALIDARG;if(bytes.empty()){*first=nullptr;*first_size=0;if(second)*second=nullptr;if(second_size)*second_size=0;return S_OK;}offset%=bytes.size();const auto initial=std::min<std::size_t>(count,bytes.size()-offset);if(count>initial&&(!second||!second_size))return E_INVALIDARG;*first=bytes.data()+offset;*first_size=initial;if(second)*second=count>initial?bytes.data():nullptr;if(second_size)*second_size=count-initial;return S_OK;}
    HRESULT Play(DWORD,DWORD,DWORD play_flags)override{web_audio_play(id,(play_flags&DSBPLAY_LOOPING)!=0,volume,pan);return S_OK;}
    HRESULT SetCurrentPosition(DWORD position)override{if(!bytes.empty()&&position>=bytes.size())return E_INVALIDARG;web_audio_set_position(id,position);return S_OK;}
    HRESULT SetFormat(const WAVEFORMATEX* value)override{if(!value)return E_POINTER;format=*value;sync();return S_OK;}
    HRESULT SetPan(LONG value)override{pan=value;web_audio_set_pan(id,value);return S_OK;}
    HRESULT SetVolume(LONG value)override{volume=value;web_audio_set_volume(id,value);return S_OK;}
    HRESULT Stop()override{web_audio_stop(id);return S_OK;}
    HRESULT Unlock(void* first,DWORD first_size,void* second,DWORD second_size)override{auto upload=[&](void* pointer,DWORD size){if(!size)return true;const auto* begin=static_cast<std::uint8_t*>(pointer);if(begin<bytes.data()||begin>bytes.data()+bytes.size()||size>static_cast<std::size_t>(bytes.data()+bytes.size()-begin))return false;web_audio_upload(id,begin-bytes.data(),begin,size);return true;};return upload(first,first_size)&&upload(second,second_size)?S_OK:E_INVALIDARG;}
    HRESULT Restore()override{return S_OK;}
    HRESULT SetNotificationPositions(DWORD count,const DSBPOSITIONNOTIFY* values)override{if(count&&!values)return E_POINTER;notifications.assign(values,values+count);std::vector<std::uint32_t> offsets;for(const auto& notification:notifications)offsets.push_back(notification.dwOffset);web_audio_notifications(id,offsets.data(),offsets.size());return S_OK;}
};

struct WebDirectSound final:IDirectSound8,RefCount<WebDirectSound>{
    HRESULT QueryInterface(REFIID,void** output)override{if(!output)return E_POINTER;*output=this;AddRef();return S_OK;}ULONG AddRef()override{return add();}ULONG Release()override{return release(this);}
    HRESULT CreateSoundBuffer(const DSBUFFERDESC* description,IDirectSoundBuffer** output,IUnknown*)override{if(!description||!output)return E_POINTER;*output=new WebSoundBuffer(*description);return S_OK;}
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer* source,IDirectSoundBuffer** output)override{if(!source||!output)return E_POINTER;auto* buffer=dynamic_cast<WebSoundBuffer*>(source);if(!buffer)return E_INVALIDARG;*output=new WebSoundBuffer(*buffer);return S_OK;}
    HRESULT SetCooperativeLevel(HWND,DWORD)override{return S_OK;}
};
}

extern "C" int th20_web_audio_notification_count(IDirectSoundBuffer* buffer,int consume){auto* web=dynamic_cast<WebSoundBuffer*>(buffer);return web?web_audio_pending_notifications(web->id,consume):0;}
extern "C" HRESULT WINAPI DirectSoundCreate8(const GUID*,IDirectSound8** output,IUnknown*){if(!output)return E_POINTER;*output=new WebDirectSound;return S_OK;}
