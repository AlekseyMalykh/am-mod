/* Execute the served page's actual JS with fragmented/coalesced fetch chunks.
 * DOM/image decoder are fixtures; this does not claim a browser FPS test. */
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const html = fs.readFileSync(process.argv[2], 'utf8');
const script = html.match(/<script>([\s\S]*)<\/script>/)[1];
const size = 230454;
async function run(corrupt) {
  const a = new Uint8Array(size).fill(7), b = new Uint8Array(size).fill(9);
  a.set([66, 77]); b.set([66, 77]);
  if (corrupt) a[0] = 0;
  const both = new Uint8Array(size * 2); both.set(a); both.set(b, size);
  // Split the BMP magic/header and cross the record boundary in one read.
  const chunks = [both.slice(0, 1), both.slice(1, 19), both.slice(19, size + 100), both.slice(size + 100)];
  const blobs = [], revoked = [], delays = [], controllers = [];
  const screen = {src: '', hidden: true}, status = {hidden: false};
  let polls = 0;
  class Controller extends AbortController {constructor(){super();controllers.push(this);}}
  const context = {
    document: {getElementById: id => id === 'screen' ? screen : status},
    URL: {createObjectURL: blob => {blobs.push(blob);return 'blob:' + blobs.length;}, revokeObjectURL: u => revoked.push(u)},
    Blob, Uint8Array, AbortController: Controller, AbortSignal,
    setTimeout: (f, n) => {delays.push(n);return delays.length;}, clearTimeout: () => {},
    fetch: async path => {
      if(path === '/frame.bmp') {++polls;throw Error('offline fallback');}
      assert.equal(path, '/stream');
      return {ok: true, body: {getReader: () => ({read: async () => chunks.length ? {value: chunks.shift(), done: false} : {done: true}})}};
    }
  };
  vm.runInNewContext(script, context);
  // All fetch/blob operations in this fixture resolve without real timers.
  for(let i = 0; i < 40; ++i) await Promise.resolve();
  assert.equal(polls, 1);
  assert.equal(controllers[0].signal.aborted, true);
  assert(delays.includes(80));
  assert.equal(blobs.length, corrupt ? 0 : 2);
  if(!corrupt) {
    assert.deepEqual(new Uint8Array(await blobs[0].arrayBuffer()), a);
    assert.deepEqual(new Uint8Array(await blobs[1].arrayBuffer()), b);
    assert.deepEqual(revoked, ['blob:1']);
  }
}
(async () => {await run(false);await run(true);console.log('PASS production page JS: split/coalesced frames, immutable Blob copies, URL cleanup, corrupt-frame rejection and polling fallback');})().catch(e => {console.error(e);process.exitCode = 1;});
