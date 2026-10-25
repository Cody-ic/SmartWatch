/* Offline scheduling illustration; no hardware or real-time scheduler. */
(() => {
  'use strict';
  const lab = document.getElementById('rtos-scheduler-lab');
  if (!lab) return;
  const policy = lab.querySelector('[name="policy"]');
  const trace = lab.querySelector('.rtos-trace');
  const output = lab.querySelector('output');
  const step = lab.querySelector('[data-step]');
  let slot = 0, a = 0, b = 0;
  function reset() {
    slot = 0; a = 0; b = 0; trace.replaceChildren(); step.disabled = false;
    output.textContent = '先预测：A 做完这一格的工作后，B 有没有机会执行？';
  }
  step.addEventListener('click', () => {
    const working = slot % 4 === 0;
    const runA = policy.value === 'busy' || working;
    const cell = document.createElement('span');
    cell.className = runA ? 'task-a' : 'task-b';
    cell.textContent = `${slot + 1}: ${runA ? 'A' : 'B'}`;
    trace.append(cell);
    if (runA) a++; else b++;
    slot++;
    output.textContent = `已选择 ${slot} 次：A 执行 ${a} 格，B 执行 ${b} 格。` +
      (working ? '这一格 A 有工作，优先执行。' : policy.value === 'busy' ?
      'A 虽然没有新工作，但仍在就绪状态循环检查；B 优先级更低，无法运行。' :
      'A 没有新工作，阻塞等待；就绪的 B 得到执行机会。');
    if (slot === 16) { step.disabled = true; output.textContent += '本轮结束，可切换等待方式重新比较。'; }
  });
  policy.addEventListener('change', reset);
  lab.querySelector('[data-reset]').addEventListener('click', reset);
  reset();
})();
