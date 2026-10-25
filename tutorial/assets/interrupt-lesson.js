/* Teaching models only. No interrupt registers, device connections or real waits. */
(() => {
  'use strict';
  const flow = document.getElementById('irq-entry-lab');
  if (flow) {
    const stages = [
      ['主循环', '正在执行主循环中的普通工作。按钮事件还没有发生。'],
      ['检测事件', '按钮电平由高变低，EXTI 记录下降沿，并提出中断请求。'],
      ['响应中断', '本例已使能中断，且没有更紧急处理或屏蔽阻挡。CPU 暂停原流程，进入对应入口。'],
      ['HAL 与回调', '中断入口调用 HAL 处理函数；HAL 检查并清除相应事件标志，再调用下降沿回调。'],
      ['处理返回', '回调返回 HAL，HAL 返回入口；入口结束后，由异常返回机制恢复原执行流程。'],
      ['继续主循环', '主循环从被打断的位置继续，不是重新从 main 开头运行。']
    ];
    let phase = 0;
    const out = flow.querySelector('output');
    const draw = () => {
      flow.querySelectorAll('[data-flow-node]').forEach((n, i) => n.classList.toggle('active', i === phase));
      out.textContent = `第 ${phase + 1} 步 · ${stages[phase][0]}：${stages[phase][1]}`;
    };
    flow.querySelector('[data-flow="next"]').addEventListener('click', () => {phase = (phase + 1) % stages.length; draw();});
    flow.querySelector('[data-flow="reset"]').addEventListener('click', () => {phase = 0; draw();});
    draw();
  }
  const delay = document.getElementById('irq-delay-lab');
  if (delay) {
    const group = delay.querySelector('[data-delay="group"]');
    const current = delay.querySelector('[data-delay="current"]');
    const clock = delay.querySelector('[data-delay="clock"]');
    const currentSub = delay.querySelector('[data-delay="current-sub"]');
    const clockSub = delay.querySelector('[data-delay="clock-sub"]');
    const enabled = delay.querySelector('[data-delay="enabled"]');
    const masked = delay.querySelector('[data-delay="masked"]');
    const result = delay.querySelector('output');
    const trace = delay.querySelector('ol');
    let phase = 0, count = 100, finished = false;
    function reason() {
      if (!enabled.checked) return '时基中断没有获准响应';
      if (masked.checked) return '可配置中断被屏蔽';
      if (Number(clock.value) === Number(current.value)) return '抢占优先级相同，子优先级不能让时基抢占当前 ISR';
      if (Number(clock.value) > Number(current.value)) return '时基的抢占优先级更低，必须等待当前 ISR 退出';
      return '';
    }
    function show() {
      delay.querySelector('[data-count]').textContent = String(count);
      const valid = [current, clock, currentSub, clockSub].every(x => x.validity.valid && x.value !== '');
      delay.querySelector('[data-delay="step"]').disabled = !valid;
      if (!valid) {result.textContent = '请在当前分组允许的范围内填写整数优先级。'; return;}
      const why = reason();
      if (finished) result.textContent = '计数已从 100 增加到 103：模型中的等待结束，当前 ISR 可以返回，主循环恢复。';
      else if (phase === 0) result.textContent = '尚未进入等待。点击“推演下一步”，先进入当前中断中的延时。';
      else if (why) result.textContent = `计数停在 ${count}：${why}。当前 ISR 的延时继续等待；时基不能因此获得执行机会。`;
      else result.textContent = `时基可以抢占，当前计数 ${count}；继续推演，累计三个计数增量后，本模型的等待才结束。`;
    }
    function reset() {
      phase = 0; count = 100; finished = false;
      trace.replaceChildren(); show();
    }
    function grouping() {
      const split = group.value === '3';
      [current, clock].forEach(x => {x.max = split ? '7' : '15'; if (Number(x.value) > Number(x.max)) x.value = x.max;});
      [currentSub, clockSub].forEach(x => {x.disabled = !split; x.max = split ? '1' : '0'; if (!split) x.value = '0';});
      reset();
    }
    delay.querySelector('[data-delay="step"]').addEventListener('click', () => {
      if (finished) return;
      const line = document.createElement('li');
      if (phase === 0) {
        phase = 1; line.textContent = '进入当前 ISR，记录开始计数 100；等待计数增加 3。主循环暂停。';
      } else {
        const why = reason();
        if (why) line.textContent = `时基再次提出请求，但${why}。计数仍是 ${count}，当前 ISR 无法完成等待。`;
        else {
          count++;
          line.textContent = `时基中断抢占当前 ISR，计数增加到 ${count}；时基处理返回后，当前 ISR 继续检查。`;
          if (count >= 103) {finished = true; line.textContent += ' 已满足本模型的三个计数增量，等待结束。';}
        }
        phase++;
      }
      trace.append(line);
      if (trace.children.length > 12) trace.firstElementChild.remove();
      show();
    });
    delay.querySelector('[data-delay="reset"]').addEventListener('click', reset);
    group.addEventListener('change', grouping);
    [current, clock, currentSub, clockSub].forEach(x => x.addEventListener('input', reset));
    [enabled, masked].forEach(x => x.addEventListener('change', reset));
    grouping();
  }
  const vectors = document.getElementById('irq-vector-lab');
  if (vectors) {
    const choose = vectors.querySelector('select'), out = vectors.querySelector('output');
    const handlers = {11: 'EXTI0_IRQHandler', 15: 'EXTI4_IRQHandler', 49: 'TIM6_IRQHandler', 61: 'USART1_IRQHandler'};
    const update = () => {
      const irq = Number(choose.value), entry = irq + 16;
      out.textContent = `本项目 IRQn=${irq} → 向量表第 ${entry} 项（从 0 编号）→ 相对表起始偏移 ${entry * 4} 字节（0x${(entry * 4).toString(16).toUpperCase()}）→ ${handlers[irq]}。这是处理入口的位置，不是 NVIC 抢占优先级，也不是 HAL 回调地址。`;
    };
    choose.addEventListener('change', update); update();
  }
})();
