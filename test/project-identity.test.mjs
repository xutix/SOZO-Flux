import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

const read = (path) => readFile(new URL(`../${path}`, import.meta.url), 'utf8');

test('release manifest separates platform, firmware, node, and protocol versions', async () => {
  const manifest = await read('VERSION');
  assert.match(manifest, /^platform=0\.1\.0-alpha$/m);
  assert.match(manifest, /^gateway_s3=0\.1\.0-alpha$/m);
  assert.match(manifest, /^node_c3=0\.2\.1-alpha$/m);
  assert.match(manifest, /^protocol=1$/m);

  const versionHeader = await read(
    'SOZO-Common/lib/SozoVersion/src/SozoVersion.h',
  );
  assert.match(versionHeader, /kPlatform\[\] = "0\.1\.0-alpha"/);
  assert.match(versionHeader, /kGatewayS3\[\] = "0\.1\.0-alpha"/);
  assert.match(versionHeader, /kNodeC3\[\] = "0\.2\.1-alpha"/);

  const protocolHeader = await read(
    'SOZO-Common/lib/SozoNodeProtocol/src/SozoNodeProtocol.h',
  );
  assert.match(protocolHeader, /kProtocolVersion = 1/);
});

test('public device surfaces use the SOZO Flux identity', async () => {
  const gateway = await read('SOZO-ESP32-S3/src/main.cpp');
  assert.match(gateway, /SOZO Flux Gateway S3/);
  assert.match(gateway, /sozo-flux/);
  assert.match(gateway, /SozoVersion\.h/);

  const network = await read(
    'SOZO-ESP32-S3/lib/SozoNetwork/src/NetworkManager.cpp',
  );
  assert.match(network, /SOZO-FLUX-SETUP/);
  assert.match(network, /sozo-flux/);

  const controlPage = await read('SOZO-ESP32-S3/src/SpatialLightPage.cpp');
  assert.match(controlPage, /<title>SOZO Flux 空间灯光<\/title>/);
  assert.match(controlPage, /SOZO FLUX · SPATIAL CONTROL/);

  const setupPage = await read('SOZO-ESP32-S3/lib/SozoWeb/src/WebApi.cpp');
  assert.match(setupPage, /<title>SOZO Flux 网络设置<\/title>/);

  const node = await read('SOZO-ESP32-C3/src/main.cpp');
  assert.match(node, /SOZO Flux C3/);
  assert.match(node, /SozoVersion\.h/);

  const peripheral = await read(
    'SOZO-ESP32-C3/lib/SozoBlePeripheral/src/BlePeripheralAdapter.cpp',
  );
  assert.match(peripheral, /SOZO-FLUX-C3/);
});

test('renaming keeps persisted settings compatible', async () => {
  const settings = await read(
    'SOZO-ESP32-S3/lib/SozoSettings/src/SettingsStore.h',
  );
  assert.match(settings, /nvsNamespace = "sozo-light"/);
});
