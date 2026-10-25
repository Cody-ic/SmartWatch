/* Local educational models: 8N1, LSB first, no parity or inversion.
 * The receive model assumes a perfect start-edge detection and samples once
 * at each nominal bit centre. It is not a complete STM32 receiver model. */
(() => {
  'use strict';
  const ns = 'http://www.w3.org/2000/svg';
  const make = (tag, attrs, value) => {
    const n = document.createElementNS(ns, tag);
    Object.entries(attrs).forEach(([k, v]) => n.setAttribute(k, String(v)));
    if (value !== undefined) n.textContent = value;
    return n;
  };
  const hex = n => n.toString(16).toUpperCase().padStart(2, '0');
  const bitsOf = n => Array.from({length: 8}, (_, i) => (n >> i) & 1);
  function chart(svg, values, names, active = -1) {
    svg.replaceChildren(); svg.setAttribute('viewBox', '0 0 960 210');
    const start = 50, step = 70, y = v => v ? 65 : 140;
    if (active >= 0) svg.append(make('rect', {x: start + active * step, y: 35, width: step, height: 125, fill: '#fff0cd'}));
    let path = '';
    values.forEach((v, i) => {
      const x = start + i * step;
      svg.append(make('line', {x1: x, y1: 40, x2: x, y2: 158, stroke: '#dce1dc', 'stroke-dasharray': '3 4'}));
      svg.append(make('text', {x: x + step / 2, y: 185, 'text-anchor': 'middle', 'font-size': 14, fill: '#4b6861'}, names[i]));
      svg.append(make('text', {x: x + step / 2, y: v ? 54 : 130, 'text-anchor': 'middle', 'font-size': 17, fill: '#183d37'}, v));
      path += `${i ? 'L' : 'M'} ${x} ${y(v)} H ${x + step} `;
    });
    svg.append(make('path', {d: path, fill: 'none', stroke: '#007b68', 'stroke-width': 3}));
    svg.append(make('text', {x: 7, y: 69, 'font-size': 15, fill: '#4b6861'}, '高'));
    svg.append(make('text', {x: 7, y: 144, 'font-size': 15, fill: '#4b6861'}, '低'));
    svg.append(make('text', {x: 830, y: 208, 'font-size': 13, fill: '#4b6861'}, '时间 →'));
  }
  const frame = document.getElementById('uart-frame-lab');
  if (frame) {
    const input = frame.querySelector('input'), baud = frame.querySelector('select');
    const svg = frame.querySelector('svg'), out = frame.querySelector('output');
    const representation = frame.querySelector('[data-representation]');
    const play = frame.querySelector('[data-frame="play"]');
    let phase = 0, timer = null;
    const names = ['空闲', '起始', ...Array.from({length: 8}, (_, i) => `D${i}`), '停止', '空闲'];
    const stop = () => { if (timer !== null) clearInterval(timer); timer = null; play.textContent = '播放示意动画'; };
    function draw() {
      if (!input.validity.valid || input.value === '') { stop(); out.textContent = '请填入 0～255 的整数。'; return; }
      const n = Number(input.value), bits = bitsOf(n), values = [1, 0, ...bits, 1, 1];
      chart(svg, values, names, phase);
      const readable = n >= 32 && n <= 126 ? ` · ASCII 字符 ${String.fromCharCode(n)}` : ' · 不一定是可显示字符';
      representation.textContent = `十进制 ${n} = 0x${hex(n)} = 二进制 ${n.toString(2).padStart(8, '0')}${readable}；线上数据顺序：${bits.join(' ')}`;
      const received = bits.slice(0, Math.max(0, Math.min(8, phase - 1)));
      let action = phase === 0 ? '还没有开始发送，线路保持高电平。' : phase === 1 ? '发送起始低电平，接收端据此开始计时。' : phase <= 9 ? `现在发送 D${phase - 2}，电平表示 ${bits[phase - 2]}。` : phase === 10 ? '数据位已发完，发送停止高电平。' : '线路回到空闲；下一个字节会有自己的起始位。';
      out.textContent = `${names[phase]}：${action} 已经过的数据位：${received.length ? received.join(' ') : '无'}。一个位时间约 ${(1e6 / Number(baud.value)).toFixed(2)} μs。`;
    }
    input.addEventListener('input', () => {stop(); phase = 0; draw();});
    baud.addEventListener('change', draw);
    frame.querySelector('[data-frame="next"]').addEventListener('click', () => {stop(); phase = (phase + 1) % 12; draw();});
    frame.querySelector('[data-frame="reset"]').addEventListener('click', () => {stop(); phase = 0; draw();});
    play.addEventListener('click', () => {
      if (timer !== null) {stop(); return;}
      if (!input.validity.valid || input.value === '') {draw(); return;}
      play.textContent = '暂停动画';
      timer = setInterval(() => {phase = (phase + 1) % 12; draw(); if (phase === 11) stop();}, 700);
    });
    document.addEventListener('visibilitychange', () => {if (document.hidden) stop();});
    draw();
  }
  const receiver = document.getElementById('uart-baud-lab');
  if (receiver) {
    const tx = receiver.querySelector('[data-baud="tx"]'), rx = receiver.querySelector('[data-baud="rx"]');
    const out = receiver.querySelector('output'), svg = receiver.querySelector('svg');
    function update() {
      const a = Number(tx.value), b = Number(rx.value), ratio = a / b;
      const original = 65, data = bitsOf(original), frameBits = [0, ...data, 1];
      const signalAt = t => t >= 0 && t < 10 ? frameBits[Math.floor(t)] : 1;
      const samples = Array.from({length: 8}, (_, i) => signalAt((1.5 + i) * ratio));
      const value = samples.reduce((sum, bit, i) => sum + (bit << i), 0);
      const stopBit = signalAt(9.5 * ratio);
      chart(svg, [1, ...frameBits, 1], ['空闲', '起始', ...Array.from({length: 8}, (_, i) => `D${i}`), '停止', '空闲']);
      let outside = 0;
      for (let i = 0; i < 8; i++) {
        const at = (1.5 + i) * ratio;
        if (at > 11) {outside++; continue;}
        const x = 50 + (1 + at) * 70;
        svg.append(make('circle', {cx: x, cy: samples[i] ? 65 : 140, r: 6, fill: '#c47912', stroke: '#fffefb', 'stroke-width': 2}));
      }
      out.textContent = `发送位宽 ${(1e6/a).toFixed(2)} μs；接收位宽 ${(1e6/b).toFixed(2)} μs。本模型采到 ${samples.join(' ')} → 0x${hex(value)}（${value}），${value === original ? '此字节数值相同' : '与原发 0x41 不同'}；停止位读到 ${stopBit}。${outside ? `${outside} 个采样点已超出图示范围，落入后续空闲。` : ''}`;
    }
    tx.addEventListener('change', update); rx.addEventListener('change', update); update();
  }
})();
