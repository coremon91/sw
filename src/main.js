const feeds = [
  { id: 1, name: 'CAM 1', meta: 'DeckLink 8K Pro · 1', scene: 'studio', label: 'STUDIO A' },
  { id: 2, name: 'CAM 2', meta: 'DeckLink 8K Pro · 2', scene: 'stage', label: 'MAIN STAGE' },
  { id: 3, name: 'CAM 3', meta: 'DeckLink Duo 2 · 1', scene: 'desk', label: 'NEWS DESK' },
  { id: 4, name: 'REMOTE', meta: 'SRT · Guest feed', scene: 'remote', label: 'REMOTE 01' },
  { id: 5, name: 'MEDIA 1', meta: 'Playback · Opening', scene: 'media', label: 'OPENING FILM' },
  { id: 6, name: 'GFX', meta: 'Fill + Key · 1080p50', scene: 'gfx', label: 'LOWER THIRD' },
];

let preview = 2;
let program = 1;
let transition = 'Mix';
let duration = 12;

const sceneMarkup = (scene, label, compact = false) => `
  <div class="scene scene-${scene} ${compact ? 'compact' : ''}">
    <div class="scene-glow"></div>
    <div class="scene-grid"></div>
    ${scene === 'remote' ? '<div class="remote-person"></div><div class="remote-window"></div>' : ''}
    ${scene === 'gfx' ? '<div class="graphic-card"><b>VISION</b><span>LIVE UPDATE</span></div>' : ''}
    ${scene === 'media' ? '<div class="media-ring"></div>' : ''}
    <div class="scene-title">${label}</div>
  </div>`;

const icon = (name) => ({
  grid: '<svg viewBox="0 0 24 24"><path d="M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z"/></svg>',
  layers: '<svg viewBox="0 0 24 24"><path d="m12 3 9 5-9 5-9-5 9-5Zm-7 9 7 4 7-4M5 16l7 4 7-4"/></svg>',
  media: '<svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="2"/><path d="m8 14 3-3 5 5M15 10h.01"/></svg>',
  settings: '<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.88l.06.06-2.83 2.83-.06-.06A1.7 1.7 0 0 0 15 19.4a1.7 1.7 0 0 0-1 .6H10a1.7 1.7 0 0 0-1-.6 1.7 1.7 0 0 0-1.88.34l-.06.06-2.83-2.83.06-.06A1.7 1.7 0 0 0 4.6 15a1.7 1.7 0 0 0-.6-1v-4a1.7 1.7 0 0 0 .6-1 1.7 1.7 0 0 0-.34-1.88l-.06-.06 2.83-2.83.06.06A1.7 1.7 0 0 0 9 4.6a1.7 1.7 0 0 0 1-.6h4a1.7 1.7 0 0 0 1 .6 1.7 1.7 0 0 0 1.88-.34l.06-.06 2.83 2.83-.06.06A1.7 1.7 0 0 0 19.4 9c.14.37.34.71.6 1v4c-.26.29-.46.63-.6 1Z"/></svg>',
})[name];

document.querySelector('#app').innerHTML = `
  <aside class="sidebar">
    <div class="brand"><span class="brand-mark"><i></i><i></i></span><span>VISION<b>MIX</b></span></div>
    <nav>
      <button class="nav-item active">${icon('grid')}<span>Switcher</span></button>
      <button class="nav-item">${icon('layers')}<span>Graphics</span></button>
      <button class="nav-item">${icon('media')}<span>Media</span></button>
    </nav>
    <button class="nav-item settings">${icon('settings')}<span>Settings</span></button>
    <div class="user"><span>MK</span><div><b>Min Kim</b><small>Director</small></div><i></i></div>
  </aside>
  <main>
    <header>
      <div><h1>Live Production</h1><p>Studio A · Evening News</p></div>
      <div class="header-actions">
        <div class="status-pill"><span></span> DeckLink connected <b>4 / 4</b></div>
        <div class="clock" id="clock">--:--:--</div>
        <button class="icon-button">•••</button>
      </div>
    </header>
    <section class="workspace">
      <div class="monitors">
        <div class="monitor preview-monitor">
          <div class="monitor-head"><span><i></i>PREVIEW</span><b>CAM 2</b></div>
          <div class="monitor-screen" id="preview-screen">${sceneMarkup('stage', 'MAIN STAGE')}</div>
          <div class="audio-meter"><span>L</span><i style="--level:62%"></i><span>R</span><i style="--level:54%"></i></div>
        </div>
        <div class="monitor program-monitor">
          <div class="monitor-head"><span><i></i>PROGRAM</span><b>CAM 1</b></div>
          <div class="monitor-screen" id="program-screen">${sceneMarkup('studio', 'STUDIO A')}</div>
          <div class="on-air"><i></i> ON AIR</div>
          <div class="audio-meter"><span>L</span><i style="--level:76%"></i><span>R</span><i style="--level:69%"></i></div>
        </div>
      </div>
      <div class="source-section">
        <div class="section-title"><div><h2>Sources</h2><span>6 active inputs</span></div><button class="add-source">＋ Add source</button></div>
        <div class="source-grid" id="source-grid"></div>
      </div>
      <section class="control-surface">
        <div class="transition-panel">
          <div class="panel-label">TRANSITION</div>
          <div class="transition-tabs">${['Cut','Mix','Dip','Wipe'].map(t => `<button data-transition="${t}" class="${t === transition ? 'active':''}">${t}</button>`).join('')}</div>
          <div class="duration-row"><span>Duration</span><div><input id="duration" type="range" min="1" max="30" value="12"><b id="duration-value">${duration}f</b></div></div>
        </div>
        <div class="action-panel">
          <button class="take-button" id="take"><span>TAKE</span><small>Space</small></button>
          <button class="auto-button" id="auto"><span class="auto-icon">▶</span><span>AUTO</span><small>Enter</small></button>
        </div>
        <div class="output-panel">
          <div class="panel-label">OUTPUT</div>
          <div class="output-card"><span class="output-icon">↗</span><div><b>DeckLink 8K Pro</b><small>SDI 1 · 1080p50</small></div><i></i></div>
          <div class="record-row"><button id="record"><i></i><span>REC</span></button><span id="record-time">00:42:18</span><button class="stream"><i></i>STREAM</button></div>
        </div>
      </section>
    </section>
  </main>
  <div class="toast" id="toast">Program output switched</div>`;

function renderSources() {
  document.querySelector('#source-grid').innerHTML = feeds.map(feed => `
    <button class="source-card ${feed.id === preview ? 'is-preview' : ''} ${feed.id === program ? 'is-program' : ''}" data-id="${feed.id}">
      <div class="source-image">${sceneMarkup(feed.scene, feed.label, true)}<span class="source-status">${feed.id === program ? 'PGM' : feed.id === preview ? 'PVW' : ''}</span></div>
      <div class="source-info"><div><b>${feed.name}</b><small>${feed.meta}</small></div><span class="signal">▮▮▮</span></div>
    </button>`).join('');
  document.querySelectorAll('.source-card').forEach(card => card.addEventListener('click', () => setPreview(+card.dataset.id)));
}

function setPreview(id) {
  if (id === program) return;
  preview = id;
  const feed = feeds.find(f => f.id === id);
  document.querySelector('.preview-monitor .monitor-head b').textContent = feed.name;
  document.querySelector('#preview-screen').innerHTML = sceneMarkup(feed.scene, feed.label);
  renderSources();
}

function take(auto = false) {
  const oldProgram = program;
  program = preview;
  preview = oldProgram;
  const pgm = feeds.find(f => f.id === program);
  const pvw = feeds.find(f => f.id === preview);
  const pgmScreen = document.querySelector('#program-screen');
  if (auto) pgmScreen.classList.add('transitioning');
  pgmScreen.innerHTML = sceneMarkup(pgm.scene, pgm.label);
  document.querySelector('#preview-screen').innerHTML = sceneMarkup(pvw.scene, pvw.label);
  document.querySelector('.program-monitor .monitor-head b').textContent = pgm.name;
  document.querySelector('.preview-monitor .monitor-head b').textContent = pvw.name;
  setTimeout(() => pgmScreen.classList.remove('transitioning'), 450);
  renderSources();
  showToast(`${pgm.name} is now live`);
}

function showToast(message) {
  const toast = document.querySelector('#toast');
  toast.textContent = message; toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 1700);
}

renderSources();
document.querySelector('#take').addEventListener('click', () => take(false));
document.querySelector('#auto').addEventListener('click', () => take(true));
document.querySelectorAll('[data-transition]').forEach(button => button.addEventListener('click', () => {
  document.querySelectorAll('[data-transition]').forEach(b => b.classList.remove('active'));
  button.classList.add('active'); transition = button.dataset.transition;
}));
document.querySelector('#duration').addEventListener('input', e => document.querySelector('#duration-value').textContent = `${e.target.value}f`);
document.querySelector('#record').addEventListener('click', e => { e.currentTarget.classList.toggle('active'); showToast(e.currentTarget.classList.contains('active') ? 'Recording started' : 'Recording stopped'); });
document.addEventListener('keydown', e => { if (e.code === 'Space') { e.preventDefault(); take(false); } if (e.code === 'Enter') take(true); });
setInterval(() => document.querySelector('#clock').textContent = new Date().toLocaleTimeString('en-GB', { hour12: false }), 1000);
