import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const read = (path) =>
  readFile(new URL(`../${path}`, import.meta.url), 'utf8');

test('S3 local strip is an optional independently addressed lighting target', async () => {
  const main = await read('src/main.cpp');
  assert.match(main, /SOZO_LOCAL_LIGHT_ENABLED/);
  assert.match(main, /LightNodeRuntime localLightNode/);
  assert.match(main, /LocalLightingTargetAdapter/);
  assert.match(main, /runtime_\.applyScene\(/);
  assert.match(main, /LightSceneTarget::Independent/);
  assert.match(main, /SceneDeliveryCoordinator sceneDelivery/);
  assert.match(main, /nodeCoordinator\.tick\(now, scene, audio\)/);
  assert.match(main, /sceneDelivery\.tick\(now\)/);
  assert.doesNotMatch(main, /CommandRouter commandRouter\(lightingController/);
});

test('serial manual pixel intent is not limited by the S3 local strip', async () => {
  const serial = await read('lib/SozoSerial/src/SerialConsole.cpp');
  assert.match(serial, /requestedCount > spatial_light::kMaxLedCount/);
  assert.match(serial, /LightingParameter::ManualLitPixelCount/);
  assert.doesNotMatch(serial, /requestedCount > activeCount/);
});
