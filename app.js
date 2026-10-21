const state = { program: 'CAM 1', preview: '4K PLAYER', playing: false, cgOn: true, elapsed: 5076 };

const $ = (selector) => document.querySelector(selector);
const toast = (message) => {
  const element = $('#toast');
  element.textContent = message;
  element.classList.add('show');
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => element.classList.remove('show'), 1800);
};

function formatTime(seconds, includeHours = false) {
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const secs = seconds % 60;
  return [hours, minutes, secs]
    .slice(includeHours ? 0 : 1)
    .map((value) => String(value).padStart(2, '0'))
    .join(':');
}

function selectSource(button) {
  const bus = button.closest('[data-bus]').dataset.bus;
  const activeClass = bus === 'program' ? 'active-red' : 'active-green';
  button.parentElement.querySelectorAll('.source').forEach((item) => item.classList.remove(activeClass));
  button.classList.add(activeClass);
  state[bus] = button.dataset.source;
  $(`#${bus}Name`).textContent = state[bus] + (bus === 'program' && state.cgOn ? ' + CG' : '');
}

document.querySelectorAll('.source').forEach((button) => button.addEventListener('click', () => selectSource(button)));

function take() {
  const previousProgram = state.program;
  state.program = state.preview;
  state.preview = previousProgram;
  document.querySelectorAll('[data-bus="program"] .source').forEach((button) => button.classList.toggle('active-red', button.dataset.source === state.program));
  document.querySelectorAll('[data-bus="preview"] .source').forEach((button) => button.classList.toggle('active-green', button.dataset.source === state.preview));
  $('#programName').textContent = state.program + (state.cgOn ? ' + CG' : '');
  $('#previewName').textContent = state.preview;
  $('.program-monitor').animate([{ opacity: 0.35 }, { opacity: 1 }], { duration: 450 });
  toast(`${state.program} 소스를 송출합니다`);
}

$('#takeButton').addEventListener('click', take);
document.addEventListener('keydown', (event) => {
  if (event.code === 'Space' && !['INPUT', 'TEXTAREA'].includes(document.activeElement.tagName)) {
    event.preventDefault();
    take();
  }
});

$('#playButton').addEventListener('click', () => {
  state.playing = !state.playing;
  $('#playButton').textContent = state.playing ? 'Ⅱ' : '▶';
  toast(state.playing ? '4K Player 재생을 시작했습니다' : '재생을 일시 정지했습니다');
});

$('#loop').addEventListener('click', (event) => {
  event.currentTarget.classList.toggle('active');
  toast(event.currentTarget.classList.contains('active') ? '반복 재생 켜짐' : '반복 재생 꺼짐');
});

$('#progress').addEventListener('input', (event) => { $('#playerCurrent').textContent = formatTime(Number(event.target.value)); });
['#previous', '#next'].forEach((selector) => $(selector).addEventListener('click', () => toast('재생 목록에서 클립을 선택했습니다')));

$('#updateCg').addEventListener('click', () => {
  $('#onAirName').textContent = $('#nameInput').value.trim() || '이름 없음';
  $('#onAirRole').textContent = $('#roleInput').value.trim() || '역할 없음';
  toast('NodeCG 그래픽을 업데이트했습니다');
});

$('#cgToggle').addEventListener('click', () => {
  state.cgOn = !state.cgOn;
  $('#lowerThird').classList.toggle('hidden', !state.cgOn);
  $('#cgToggle').classList.toggle('off', !state.cgOn);
  $('#cgToggle span').textContent = state.cgOn ? 'ON AIR' : 'OFF AIR';
  $('#programName').textContent = state.program + (state.cgOn ? ' + CG' : '');
  toast(state.cgOn ? 'CG를 송출했습니다' : 'CG 송출을 종료했습니다');
});

document.querySelectorAll('.transition-settings button').forEach((button) => button.addEventListener('click', () => {
  button.parentElement.querySelectorAll('button').forEach((item) => item.classList.remove('selected'));
  button.classList.add('selected');
}));

document.querySelectorAll('.template-tabs button').forEach((button) => button.addEventListener('click', () => {
  button.parentElement.querySelectorAll('button').forEach((item) => item.classList.remove('active'));
  button.classList.add('active');
  toast(`${button.textContent} 템플릿을 선택했습니다`);
}));

document.querySelectorAll('.nav-item').forEach((button) => button.addEventListener('click', () => {
  document.querySelectorAll('.nav-item').forEach((item) => { item.classList.remove('active'); item.removeAttribute('aria-current'); });
  button.classList.add('active');
  button.setAttribute('aria-current', 'page');
  if (button.dataset.view !== 'studio') toast(`${button.textContent.trim()} 패널은 준비 중입니다`);
}));

setInterval(() => {
  const now = new Date();
  $('#clock').textContent = new Intl.DateTimeFormat('ko-KR', { timeZone: 'Asia/Seoul', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false }).format(now);
  $('#dateLabel').textContent = new Intl.DateTimeFormat('ko-KR', { timeZone: 'Asia/Seoul', year: 'numeric', month: '2-digit', day: '2-digit' }).format(now);
  state.elapsed += 1;
  $('#liveTimer').textContent = formatTime(state.elapsed, true);
  if (state.playing) {
    const progress = $('#progress');
    progress.value = (Number(progress.value) + 1) % 151;
    $('#playerCurrent').textContent = formatTime(Number(progress.value));
  }
}, 1000);
