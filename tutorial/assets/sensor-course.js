/* Offline teaching models; no sensor access and no training data export. */
(() => {
  'use strict';
  const lab = document.getElementById('mlc-label-lab');
  if (lab) {
    const mlc = lab.querySelector('[name=mlc]');
    const human = lab.querySelector('[name=human]');
    const out = lab.querySelector('output');
    const names = {pending:'尚未确认',still:'静止',walk:'步行',run:'跑步',cycle:'骑行'};
    const codes = {'1':'OTHER','4':'WALKING','8':'RUNNING','255':'UNKNOWN'};
    const update = () => {
      const actual = human.value, raw = mlc.value;
      const record = `分别记录：人工=${names[actual]}；MLC=${raw}/${codes[raw]}。`;
      let advice;
      if (actual === 'pending') advice = '先保留机器建议，安排人工确认与窗口时间复核；此时只有预测，没有人工确认标签。';
      else if (raw === '255') advice = '机器结果无效，检查通信或等待分类；原始数据和已确认的人工记录仍可单独保留。';
      else if (raw === '1') advice = 'OTHER 覆盖多个非走跑动作，继续保留具体人工标签；它不能给出“静止”或“骑行”的细分类结论。';
      else if ((raw === '4' && actual === 'walk') || (raw === '8' && actual === 'run')) advice = '粗类别一致；继续核对数据完整性、动作过渡和结果延迟，再决定训练用途。';
      else advice = '这是优先复核的分歧片段：检查实际动作、佩戴与时间对应。保留两份来源，分歧本身不能确定谁正确。';
      out.textContent = record + advice;
    };
    mlc.addEventListener('change', update);
    human.addEventListener('change', update);
    update();
  }
})();
