import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const output = readFileSync(
  new URL('../lib/SozoLightingAdapter/src/S3LightingOutput.cpp', import.meta.url),
  'utf8',
);
const main = readFileSync(new URL('../src/main.cpp', import.meta.url), 'utf8');

test('uses FastLED for the S3 strip and keeps logical RGB channel order', () => {
  assert.match(output, /kMainStripColorOrder\s*=\s*RGB/);
  assert.match(output, /FastLED\.addLeds/);
  assert.match(output, /CRGB\(color\.red, color\.green, color\.blue\)/);
});

test('uses FastLED for the onboard disabled status pixel', () => {
  assert.match(main, /FastLED\.addLeds<WS2812, kStatusLedPin, GRB>/);
});
