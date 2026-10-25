/* Offline arithmetic illustration; artificial points are not sensor measurements. */
(() => {
  'use strict';
  const lab = document.getElementById('prototype-lab');
  if (!lab) return;
  const angle = lab.querySelector('[name="angle"]');
  const wrong = lab.querySelector('[name="wrong"]');
  const svg = lab.querySelector('svg');
  const output = lab.querySelector('output');
  const ns = 'http://www.w3.org/2000/svg';
  function el(name, attrs, text) {
    const e = document.createElementNS(ns, name);
    Object.entries(attrs).forEach(([k,v]) => e.setAttribute(k,v));
    if (text !== undefined) e.textContent = text;
    return e;
  }
  const unit = (x,y) => { const n=Math.hypot(x,y); return [x/n,y/n]; };
  function draw() {
    const walking = wrong.checked ? unit(1.7,1.3) : unit(1.7,.3);
    const running = unit(.3,1.7);
    const radians = Number(angle.value)*Math.PI/180;
    const query = [Math.cos(radians),Math.sin(radians)];
    const scores = [walking,running].map(p => p[0]*query[0]+p[1]*query[1]);
    svg.replaceChildren();
    svg.append(el('path',{d:'M60 30V280H475',fill:'none',stroke:'#78978b','stroke-width':2}));
    for (const [p,name,color] of [[walking,'走路中心','#087d68'],[running,'跑步中心','#5268b4'],[query,'新样本','#bc671b']]) {
      const x=60+p[0]*220,y=280-p[1]*220;
      svg.append(el('line',{x1:60,y1:280,x2:x,y2:y,stroke:color,'stroke-width':3}));
      svg.append(el('circle',{cx:x,cy:y,r:6,fill:color}));
      // Separate labels prevent overlap when two vectors nearly coincide.
      const row=name==='走路中心'?0:name==='跑步中心'?1:2;
      svg.append(el('text',{x:290,y:28+row*23,fill:color,'font-size':16},name));
    }
    svg.append(el('text',{x:450,y:307,fill:'#78978b','font-size':16},'x'));
    svg.append(el('text',{x:35,y:35,fill:'#78978b','font-size':16},'y'));
    output.textContent = `方向 ${angle.value}°；与走路中心相似度 ${scores[0].toFixed(3)}，与跑步中心 ${scores[1].toFixed(3)}。选择：${scores[0]>=scores[1]?'走路':'跑步'}。`+
      (wrong.checked?'错误标签改变了走路中心。':'')+'这些值是方向相似度，不是校准概率。';
  }
  angle.addEventListener('input',draw);
  wrong.addEventListener('change',draw);
  draw();
})();
