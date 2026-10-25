/* Browser teaching models, not an embedded LVGL runtime. */
(() => {
  'use strict';
  for(const lab of document.querySelectorAll('[data-ui-lab]')){
    const out=lab.querySelector('output'),kind=lab.dataset.uiLab;
    if(kind==='buffer'||kind==='performance'){
      const inputs=[...lab.querySelectorAll('input')];
      function update(){
        if(inputs.some(i=>i.value===''||!i.validity.valid)){out.textContent='请在给定范围内填写数值。';return;}
        if(kind==='buffer'){
          const rows=Number(inputs[0].value),double=inputs[1].checked;
          out.textContent=`每块 ${240*rows} 像素、${480*rows} 字节；${double?'两':'一'}块共 ${480*rows*(double?2:1)} 字节。若整屏按完整条带拆分，至少约 ${Math.ceil(240/rows)} 批。只改缓冲区大小不会减少整屏的 115200 个像素字节。`;
        }else{
          const [w,h,mhz]=inputs.map(i=>Number(i.value)),bytes=w*h*2;
          out.textContent=`区域 ${w}×${h}：${bytes} 字节；理想纯像素传输 ${(bytes*8/(mhz*1000)).toFixed(2)} ms。尚未加绘图、命令、调度和软件开销，不能直接当作实际帧率。${bytes>65535?' 当前驱动单次计数会超限，须分块。':''}`;
        }
      }inputs.forEach(i=>i.addEventListener('input',update));update();
    }
    if(kind==='label'){
      let count=0;
      lab.querySelector('button').addEventListener('click',()=>{count++;lab.querySelector('[data-label]').textContent=`Count: ${count}`;out.textContent=`仍然是同一个标签对象，只把文本改为 Count: ${count}；本模型没有新建第二个标签。`;});
    }
    if(kind==='layout'){
      const input=lab.querySelector('input'),select=lab.querySelector('select'),row=lab.querySelector('[data-layout-row]');
      const update=()=>{row.style.width=`${Number(input.value)}px`;row.style.flexDirection=select.value;out.textContent=`容器宽度 ${input.value}px，${select.value==='row'?'横向排列并允许换行':'纵向排列'}。这个网页用 CSS 表示排布效果，不是 LVGL 的精确尺寸计算。`;};
      input.addEventListener('input',update);select.addEventListener('change',update);update();
    }
    if(kind==='event'){
      let alive=true,count=0;
      const click=lab.querySelector('[data-event=click]');
      click.addEventListener('click',()=>{if(!alive)return;count++;lab.querySelector('[data-count]').textContent=count;out.textContent=`模拟 CLICKED → 注册的回调 → 应用计数加一 → 修改标签，当前 ${count}。这里不是 MCU 硬件中断。`;});
      lab.querySelector('[data-event=delete]').addEventListener('click',()=>{alive=false;click.disabled=true;out.textContent='页面已删除：旧对象不可再使用，必须先停止引用它的定时器或回调。';});
      lab.querySelector('[data-event=create]').addEventListener('click',()=>{alive=true;count=0;click.disabled=false;lab.querySelector('[data-count]').textContent='0';out.textContent='重新创建页面，重新取得对象指针并注册事件。';});
    }
    if(kind==='data'){
      let now=0,sample={value:72,time:0,valid:false};
      const update=()=>{const fresh=sample.valid&&now-sample.time<3000;lab.querySelector('[data-value]').textContent=fresh?String(sample.value):'--';lab.querySelector('[data-state]').textContent=fresh?'示例数据有效':sample.valid?'示例数据已过期':'暂无有效示例数据';out.textContent=`模型时间 ${now} ms；样本时间 ${sample.time} ms；年龄 ${now-sample.time} ms。这里采用 3 秒教学阈值，不代表项目实际阈值或医学判断。`;};
      lab.querySelector('[data-data=sample]').addEventListener('click',()=>{sample={value:72,time:now,valid:true};update();});
      lab.querySelector('[data-data=advance]').addEventListener('click',()=>{now+=3000;update();});
      lab.querySelector('[data-data=invalid]').addEventListener('click',()=>{sample.valid=false;update();});update();
    }
  }
})();
