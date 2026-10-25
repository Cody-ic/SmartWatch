/* Offline teaching models. No device access. */
(() => {
  'use strict';
  const hx=n=>n.toString(16).toUpperCase().padStart(2,'0');
  const parse=input=>/^[0-9a-f]{1,2}$/i.test(input.value.trim())?parseInt(input.value,16):null;
  const address=document.getElementById('i2c-address-lab');
  if(address){const input=address.querySelector('input'),select=address.querySelector('select'),out=address.querySelector('output');
    const update=()=>{const n=parse(input);if(n===null||n<8||n>0x77){out.textContent='请输入普通七位设备地址 08～77（十六进制），本模型排除保留范围。';return;}
      const rw=Number(select.value);out.textContent=`七位设备地址：0x${hx(n)} = ${n.toString(2).padStart(7,'0')}。\n${rw?'主机读取':'主机写入'}：末尾接读写位 ${rw}，线上地址字节为 0x${hx((n<<1)|rw)}。\n传给本章 HAL 的 DevAddress：0x${hx(n<<1)}，读、写都用这个值，方向由 HAL 设置。`;};
    input.addEventListener('input',update);select.addEventListener('change',update);update();}
  const bus=document.getElementById('i2c-bus-lab');
  if(bus){const ins=[...bus.querySelectorAll('input')],out=bus.querySelector('output');const update=()=>{
    const [mcu,sensor]=ins.map(i=>i.checked);
    const who=mcu&&sensor?'STM32 和传感器都':mcu?'STM32':'传感器';
    out.textContent=mcu||sensor?`${who}通过内部电路把 SDA 接到 GND。\nSDA 电压接近 0 V，读作 0。`:'两个内部开关都断开，没有芯片把 SDA 接到 GND。\nSDA 经上拉电阻连到 3.3 V 电源，电压为高，读作 1。';
  };ins.forEach(i=>i.addEventListener('change',update));update();}
  const mask=document.getElementById('i2c-mask-lab');
  if(mask){const input=mask.querySelector('input'),select=mask.querySelector('select'),out=mask.querySelector('output');const update=()=>{const old=parse(input);if(old===null){out.textContent='请输入一个十六进制字节。';return;}const bits=Number(select.value)<<3,result=(old&0xe7)|(bits&0x18);out.textContent=`(${hx(old)} & E7) | ${hx(bits)} → ${hx(result)}，二进制 ${result.toString(2).padStart(8,'0')}。只有 bit4:3 被字段覆盖。`;};input.addEventListener('input',update);select.addEventListener('change',update);update();}
  const decode=document.getElementById('i2c-decode-lab');
  if(decode){const ins=[...decode.querySelectorAll('input')],out=decode.querySelector('output');const update=()=>{const values=ins.map(parse);if(values.some(n=>n===null)){out.textContent='两个输入都应是一个十六进制字节。';return;}const word=(values[0]<<8)|values[1],signed=word>=0x8000?word-65536:word;out.textContent=`${hx(values[0])} ${hx(values[1])} → 0x${word.toString(16).toUpperCase().padStart(4,'0')} → 有符号 ${signed} → ${(signed/4096).toFixed(4)} g。`;};ins.forEach(i=>i.addEventListener('input',update));update();}
  const byte=document.getElementById('i2c-byte-lab');
  if(byte){let step=0,timer=null;const svg=byte.querySelector('svg'),out=byte.querySelector('output'),play=byte.querySelector('[data-byte-play]');const bits=[1,1,0,1,0,0,0,0,0];
    const stop=()=>{clearInterval(timer);timer=null;play.textContent='播放过程';};
    const draw=()=>{let drawing='<rect width="760" height="120" fill="#fffefb"/><text x="8" y="35" fill="#182a30" font-size="15">SCL</text><text x="8" y="90" fill="#182a30" font-size="15">SDA</text>';
      let clock='M65 43',data=`M65 ${bits[0]?65:90}`;
      bits.forEach((v,i)=>{const x=65+i*75,y=v?65:90;drawing+=`<rect x="${x}" y="6" width="73" height="106" fill="${step===i+1?'#f8e9d7':'#e6f2eb'}"/><text x="${x+37}" y="110" text-anchor="middle" font-size="13" fill="#182a30">${i<8?'位'+(7-i)+'='+v:'应答=0'}</text>`;clock+=` H${x+20} V19 H${x+50} V43 H${x+75}`;data+=` V${y} H${x+75}`;});
      drawing+=`<path d="${clock}" fill="none" stroke="#007b68" stroke-width="2"/><path d="${data}" fill="none" stroke="#007b68" stroke-width="2"/><path d="M665 90 H740" fill="none" stroke="#a1580b" stroke-width="3"/><path d="M665 8 V99" fill="none" stroke="#a1580b" stroke-dasharray="3 3"/>`;svg.innerHTML=drawing;
      if(step===0)out.textContent='准备发送 0xD0，也就是 11010000。\n当前 SCL 为低；点“下一拍”，先看最高位 1。';
      else if(step<=8){const bit=bits[step-1];out.textContent=`第 ${step} 拍：STM32 准备位${8-step} = ${bit}。\nSCL 为低时，STM32 让 SDA 为${bit?'高':'低'}电平。\nSCL 变高时，传感器读到 ${bit}；SCL 再变低后，准备下一位。`;}
      else out.textContent='第九拍：现在由传感器回应。\nSCL 为低时，STM32 释放 SDA，传感器让 SDA 为低，准备应答。\nSTM32 让 SCL 变高，读到 SDA 为低，这就是 ACK。SCL 随后回到低，结束这一拍。';};
    byte.querySelector('[data-byte-next]').addEventListener('click',()=>{stop();step=(step+1)%10;draw();});byte.querySelector('[data-byte-reset]').addEventListener('click',()=>{stop();step=0;draw();});play.addEventListener('click',()=>{if(timer){stop();return;}if(step===9)step=0;play.textContent='暂停过程';timer=setInterval(()=>{step++;draw();if(step===9)stop();},850);});document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});draw();}
})();
