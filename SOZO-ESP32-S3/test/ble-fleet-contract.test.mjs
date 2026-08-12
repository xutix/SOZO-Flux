import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const header = readFileSync(
  new URL('../lib/SozoTransport/src/BleFleetAdapter.h', import.meta.url),
  'utf8',
);
const source = readFileSync(
  new URL('../lib/SozoTransport/src/BleFleetAdapter.cpp', import.meta.url),
  'utf8',
);

test('fleet owns four independent BLE links', () => {
  assert.match(header, /kLinkCapacity\s*=\s*4U/);
  assert.match(header, /BleCentralAdapter links_\[kLinkCapacity\]/);
});

test('normal discovery accepts bonds while explicit pairing admits new nodes', () => {
  assert.match(source, /pairingWindowOpen\(millis\(\)\)/);
  assert.match(source, /NimBLEDevice::isBonded\(deviceAddress\)/);
  assert.match(source, /NimBLEDevice::getNumBonds\(\)/);
  assert.match(source, /hasUnassignedBond\(\)/);
});
