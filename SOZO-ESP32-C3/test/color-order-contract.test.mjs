import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(
  new URL('../lib/SozoC3Node/src/C3LightingHardware.cpp', import.meta.url),
  'utf8',
);

test('keeps C3 main-strip output in RGB order without a channel swap', () => {
  assert.match(source, /kMainStripColorOrder\s*=\s*RGB/);
  assert.match(source, /CRGB\(color\.red, color\.green, color\.blue\)/);
  assert.doesNotMatch(source, /kMainStripColorOrder\s*=\s*GRB/);
});
