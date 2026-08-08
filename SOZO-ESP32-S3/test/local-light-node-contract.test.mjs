import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const read = (path) =>
  readFile(new URL(`../${path}`, import.meta.url), 'utf8');

test('S3 local strip is an optional consumer of the shared space scene', async () => {
  const main = await read('src/main.cpp');
  assert.match(main, /SOZO_LOCAL_LIGHT_ENABLED/);
  assert.match(main, /LightNodeRuntime localLightNode/);
  assert.match(main, /localLightNode\.applyScene\(scene\.lighting/);
  assert.match(main, /nodeCoordinator\.tick\(now, scene, audio\)/);
  assert.doesNotMatch(main, /CommandRouter commandRouter\(lightingController/);
});

test('serial manual pixel intent is not limited by the S3 local strip', async () => {
  const serial = await read('lib/SozoSerial/src/SerialConsole.cpp');
  assert.match(serial, /requestedCount > spatial_light::kMaxLedCount/);
  assert.match(serial, /LightingParameter::ManualLitPixelCount/);
  assert.doesNotMatch(serial, /requestedCount > activeCount/);
});
