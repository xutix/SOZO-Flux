import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(
  new URL('../lib/SozoWeb/src/WebApi.cpp', import.meta.url),
  'utf8',
);

test('/api/nodes keeps its existing fields and exposes BLE worker diagnostics', () => {
  for (const field of [
    'bleState',
    'ready',
    'nodes',
    'operationStage',
    'workerBusy',
    'bleTimeouts',
  ]) {
    assert.match(source, new RegExp(`\\\\"${field}\\\\"`),
      `missing /api/nodes field: ${field}`);
  }
});

test('provides device-scoped endpoints for control mode and independent scenes', () => {
  assert.match(source, /"\/api\/node\/mode"/);
  assert.match(source, /"\/api\/node\/lighting"/);
  assert.match(source, /handleSetNodeMode/);
  assert.match(source, /handleSetNodeLighting/);
  assert.match(source, /requestNodeControlMode/);
  assert.match(source, /requestIndependentScene/);
});

test('/api/nodes exposes fleet capacity and an explicit pairing window', () => {
  for (const field of [
    'knownCount',
    'onlineCount',
    'capacity',
    'scanning',
    'pairingWindowOpen',
    'pairingRemainingMs',
  ]) {
    assert.match(source, new RegExp(`\\\\"${field}\\\\"`),
      `missing fleet field: ${field}`);
  }
  assert.match(source, /"\/api\/nodes\/pairing"/);
  assert.match(source, /handleOpenNodePairing/);
  assert.match(source, /openPairingWindow/);
});

test('independent scene request applies the selected effect to the scene state', () => {
  const start = source.indexOf('bool WebApi::parseLightingRequest');
  const end = source.indexOf('void WebApi::handleSetLighting', start);
  assert.ok(start >= 0 && end > start, 'lighting request parser was not found');
  const parser = source.slice(start, end);
  assert.match(
    parser,
    /next\.mode\s*=\s*requestedMode\s*;/,
    'the parsed effect must be written into the state sent to the C3',
  );
});

test('provides one device-scoped endpoint for the C3 LED count', () => {
  assert.match(source, /"\/api\/node\/led-count"/);
  assert.match(source, /handleSetNodeLedCount/);
  assert.match(source, /requestNodeLedCount/);
});

test('provides one encrypted BLE firmware workflow for OTA-capable C3 nodes', () => {
  assert.match(source, /"\/api\/node\/firmware"/);
  assert.match(source, /handleNodeFirmwareUploadData/);
  assert.match(source, /handleGetNodeFirmwareStatus/);
  assert.match(source, /requestNodeFirmwareUpdate/);
  for (const field of ['otaCapable', 'firmware', 'confirmedBytes', 'progress']) {
    assert.match(source, new RegExp(`\\\\"${field}\\\\"`),
      `missing firmware field: ${field}`);
  }
});
