import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(
  new URL('../lib/SozoWeb/src/WebApi.cpp', import.meta.url),
  'utf8',
);
const nodeNameStoreSource = readFileSync(
  new URL('../lib/SozoSettings/src/NodeNameStore.cpp', import.meta.url),
  'utf8',
);
const nodeNamePolicySource = readFileSync(
  new URL('../lib/SozoSettings/src/NodeNamePolicy.cpp', import.meta.url),
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

test('retains legacy device endpoints while scenes use desired per-target state', () => {
  assert.match(source, /"\/api\/node\/mode"/);
  assert.match(source, /"\/api\/node\/lighting"/);
  assert.match(source, /handleSetNodeMode/);
  assert.match(source, /handleSetNodeLighting/);
  assert.match(source, /requestNodeControlMode/);
  assert.match(source, /requestIndependentScene/);
});

test('provides named scenes with independently editable node assignments', () => {
  for (const endpoint of [
    '/api/scenes',
    '/api/scene',
    '/api/scene/assignment',
    '/api/scene/activate',
    '/api/target/lighting',
  ]) {
    assert.match(source, new RegExp(`"${endpoint.replaceAll('/', '\\/')}"`));
  }
  assert.match(source, /handleSetSceneAssignment/);
  assert.match(source, /target is not a member of this scene/);
  assert.match(source, /sourceSceneId/);
});

test('/api/status identifies the Flux Hub and optional local light node', () => {
  for (const field of [
    'hubFirmware',
    'platformVersion',
    'protocolVersion',
    'localLightEnabled',
    'sceneRevision',
  ]) {
    assert.match(source, new RegExp(`\\\\"${field}\\\\"`),
      `missing /api/status field: ${field}`);
  }
});

test('serves the embedded control page without stale browser caching', () => {
  assert.match(source, /sendHeader\("Cache-Control", "no-store, no-cache, must-revalidate"\)/);
  assert.match(source, /sendHeader\("Pragma", "no-cache"\)/);
});

test('/api/nodes exposes light capability without leaking its bit mask to the UI', () => {
  assert.match(source, /\\"lightCapable\\"/);
  assert.match(source, /Capability::LightOutput/);
});

test('persists editable local and wireless light-node names on the Hub', () => {
  assert.match(source, /"\/api\/node\/name"/);
  assert.match(source, /handleSetNodeName/);
  assert.match(source, /\\"localLightName\\"/);
  assert.match(source, /\\"name\\"/);
  assert.match(source, /nodeNames_\.saveLocalName/);
  assert.match(source, /nodeNames_\.saveNodeName/);
  assert.match(source, /escapeJson\(nodeNames_\./);
  assert.doesNotMatch(
    source.slice(source.indexOf('void WebApi::handleSetNodeName'),
      source.indexOf('void WebApi::handleNodeFirmwareUploadData')),
    /requestNode|lastReceiptMs/,
    'Hub-local names must not wait for a BLE command receipt',
  );
});

test('node-name storage validates UTF-8, caches reads, and clears aliases safely', () => {
  assert.match(nodeNamePolicySource, /decodeUtf8CodePoint/);
  assert.match(nodeNamePolicySource, /kMaxNodeNameCodePoints/);
  assert.match(nodeNamePolicySource, /codePoint >= 0x7fU && codePoint <= 0x9fU/);
  assert.match(nodeNameStoreSource, /inspectNodeName/);
  assert.match(nodeNameStoreSource, /preferences_\.remove/);
  assert.match(nodeNameStoreSource, /local_name/);
  assert.match(nodeNameStoreSource, /"n%08lx"/);
  assert.match(nodeNameStoreSource, /entry->loaded/);
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

test('provides full device-scoped geometry while retaining LED-count compatibility', () => {
  assert.match(source, /"\/api\/node\/led-count"/);
  assert.match(source, /handleSetNodeLedCount/);
  assert.match(source, /requestNodeLedCount/);
  assert.match(source, /"\/api\/node\/layout"/);
  assert.match(source, /handleSetNodeLayout/);
  assert.match(source, /requestNodeGeometry/);
  for (const field of ['layout', 'centerIndex', 'leftCount', 'centerCount', 'rightCount', 'reversed']) {
    assert.match(source, new RegExp(`\\\\"${field}\\\\"`));
  }
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
