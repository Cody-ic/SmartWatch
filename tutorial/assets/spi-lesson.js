/* Educational SPI model. MSB first, one 8-bit word, ideal wires; not a device driver. */
(() => {
  'use strict';
  const ns='http://www.w3.org/2000/svg';
  function node(tag,attrs,text){const n=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))n.setAttribute(k,String(v));if(text!==undefined)n.textContent=text;return n;}
  const hex=n=>n.toString(16).toUpperCase().padStart(2,'0');
  const lab=document.getElementById('spi-wave-lab');
  if(lab){
    const mode=lab.querySelector('[data-spi="mode"]'),tx=lab.querySelector('[data-spi="tx"]'),reply=lab.querySelector('[data-spi="reply"]');
    const svg=lab.querySelector('svg'),out=lab.querySelector('output');
    let edge=0,timer=null;
    function stop(){if(timer!==null)clearInterval(timer);timer=null;lab.querySelector('[data-spi="play"]').textContent='播放慢动作';}
    function draw(){
      const valid=[tx,reply].every(x=>x.value!==''&&x.validity.valid);
      for(const b of lab.querySelectorAll('[data-spi="next"],[data-spi="play"]'))b.disabled=!valid;
      if(!valid){stop();out.textContent='请填写 0～255 的整数。';return;}
      const m=Number(mode.value),pol=m>>1,pha=m&1,a=Number(tx.value),b=Number(reply.value);
      svg.replaceChildren();svg.setAttribute('viewBox','0 0 960 350');
      svg.append(node('rect',{width:960,height:350,fill:'#fffefb'}));
      const start=160,step=40,end=start+16*step;
      const names=['CS','SCK','MOSI','MISO'],base=[65,135,205,275];
      const value=(row,e)=>{
        if(row===0)return 0;
        if(row===1)return e%2?1-pol:pol;
        const bit=pha?Math.max(0,Math.floor((e-1)/2)):Math.min(7,Math.floor(e/2));
        return ((row===2?a:b)>>(7-bit))&1;
      };
      if(edge>0)svg.append(node('rect',{x:start+(edge-1)*step,y:30,width:step,height:275,fill:'#fff0cd'}));
      for(let r=0;r<4;r++){
        svg.append(node('text',{x:18,y:base[r]+6,fill:'#183d37','font-size':18},names[r]));
        const y=v=>base[r]+(v?-15:15);
        const idle=r===0?1:r===1?pol:value(r,0);
        let d=`M105 ${y(idle)} H${start} V${y(value(r,0))}`;
        for(let e=1;e<=16;e++)d+=` H${start+e*step} V${y(value(r,e))}`;
        d+=` H${end+35}`;
        if(r===0)d+=` V${y(1)} H900`;
        svg.append(node('path',{d,fill:'none',stroke:r===1?'#007b68':'#416a83','stroke-width':3}));
      }
      let n=0,received=0,slaveReceived=0;
      for(let e=1;e<=16;e++){
        const sample=pha?e%2===0:e%2===1;
        if(sample){
          const bit=(a>>(7-n))&1,ret=(b>>(7-n))&1;
          const x=start+e*step;
          svg.append(node('line',{x1:x,y1:100,x2:x,y2:300,stroke:'#d98c24','stroke-dasharray':'4 4'}));
          svg.append(node('text',{x,y:330,'text-anchor':'middle',fill:'#7e4c0b','font-size':15},`D${7-n}`));
          if(e<=edge){slaveReceived=(slaveReceived<<1)|bit;received=(received<<1)|ret;}
          n++;
        }
      }
      const samples=pha?Math.floor(edge/2):Math.ceil(edge/2);
      const direction=edge%2?(pol?'下降':'上升'):(pol?'上升':'下降');
      const sampling=edge>0&&(pha?edge%2===0:edge%2===1);
      let desc=edge===0?`CS 已拉低，SCK 空闲为${pol?'高':'低'}；${pha?'第一个边沿准备首位，第二个边沿才采样':'首位 D7 已在第一个边沿前准备好'}。`:`第 ${edge} 个边沿（${direction}沿）：${sampling?'双方采样当前数据位':'数据更新一侧的边沿，为后续采样准备'}。`;
      if(edge===16)desc+=' 八个采样点已经完成；示意图随后释放 CS。真实事务也可能继续传下一个字节。';
      out.textContent=`Mode ${m}，CPOL=${pol}、CPHA=${pha}。${desc} 已采样 ${samples}/8 位：从机收到主机数据的当前值 0x${hex(slaveReceived)}，主机收到返回数据的当前值 0x${hex(received)}。`;
    }
    function next(){edge=Math.min(16,edge+1);draw();if(edge===16)stop();}
    lab.querySelector('[data-spi="next"]').addEventListener('click',next);
    lab.querySelector('[data-spi="reset"]').addEventListener('click',()=>{stop();edge=0;draw();});
    lab.querySelector('[data-spi="play"]').addEventListener('click',()=>{if(timer!==null){stop();return;}if(edge===16)edge=0;timer=setInterval(next,650);lab.querySelector('[data-spi="play"]').textContent='暂停慢动作';});
    for(const x of [mode,tx,reply])x.addEventListener('input',()=>{stop();edge=0;draw();});
    document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});draw();
  }
  const cs=document.getElementById('spi-cs-lab');
  if(cs){let a=[],b=[];const choose=cs.querySelector('select'),out=cs.querySelector('output');
    function show(message){const v=choose.value;for(const n of cs.querySelectorAll('[data-device]'))n.classList.toggle('selected',v===n.dataset.device||v==='both');cs.querySelector('[data-received="a"]').textContent=a.length?a.join(' '):'尚未接收';cs.querySelector('[data-received="b"]').textContent=b.length?b.join(' '):'尚未接收';out.textContent=message||`CS_A=${v==='a'||v==='both'?0:1}，CS_B=${v==='b'||v==='both'?0:1}。现在可以试发一个字节。`;}
    choose.addEventListener('change',()=>show());
    cs.querySelector('[data-cs="send"]').addEventListener('click',()=>{const v=choose.value;if(v==='both'){show('两颗从机同时被选中：共享 MISO 可能发生输出冲突。本演示阻止传输，不模拟一个有效返回值。');return;}if(v==='a')a.push('A6');if(v==='b')b.push('A6');a=a.slice(-8);b=b.slice(-8);show(v==='none'?'没有选中从机：即便主机产生时钟，也没有本例设备接受这次事务。':`设备 ${v.toUpperCase()} 被选中，收到 A6；另一设备不参与。`);});
    cs.querySelector('[data-cs="reset"]').addEventListener('click',()=>{a=[];b=[];choose.value='none';show();});show();
  }
})();
