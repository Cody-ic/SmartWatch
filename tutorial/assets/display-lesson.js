/* Logical coordinate/color teaching models; no hardware access. */
(() => {
  'use strict';
  const hx=(n,d=2)=>n.toString(16).toUpperCase().padStart(d,'0');
  const rgb=document.getElementById('display-rgb-lab');
  if(rgb){
    const inputs=[...rgb.querySelectorAll('input')];
    const draw=()=>{
      if(inputs.some(i=>i.value===''||!i.validity.valid)){rgb.querySelector('output').textContent='请填写 0～255 的整数颜色分量。';return;}
      const [r,g,b]=inputs.map(i=>Number(i.value));
      const r5=r>>3,g6=g>>2,b5=b>>3,value=(r5<<11)|(g6<<5)|b5;
      rgb.querySelector('[data-color]').style.backgroundColor=`rgb(${Math.round(r5*255/31)},${Math.round(g6*255/63)},${Math.round(b5*255/31)})`;
      rgb.querySelector('output').textContent=`R5=${r5}，G6=${g6}，B5=${b5} → RGB565=0x${hx(value,4)} → 本驱动期望的发送字节 ${hx(value>>8)} ${hx(value&255)}。若把这个数按小端 uint16_t 内存原样发送，则是 ${hx(value&255)} ${hx(value>>8)}；这与 MSB First 的位顺序是两件事。`;
    };inputs.forEach(i=>i.addEventListener('input',draw));draw();
  }
  const region=document.getElementById('display-window-lab');
  if(region){
    const inputs=[...region.querySelectorAll('input')];
    const draw=()=>{
      const rect=region.querySelector('[data-window]');
      if(inputs.some(i=>i.value===''||!i.validity.valid)){rect.setAttribute('visibility','hidden');region.querySelector('output').textContent='请填写有效坐标和非零尺寸。';return;}
      const [x,y,w,h]=inputs.map(i=>Number(i.value));
      if(x+w>240||y+h>240){rect.setAttribute('visibility','hidden');region.querySelector('output').textContent='这个矩形超出 240×240 地址范围。现有 DrawBitmap 不会替你裁剪；应先调整坐标或尺寸。';return;}
      rect.setAttribute('visibility','visible');
      rect.setAttribute('x',String(60+x));rect.setAttribute('y',String(35+y));rect.setAttribute('width',String(w));rect.setAttribute('height',String(h));
      const bytes=n=>`${hx(n>>8)} ${hx(n&255)}`;
      region.querySelector('output').textContent=`SetWindow(${x}, ${y}, ${x+w-1}, ${y+h-1})。D/C=0：2A；D/C=1：${bytes(x)} ${bytes(x+w-1)}。D/C=0：2B；D/C=1：${bytes(y)} ${bytes(y+h-1)}。D/C=0：2C；随后 D/C=1 发送 ${w*h} 个像素、${w*h*2} 字节。${w*h*2>65535?'这个尺寸超过现有单次 HAL 计数范围，须分块，不能直接调用一次 WritePixels。':''}`;
    };inputs.forEach(i=>i.addEventListener('input',draw));draw();
  }
})();
