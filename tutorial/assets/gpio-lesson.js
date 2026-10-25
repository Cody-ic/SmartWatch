/* An educational model of the high-active LED circuit. No device access. */
(() => {
  'use strict';
  const root = document.getElementById('gpio-output-demo');
  if (!root) return;
  let high = false;
  const led = root.querySelector('.gpio-demo-led');
  const status = root.querySelector('output');
  const statement = root.querySelector('.gpio-demo-code code');
  root.querySelectorAll('[data-gpio]').forEach(button => {
    button.addEventListener('click', () => {
      const action = button.dataset.gpio;
      high = action === 'toggle' ? !high : action === 'set';
      led.classList.toggle('lit', high);
      status.textContent = high ? 'PA5：高电平 · LED 亮' : 'PA5：低电平 · LED 灭';
      statement.textContent = action === 'toggle'
        ? 'HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);'
        : `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_${high ? 'SET' : 'RESET'});`;
    });
  });
})();
