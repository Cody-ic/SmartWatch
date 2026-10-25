/* Local-only tutorial UI. No tracking, CDN or firmware interaction. */
(() => {
  'use strict';
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const read = key => { try { return localStorage.getItem(key); } catch (_) { return null; } };
  const save = (key, val) => { try { localStorage.setItem(key, val); } catch (_) {} };
  let completed = [];
  try { completed = JSON.parse(read('sw-course-done') || '[]'); if (!Array.isArray(completed)) completed = []; } catch (_) {}
  const current = document.body.dataset.lesson;
  function progress() {
    $$('.lesson-link').forEach(a => a.classList.toggle('done', completed.includes(a.dataset.id)));
    const links = $$('.lesson-link');
    const n = links.filter(a => completed.includes(a.dataset.id)).length;
    if ($('#progress-value')) $('#progress-value').textContent = `${n} / ${links.length} 节已完成`;
    if ($('#progress-bar')) { $('#progress-bar').max = links.length; $('#progress-bar').value = n; }
    if ($('#mark-done')) $('#mark-done').textContent = completed.includes(current) ? '✓ 已完成 · 点击撤销' : '标记本篇完成';
  }
  if (read('sw-course-theme') === 'dark') document.body.classList.add('dark');
  $('#theme-toggle')?.addEventListener('click', () => { document.body.classList.toggle('dark'); save('sw-course-theme', document.body.classList.contains('dark') ? 'dark' : 'light'); });
  $('#menu-toggle')?.addEventListener('click', () => { document.body.classList.toggle('nav-open'); $('#menu-toggle').setAttribute('aria-expanded', document.body.classList.contains('nav-open')); });
  $('#mark-done')?.addEventListener('click', () => { completed = completed.includes(current) ? completed.filter(x => x !== current) : [...completed, current]; save('sw-course-done', JSON.stringify(completed)); progress(); });
  let searching = false;
  const branchOpenBeforeSearch = new Map();
  $$('.nav-branch').forEach(branch => {
    const key = `sw-course-nav-${branch.id}`;
    if ($('a.current', branch) || read(key) === 'open') branch.open = true;
    branch.addEventListener('toggle', () => {
      if (!searching) save(key, branch.open ? 'open' : 'closed');
    });
  });
  $('#search')?.addEventListener('input', e => {
    const q = e.target.value.trim().toLocaleLowerCase();
    if (q && !searching) $$('.nav-branch').forEach(branch => branchOpenBeforeSearch.set(branch, branch.open));
    $$('.lesson-link').forEach(a => { a.hidden = !(`${a.textContent} ${a.dataset.keywords}`).toLocaleLowerCase().includes(q); });
    $$('.nav-branch').forEach(branch => {
      if (q) {
        const matched = $$('.lesson-link', branch).some(a => !a.hidden);
        branch.hidden = !matched;
        branch.open = matched;
      } else {
        branch.hidden = false;
        if (searching) branch.open = branchOpenBeforeSearch.get(branch) ?? false;
      }
    });
    searching = Boolean(q);
    if ($('#search-empty')) $('#search-empty').hidden = $$('.lesson-link').some(a => !a.hidden);
  });
  $$('pre').forEach(pre => {
    const b = document.createElement('button'); b.className = 'copy'; b.type = 'button'; b.textContent = '复制代码';
    b.addEventListener('click', async () => {
      const code = $('code', pre).textContent;
      try {
        if (navigator.clipboard && window.isSecureContext) await navigator.clipboard.writeText(code);
        else { const t = document.createElement('textarea'); t.value = code; t.style.position = 'fixed'; t.style.opacity = '0'; document.body.append(t); t.select(); if (!document.execCommand('copy')) throw Error('copy'); t.remove(); }
        b.textContent = '已复制';
      } catch (_) { b.textContent = '请选中代码复制'; }
      setTimeout(() => { b.textContent = '复制代码'; }, 1800);
    }); pre.append(b);
  });
  $$('form.quiz').forEach(f => f.addEventListener('submit', e => { e.preventDefault(); const chosen = $('input:checked', f); $('output', f).textContent = !chosen ? '请先选择一个答案。' : chosen.value === f.dataset.answer ? `正确。${f.dataset.explain}` : `再想一想：${f.dataset.hint}`; }));
  const ns = 'http://www.w3.org/2000/svg';
  function el(tag, attrs, text) { const e = document.createElementNS(ns, tag); Object.entries(attrs).forEach(([k,v]) => e.setAttribute(k, v)); if (text !== undefined) e.textContent = text; return e; }
  function wave(svg, values, names) {
    svg.replaceChildren(); svg.setAttribute('viewBox', '0 0 760 130');
    svg.append(el('line', {x1:45,y1:105,x2:735,y2:105,stroke:'#b8c7c3'}));
    const step = 680 / values.length; let d = '';
    values.forEach((v,i) => { const x=45+i*step,y=v?30:85; d += `${i?'L':'M'} ${x} ${y} L ${x+step} ${y} `; svg.append(el('text',{x:x+step/2,y:123,'text-anchor':'middle','font-size':11,fill:'#607075'},names?.[i] ?? '')); });
    svg.append(el('path',{d,fill:'none',stroke:'#007b68','stroke-width':3}));
  }
  $$('[data-widget="uart"]').forEach(w => {
    const input=$('input',w),svg=$('svg',w),out=$('output',w); let marker=null, start=0, offset=0, running=false;
    function draw(){const v=Math.max(0,Math.min(255,Number(input.value)||0)); const bits=[1,0,...Array.from({length:8},(_,i)=>(v>>i)&1),1,1]; wave(svg,bits,['空闲','起始',...'01234567'.split('').map(x=>'D'+x),'停止','空闲']);out.textContent=`0x${v.toString(16).padStart(2,'0').toUpperCase()} · LSB 先传 · 8N1 每字节 10 位`;marker=el('line',{x1:45,y1:14,x2:45,y2:97,stroke:'#db8a22','stroke-width':2});svg.append(marker);}
    draw(); input.addEventListener('input',draw);
    const btn=$('button',w);btn.addEventListener('click',()=>{const now=performance.now();if(running){offset=(offset+(now-start)/7000)%1;running=false;}else{start=now;running=true;}btn.textContent=running?'暂停动画':'播放位传输';if(running)requestAnimationFrame(tick);});
    function tick(now){if(!running)return; const p=(offset+(now-start)/7000)%1; marker.setAttribute('transform',`translate(${p*680} 0)`);if(now-start>7000){offset=p;start=now;}requestAnimationFrame(tick);}
  });
  $$('[data-widget="pwm"]').forEach(w=>{const ins=$$('input',w);function update(){if(ins.some(i=>!i.validity.valid||i.value==='')){$('output',w).textContent='请在输入框允许的范围内填写参数。';return;}const f=Number(ins[0].value),psc=Number(ins[1].value),arr=Number(ins[2].value),duty=Number(ins[3].value);$('output',w).textContent=`更新频率 ${(f/((psc+1)*(arr+1))).toFixed(2)} Hz · CCR≈${Math.round((arr+1)*duty/100)} · 占空比 ${duty}%`;
    const svg=$('svg',w);const vals=Array.from({length:100},(_,i)=>(i%25)/25<duty/100?1:0);wave(svg,vals); }ins.forEach(x=>x.addEventListener('input',update));update();});
  $$('[data-widget="adc"]').forEach(w=>{const ins=$$('input',w);function update(){if(ins.some(i=>!i.validity.valid||i.value==='')){$('output',w).textContent='请填写有效位数与大于零的参考电压。';return;}const n=Number(ins[0].value),ref=Number(ins[1].value),v=Number(ins[2].value);const max=2**n-1,code=Math.round(Math.max(0,Math.min(1,v/ref))*max);$('output',w).textContent=`近似码值 ${code} / ${max} · 1 LSB≈${(ref/2**n*1000).toFixed(3)} mV · 反算 ${(code/max*ref).toFixed(4)} V`; }ins.forEach(x=>x.addEventListener('input',update));update();});
  $$('[data-widget="packet"]').forEach(w=>{function update(){const text=$('input',w).value.trim();const tokens=text?text.split(/\s+/):[];if(!tokens.length||tokens.some(x=>! /^[0-9a-f]{2}$/i.test(x))){$('output',w).textContent='请输入以空格分隔的两位十六进制字节。';return;}const bytes=tokens.map(x=>parseInt(x,16));if(bytes[0]!==0xaa){$('output',w).textContent='帧头应为 AA。';return;}if(bytes.length<4){$('output',w).textContent='还不是完整帧，至少需要 4 字节。';return;}const len=bytes[2];if(len>124){$('output',w).textContent='载荷长度超过 124，必须拒绝。';return;}if(bytes.length!==len+4){$('output',w).textContent=`长度不符：需要 ${len+4} 字节，实际 ${bytes.length} 字节。`;return;}$('output',w).textContent=(bytes.reduce((s,x)=>s+x,0)&255)===0?`完整有效帧 · 类型 0x${tokens[1]} · 载荷 ${len} 字节`:'校验失败：整帧累加低 8 位应为 0。';} $('input',w).addEventListener('input',update);update();});
  $$('[data-widget="proto"]').forEach(w=>{const ins=$$('input',w);function update(){const x=Number(ins[0].value),y=Number(ins[1].value),norm=Math.hypot(x,y);const a=norm?x/norm:0,b=norm?y/norm:0;$('output',w).textContent=norm?`归一化特征 [${a.toFixed(3)}, ${b.toFixed(3)}] · 更接近 ${a>b?'A=[1,0]':b>a?'B=[0,1]':'两类等距'}，点积不是概率`:'零向量没有方向，应拒绝或单独处理。';const pt=$('circle[data-point]',w);pt?.setAttribute('cx',String(180+a*90));pt?.setAttribute('cy',String(140-b*90));}ins.forEach(x=>x.addEventListener('input',update));update();});
  $$('[data-widget="animation"]').forEach(w=>{$('button',w)?.addEventListener('click',e=>{w.classList.toggle('paused');e.currentTarget.textContent=w.classList.contains('paused')?'播放示意动画':'暂停示意动画';});});
  progress();
})();
