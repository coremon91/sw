'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const net = require('node:net');
const extension = path.resolve(process.argv[2]);
const reps = new Map(), listeners = new Map(), timers = [];
const nodecg = {
  bundleConfig: { playoutMode: 'internal', weather: { serviceKey: 'test-no-network' } },
  Replicant(name, options = {}) {
    if (!reps.has(name)) reps.set(name, { value: structuredClone(options.defaultValue) });
    return reps.get(name);
  },
  listenFor(name, handler) { listeners.set(name, handler); },
  log: { info() {}, warn() {}, error() {}, debug() {} }
};
const originalConnect = net.createConnection, originalTimer = global.setTimeout;
net.createConnection = () => { throw new Error('Internal CG attempted an AMCP connection'); };
global.setTimeout = fn => { timers.push(fn); return 1; };
const send = (name, data) => new Promise((resolve, reject) => {
  assert(listeners.has(name), `Missing handler: ${name}`);
  listeners.get(name)(data, (error, result) => error ? reject(error) : resolve(result));
});
(async () => {
  try {
    require(extension)(nodecg);
    assert.equal(reps.get('swPlayout').value.mode, 'internal');
    assert.equal(reps.get('casparConnection').value.connected, true);
    for (const [message, program, preview] of [
      ['lowerThird', 'lowerThird', null], ['election', 'election', 'electionPreview'],
      ['emergency', 'emergencyTicker', 'emergencyTickerPreview'], ['logo', 'logo', 'logoPreview'],
      ['weather', 'weather', 'weatherPreview']
    ]) {
      await send(`${message}:update`, { name: 'SW 내부 합성 시험', title: '카드 없이 재생', marker: message });
      if (preview) assert.equal(reps.get(program).value.visible, false);
      await send(`${message}:show`);
      assert.equal(reps.get(program).value.visible, true);
      if (preview) assert.equal(reps.get(program).value.marker, message);
      await send(`${message}:hide`);
      assert.equal(reps.get(program).value.visible, false);
    }
    for (const timer of timers) timer();
    await Promise.resolve();
    console.log('All 5 CG types: update/TAKE/OUT passed without CasparCG or network writes.');
  } finally {
    net.createConnection = originalConnect;global.setTimeout = originalTimer;
  }
})().catch(error => {console.error(error);process.exitCode = 1;});
