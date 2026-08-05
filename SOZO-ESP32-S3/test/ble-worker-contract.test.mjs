import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(
  new URL('../lib/SozoTransport/src/BleCentralAdapter.cpp', import.meta.url),
  'utf8',
);

function functionBody(name, nextName) {
  const start = source.indexOf(`BleCentralAdapter::${name}`);
  const end = source.indexOf(`BleCentralAdapter::${nextName}`, start + 1);
  assert.notEqual(start, -1, `missing function ${name}`);
  assert.notEqual(end, -1, `missing function ${nextName}`);
  return source.slice(start, end);
}

test('bonded reconnect reuses encryption and secures before subscribing', () => {
  const body = functionBody('discoverSecureAndRead', 'writeEnvelope');
  const encryptionCheck = body.indexOf('getConnInfo().isEncrypted()');
  const secure = body.indexOf('secureConnection(false)');
  const subscribe = body.indexOf('subscribe(true');

  assert.notEqual(encryptionCheck, -1, 'must detect an already encrypted bond');
  assert.notEqual(secure, -1, 'must support first-time security setup');
  assert.notEqual(subscribe, -1, 'must subscribe after authentication');
  assert.ok(encryptionCheck < secure, 'check encryption before requesting it');
  assert.ok(secure < subscribe, 'authenticate before subscribing');
});

test('Arduino tick and send do not call blocking NimBLE operations', () => {
  const tick = functionBody('tick', 'send');
  const send = functionBody('send', 'popInbound');
  for (const operation of [
    'connect(',
    'getService(',
    'subscribe(',
    'secureConnection(',
    'readValue(',
    'writeValue(',
  ]) {
    assert.ok(!tick.includes(operation), `tick contains ${operation}`);
    assert.ok(!send.includes(operation), `send contains ${operation}`);
  }
});
