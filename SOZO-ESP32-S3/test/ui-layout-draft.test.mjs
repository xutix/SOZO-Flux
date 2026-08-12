import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

class ClassList {
  #tokens = new Set();

  toggle(token, force) {
    const enabled = force === undefined ? !this.#tokens.has(token) : force;
    if (enabled) this.#tokens.add(token);
    else this.#tokens.delete(token);
    return enabled;
  }

  contains(token) {
    return this.#tokens.has(token);
  }
}

class Element {
  constructor() {
    this.attributes = {};
    this.classList = new ClassList();
    this.checked = false;
    this.children = [];
    this.className = '';
    this.parentElement = null;
    this.queries = new Map();
    this.style = {};
    this.textContent = '';
    this.value = '';
  }

  replaceChildren(...children) {
    this.children = children;
    for (const child of children) child.parentElement = this;
  }

  append(...children) {
    this.children.push(...children);
    for (const child of children) child.parentElement = this;
  }

  querySelector(selector) {
    if (!this.queries.has(selector)) {
      const element = new Element();
      element.parentElement = this;
      this.queries.set(selector, element);
    }
    return this.queries.get(selector);
  }

  querySelectorAll() {
    return [];
  }

  addEventListener() {}

  contains(target) {
    if (target === this) return true;
    return [...this.children, ...this.queries.values()]
      .some(child => child.contains(target));
  }

  setAttribute(name, value) {
    this.attributes[name] = String(value);
  }

  set innerHTML(value) {
    this.html = value;
    this.children = [];
    this.queries = new Map();
    const inputValue = String(value).match(/<input[^>]*\bvalue="([^"]*)"/);
    if (inputValue) this.querySelector('input').value = inputValue[1];
  }
}

function createDocument() {
  const elements = new Map();
  return {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, new Element());
      return elements.get(id);
    },
    createElement() {
      return new Element();
    },
    querySelectorAll() {
      return [];
    },
  };
}

function findElement(root, predicate) {
  if (predicate(root)) return root;
  for (const child of root.children || []) {
    const match = findElement(child, predicate);
    if (match) return match;
  }
  return null;
}

function nodeCardTitle(card) {
  return card.children[0].children[0].textContent;
}

function extensionLedEditor(document) {
  const panel = document.getElementById('nodeControlPanel');
  const fields = document.getElementById('layoutFields');
  const section = fields.children[0];
  assert.ok(section, 'wireless-node geometry editor must be rendered');
  return { panel, section, input: section.querySelector('input') };
}

function extensionFirmwareEditor(document) {
  const panel = document.getElementById('nodeControlPanel');
  const section = panel.children.find(child => child.className === 'extension-firmware');
  assert.ok(section, 'wireless-node firmware editor must be rendered');
  return { panel, section, input: section.querySelector('#extensionFirmwareFile') };
}

function statusWith(profile) {
  return {
    wifiState: 'connected',
    mdns: 'sozo-flux.local',
    ip: '192.168.1.46',
    ssid: 'HEWOOSTUDIO',
    hubFirmware: '0.1.0-alpha',
    platformVersion: '0.1.0-alpha',
    protocolVersion: 2,
    localLightEnabled: true,
    localLightName: '',
    sceneRevision: 4,
    freeHeap: 140000,
    effect: 'BREATH',
    micAvailable: true,
    startupColor: '#3278dc',
    startupSpeed: 0.8,
    settings: {
      brightness: 90, color: '#d4fff4', rainbowStyle: 0, flowSpeed: 45,
      cometTail: 28, cometSpeed: 45, cometDensity: 1, cometBackground: 0,
      effectFlags: 0, sensitivityX100: 100, style: 1, audioColorGainX100: 100,
      audioHueDrive: 0, breathFloorPercent: 16, secondaryColor: '#f0a85a',
      pulseAmplitudePercent: 84, pulseHeightPercent: 10, animationBrightness: 220,
    },
    layout: {
      profile,
      activeCount: 144,
      maxLedCount: 1024,
      centerIndex: 71,
      leftCount: profile === 'segmented' ? 36 : 0,
      centerCount: profile === 'segmented' ? 72 : 144,
      rightCount: profile === 'segmented' ? 36 : 0,
      reversed: false,
    },
  };
}

function remoteLayout(activeCount, profile = 'continuous') {
  const quarter = Math.floor(activeCount / 4);
  return {
    profile,
    activeCount,
    maxLedCount: 512,
    centerIndex: Math.floor((activeCount - 1) / 2),
    leftCount: profile === 'segmented' ? quarter : 0,
    centerCount: profile === 'segmented' ? activeCount - quarter * 2 : activeCount,
    rightCount: profile === 'segmented' ? quarter : 0,
    reversed: false,
  };
}

function loadPageUi(serverStatus, nodeResponse = { ok: true, nodes: [] },
  nodeNameFailure = '', sceneResponse = { ok: true, scenes: [], desired: [] }) {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');
  const match = source.match(/<script>\s*([\s\S]*?)\s*<\/script>/);
  assert.ok(match, 'embedded page script must be present');
  const document = createDocument();
  const fetchCalls = [];
  const script = match[1].replace(
    'loadStatus();setInterval(()=>loadStatus(false),5000);',
    'globalThis.__ui={setLayoutProfile,saveLayout,selectControlTarget,loadStatus,loadNodes,loadScenes,openNodePairing,activateView,saveNodeName,getLayoutProfile:()=>layoutProfile,getActiveView:()=>activeView,setState:value=>state=value,setNodeState:value=>nodeState=value,setSelected:value=>selected=value,setSceneState:value=>sceneState=value,setSelectedControlTarget:value=>sceneEditorState.controlTarget=value,setSelectedSceneTarget:value=>sceneEditorState.sceneTarget=value,getSelectedSceneTarget:()=>sceneEditorState.sceneTarget,setSceneMemberSelected,sceneMemberIds,lightingForControlTarget,getState:()=>state,getFetchCalls:()=>fetchCalls,getSelectedNodeId:()=>selectedNodeId,getSelectedRemoteNode:selectedRemoteNode,getScopeValue:()=>document.getElementById(\'scopeValue\').textContent,getDraftValue:(nodeId,field=\'name\')=>formDrafts.read(nodeFieldKey(nodeId,field),undefined),renderEffects,chooseEffect,renderParameters,renderLightNodes,renderLightNodeControl,renderSpaceStatus,colorControl,createColorChooser,getParameterClass:typeof parameterLayoutClass===\'function\'?parameterLayoutClass:null,getParameterPlan:typeof parameterPlan===\'function\'?parameterPlan:null,getParameterGridClass:typeof parameterGridClass===\'function\'?parameterGridClass:null,getParameterColumns:typeof parameterColumns===\'function\'?parameterColumns:null,getParameterGridItems:typeof parameterGridItems===\'function\'?parameterGridItems:null};',
  );
  const sandbox = {
    URLSearchParams,
    clearTimeout,
    console,
    document,
    fetchCalls,
    fetch: async (url, options = {}) => {
      const requestUrl = String(url);
      fetchCalls.push({ url: requestUrl, options });
      const nodeNameFailed = requestUrl === '/api/node/name' && nodeNameFailure;
      return {
        ok: !nodeNameFailed,
        json: async () => {
          if (requestUrl.includes('/api/nodes')) return structuredClone(nodeResponse);
          if (requestUrl === '/api/node/name') {
            if (nodeNameFailed) return { ok: false, error: nodeNameFailure };
            const body = new URLSearchParams(options.body);
            const name = (body.get('name') || '').trim();
            return { ok: true, id: body.get('id'), name, usingDefault: name.length === 0 };
          }
          if (requestUrl === '/api/node/firmware') {
            return { ok: true, state: 'idle', nodeId: '00000000', progress: 0, error: 'none' };
          }
          if (requestUrl === '/api/scenes') return structuredClone(sceneResponse);
          if (requestUrl === '/api/target/lighting' ||
              requestUrl.startsWith('/api/scene')) {
            return { ok: true };
          }
          if (requestUrl === '/api/lighting') {
            const body = new URLSearchParams(options.body);
            const nextStatus = structuredClone(serverStatus);
            nextStatus.effect = body.get('effect') || nextStatus.effect;
            for (const [key, value] of body) {
              if (key !== 'effect') nextStatus.settings[key] = value;
            }
            return nextStatus;
          }
          return structuredClone(serverStatus);
        },
      };
    },
    setInterval: () => 0,
    setTimeout,
  };
  sandbox.globalThis = sandbox;
  vm.runInNewContext(script, sandbox, { filename: 'SpatialLightPage.inline.js' });
  return { document, ui: sandbox.__ui };
}

function loadColorMath() {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');
  const match = source.match(/<script>\s*([\s\S]*?)\s*<\/script>/);
  assert.ok(match, 'embedded page script must be present');
  const script = match[1].replace(
    'loadStatus();setInterval(()=>loadStatus(false),5000);',
    "globalThis.__color={hsvToHex:typeof hsvToHex==='function'?hsvToHex:null,hexToHsv:typeof hexToHsv==='function'?hexToHsv:null,presets:typeof COLOR_PRESETS==='undefined'?null:COLOR_PRESETS,createColorPresets:typeof createColorPresets==='function'?createColorPresets:null,createColorSpectrum:typeof createColorSpectrum==='function'?createColorSpectrum:null,createColorChooser:typeof createColorChooser==='function'?createColorChooser:null};",
  );
  const sandbox = {
    URLSearchParams,
    clearTimeout,
    console,
    document: createDocument(),
    fetch: async () => ({ ok: true, json: async () => statusWith('continuous') }),
    setInterval: () => 0,
    setTimeout,
  };
  sandbox.globalThis = sandbox;
  vm.runInNewContext(script, sandbox, { filename: 'SpatialLightPage.color.js' });
  return sandbox.__color;
}

test('keeps an unsaved segmented layout selected when status polling returns saved continuous layout', async () => {
  const serverStatus = statusWith('continuous');
  const { document, ui } = loadPageUi(serverStatus);
  ui.setState(statusWith('continuous'));

  ui.setLayoutProfile('segmented');
  await ui.loadStatus(false);

  assert.equal(ui.getLayoutProfile(), 'segmented');
  assert.equal(document.getElementById('segmentedButton').classList.contains('active'), true);
});

test('clears effect highlights while off and restores one after choosing an effect', async () => {
  const off = statusWith('continuous');
  off.effect = 'OFF';
  const { document, ui } = loadPageUi(off);

  await ui.loadStatus(false);
  const cardsWhileOff = document.getElementById('effectGrid').children;
  assert.equal(cardsWhileOff.filter(card => card.className.includes('selected')).length, 0);

  ui.chooseEffect('AURORA');
  const selectedCards = document.getElementById('effectGrid').children
    .filter(card => card.className.includes('selected'));
  assert.equal(selectedCards.length, 1);
  assert.match(selectedCards[0].html, /桌面极光/);
});

test('renders a reusable full-brightness HSV spectrum for lighting colors', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');
  const color = loadColorMath();

  assert.equal(typeof color.hsvToHex, 'function');
  assert.equal(typeof color.hexToHsv, 'function');
  assert.equal(color.hsvToHex(0, 1, 1), '#FF0000');
  assert.deepEqual({ ...color.hexToHsv('#FFFFFF') }, { h: 0, s: 0, v: 1 });
  assert.match(source, /className='color-spectrum'/);
  assert.match(source, /hsv\.v=1/);
  assert.match(source, /colorControl\(def\)/);
});

test('renders eight full-brightness presets including exact RGB primaries', () => {
  const color = loadColorMath();
  assert.ok(color.presets);
  assert.equal(color.presets.length, 8);
  assert.ok(color.presets.some(item => item.hex === '#FF0000'));
  assert.ok(color.presets.some(item => item.hex === '#00FF00'));
  assert.ok(color.presets.some(item => item.hex === '#0000FF'));
  assert.ok(color.presets.every(item => color.hexToHsv(item.hex).v === 1));

  const changes = [];
  const presets = color.createColorPresets('#FF0000', next => changes.push(next));
  assert.equal(presets.children.length, 8);
  const selected = presets.children.filter(button => button.className.includes('selected'));
  assert.equal(selected.length, 1);
  assert.equal(selected[0].attributes['aria-pressed'], 'true');

  const green = presets.children.find(button => button.value === '#00FF00');
  green.onclick();
  assert.deepEqual(changes, ['#00FF00']);
});

test('does not select a preset for a custom spectrum color', () => {
  const color = loadColorMath();
  const presets = color.createColorPresets('#123456', () => {});
  assert.equal(presets.children.filter(button => button.className.includes('selected')).length, 0);
});

test('combines presets and spectrum in one chooser without a mode switch', () => {
  const color = loadColorMath();
  const changes = [];
  const chooser = color.createColorChooser('#FF5266', 'Primary', 'main-color', next => changes.push(next));
  assert.equal(chooser.children.length, 2);
  assert.equal(chooser.children[0].className, 'color-presets');
  assert.equal(chooser.children[1].className, 'color-spectrum');
  assert.deepEqual(changes, []);
});

test('preset clicks work when children is a browser HTMLCollection', () => {
  const color = loadColorMath();
  const changes = [];
  const chooser = color.createColorChooser('#FF0000', 'Primary', 'main-color', next => changes.push(next));
  const presets = chooser.children[0];
  const green = presets.children.find(button => button.value === '#00FF00');

  // Element.children is an HTMLCollection in browsers: iterable, but it has
  // no Array.prototype.forEach method. Keep the fake DOM honest at this seam.
  presets.children.forEach = undefined;

  assert.doesNotThrow(() => green.onclick());
  assert.deepEqual(changes, ['#00FF00']);
});

test('direct-target preset click preserves RGB channel meaning', async () => {
  const { ui } = loadPageUi(statusWith('continuous'));
  ui.setState(statusWith('continuous'));
  ui.selectControlTarget('node:local-s3');
  const control = ui.colorControl(['color', 'Primary', 'color', true]);
  const green = findElement(control, item => item.value === '#00FF00');
  green.onclick();
  await new Promise(resolve => setTimeout(resolve, 190));

  assert.equal(ui.getState().settings.color, '#00FF00');
  const request = ui.getFetchCalls().find(call => call.url === '/api/target/lighting');
  assert.ok(request);
  assert.equal(new URLSearchParams(request.options.body).get('id'), 'local-s3');
  assert.equal(new URLSearchParams(request.options.body).get('color'), '#00FF00');
});

test('independent C3 preset click keeps pure blue and changes only the scene draft', () => {
  const { ui } = loadPageUi(statusWith('continuous'));
  const draft = { settings: { color: '#FF5266' } };
  const control = ui.createColorChooser(
    draft.settings.color,
    'Primary',
    'scene-node-51930b93-color',
    next => { draft.settings.color = next; },
  );
  const blue = findElement(control, item => item.value === '#0000FF');
  blue.onclick();

  assert.equal(draft.settings.color, '#0000FF');
  assert.equal(ui.getFetchCalls().some(call => call.url === '/api/node/lighting'), false);
});

test('keeps a newly checked scene output selected before membership is saved', () => {
  const { ui } = loadPageUi(statusWith('continuous'));
  const remoteLighting = {
    effect: 'SOLID',
    settings: { ...statusWith('continuous').settings, color: '#0000FF' },
  };
  ui.setSceneState({
    scenes: [{
      id: 7,
      name: '音乐空间',
      assignments: [{
        target: 'local-s3',
        lighting: { effect: 'MUSIC', settings: statusWith('continuous').settings },
      }],
    }],
    desired: [{ target: '51930b93', lighting: remoteLighting }],
  });
  ui.setSelectedControlTarget('scene:7');
  ui.setSelectedSceneTarget('51930b93');

  const lighting = ui.lightingForControlTarget();

  assert.equal(ui.getSelectedSceneTarget(), '51930b93');
  assert.equal(lighting.effect, 'SOLID');
  assert.equal(lighting.settings.color, '#0000FF');
});

test('keeps the membership checkmark when edit redraws an unsaved scene output', () => {
  const { ui } = loadPageUi(statusWith('continuous'));
  const scene = {
    id: 7,
    name: '音乐空间',
    assignments: [{
      target: 'local-s3',
      lighting: { effect: 'RAINBOW', settings: statusWith('continuous').settings },
    }],
  };
  ui.setSceneState({ scenes: [scene], desired: [] });
  ui.setSelectedControlTarget('scene:7');

  ui.setSceneMemberSelected(scene, '51930b93', true);
  ui.setSelectedSceneTarget('51930b93');

  assert.deepEqual(ui.sceneMemberIds(scene), ['local-s3', '51930b93']);
});

test('scene polling does not reset a newly checked output to the first member', async () => {
  const sceneResponse = {
    ok: true,
    scenes: [{
      id: 7,
      name: '音乐空间',
      assignments: [{
        target: 'local-s3',
        lighting: { effect: 'MUSIC', settings: statusWith('continuous').settings },
      }],
    }],
    desired: [],
  };
  const nodes = {
    ok: true,
    nodes: [{
      id: '51930b93', name: '桌下灯带', state: 'ready', capabilities: 17,
      lightCapable: true, bound: true, ledCount: 71,
      layout: remoteLayout(71),
    }],
  };
  const { ui } = loadPageUi(statusWith('continuous'), nodes, '', sceneResponse);
  ui.setState(statusWith('continuous'));
  ui.setNodeState(nodes);
  ui.setSceneState(sceneResponse);
  ui.setSelectedControlTarget('scene:7');
  ui.setSelectedSceneTarget('51930b93');

  await ui.loadScenes(false);

  assert.equal(ui.getSelectedSceneTarget(), '51930b93');
});

test('maps lighting controls to bento layout roles', () => {
  const { ui } = loadPageUi(statusWith('continuous'));

  assert.equal(ui.getParameterClass(['color', '主色', 'color']), 'param param-color');
  assert.equal(ui.getParameterClass(['brightness', '亮度', 'range', 0, 255, 1, '']), 'param param-long param-brightness');
  assert.equal(ui.getParameterClass(['flowSpeed', '流动速度', 'range', 1, 100, 1, '级']), 'param param-long param-range');
  assert.equal(ui.getParameterClass(['style', '风格', 'choice', []]), 'param param-long param-choice');
  assert.equal(ui.getParameterClass(['effectFlags', '轻微随机变化', 'toggle']), 'param param-long param-toggle');
});

test('uses square color cards only where a color control is needed', () => {
  const { ui } = loadPageUi(statusWith('continuous'));

  const rainbow = ui.getParameterPlan('RAINBOW');
  assert.equal(rainbow.some(def => def[2] === 'color'), false);
  assert.ok(rainbow.every(def => ui.getParameterClass(def).includes('param-long')));
  assert.equal(ui.getParameterGridClass('RAINBOW'), 'parameter-grid parameter-grid--no-color');
  assert.equal(ui.getParameterGridClass('BREATH'), 'parameter-grid');

  const singleColor = ui.getParameterPlan('BREATH');
  assert.equal(singleColor[0][2], 'color');
  assert.equal(singleColor[0][3], true);

  const dualColor = ui.getParameterPlan('GLASS_FLOW');
  assert.equal(dualColor.filter(def => def[2] === 'color').length, 2);
  assert.equal(dualColor[0][3], false);

  const glassColumns = ui.getParameterColumns('GLASS_FLOW');
  assert.equal(glassColumns.colorDefs.length, 2);
  assert.equal(glassColumns.controlDefs.length, 2);
  assert.equal(glassColumns.controlDefs[0][0], 'brightness');
});

test('defines compact bento grid and range scale markup', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');

  assert.match(source, /\.parameter-grid\{display:grid;grid-template-columns:minmax\(250px,1fr\) minmax\(0,2fr\)/);
  assert.match(source, /\.parameter-item\{display:grid/);
  assert.match(source, /\.parameter-item--color\{grid-column:1;grid-row:span 2/);
  assert.match(source, /\.parameter-item--control\{grid-column:2/);
  assert.match(source, /parameter-grid--no-color/);
  assert.match(source, /\.param-color \.color-input\{width:100%/);
  assert.match(source, /grid-auto-rows:minmax\(min-content,auto\)/);
  assert.match(source, /\.range-scale\{display:flex;justify-content:space-between/);
  assert.match(source, /\.param output\{padding:4px 7px;border-radius:999px/);
  assert.match(source, /\.color-input\{display:grid;grid-template-columns:minmax\(72px,\.28fr\) minmax\(150px,1fr\)/);
  assert.match(source, /\.color-presets\{display:grid;grid-template-columns:repeat\(2,minmax\(0,1fr\)\)/);
  assert.match(source, /\.color-spectrum\{position:relative/);
  assert.doesNotMatch(source, /color-input-switch/);
  assert.match(source, /class="range-scale"/);
  assert.match(source, /parameterLayoutClass\(def\)/);
  assert.doesNotMatch(source, /parameter-row/);
  assert.doesNotMatch(source, /parameter-slot/);
  assert.doesNotMatch(source, /parameter-column/);
  assert.doesNotMatch(source, /grid-template-columns:repeat\(12/);
  assert.equal((source.match(/function renderParameters\(/g) || []).length, 1);
});

test('assigns grid rows and spans by control type', () => {
  const { ui } = loadPageUi(statusWith('continuous'));
  const items = ui.getParameterGridItems('GLASS_FLOW');

  assert.equal(items.rowCount, 4);
  assert.equal(items.items[0].def[0], 'color');
  assert.equal(items.items[0].column, '1');
  assert.equal(items.items[0].rowSpan, 2);
  assert.equal(items.items[1].def[0], 'brightness');
  assert.equal(items.items[1].column, '2');
  assert.equal(items.items[1].row, 1);
  assert.equal(items.items[2].def[0], 'flowSpeed');
  assert.equal(items.items[2].row, 2);
});

test('renders grid items without wrapper rows', () => {
  const { document, ui } = loadPageUi(statusWith('continuous'));
  ui.setState(statusWith('continuous'));
  ui.setSelected('GLASS_FLOW');
  ui.renderParameters();

  const items = document.getElementById('parameterGrid').children;
  assert.equal(items.length, 4);
  assert.equal(items[0].className, 'parameter-item parameter-item--color');
  assert.equal(items[1].className, 'parameter-item parameter-item--control');
  assert.equal(items[0].children[0].className.includes('param-color'), true);
  assert.equal(items[1].children[0].className.includes('param-brightness'), true);
});

test('offers one physical-node list with the same geometry editor', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');

  for (const required of [
    'lightNodeList',
    'nodeControlPanel',
    'renderLightNodes',
    'renderLocalNodeControl',
    'renderRemoteNodeControl',
    'nodeGeometryMarkup',
    'saveLayout',
    '/api/node/layout',
    '灯珠空间映射',
    '这里只管理 C3 的物理配置与固件',
  ]) {
    assert.match(source, new RegExp(required.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')),
      `missing light-node interaction: ${required}`);
  }
  assert.match(source, /node\.lightCapable===true/);
  assert.match(source, /node\.otaCapable===true/);
  assert.doesNotMatch(source, /node\.capabilities/);
});

test('exposes only space, light nodes, and hub system as top-level scopes', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');
  const tabs = [...source.matchAll(/data-tab="([^"]+)"/g)].map(match => match[1]);

  assert.deepEqual(tabs, ['space', 'nodes', 'system']);
  for (const label of ['灯光控制', '灯光节点', '中枢系统', '当前控制目标', '灯珠空间映射']) {
    assert.match(source, new RegExp(label));
  }
  assert.doesNotMatch(source, /主灯|副设备|同步主灯/);
  assert.doesNotMatch(source, /data-tab="(?:lighting|layout|device)"/);
});

test('changes the visible operation scope without changing the selected node', async () => {
  const nodeResponse = { ok: true, knownCount: 1, onlineCount: 1, nodes: [
    { id: 'c3000042', lightCapable: true, state: 'ready', bound: true,
      ledCount: 60, controlMode: 0, lastSceneRevision: 4 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();

  ui.activateView('nodes');
  assert.equal(ui.getScopeValue(), '本地灯光节点');
  document.getElementById('lightNodeList').children[1].onclick();
  assert.equal(ui.getScopeValue(), '无线灯光节点 · C3000042');
  ui.activateView('system');
  assert.equal(ui.getScopeValue(), 'Flux Hub');
  assert.equal(ui.getSelectedNodeId(), 'c3000042');
});

test('hub-only firmware omits the local light node', async () => {
  const status = statusWith('continuous');
  status.localLightEnabled = false;
  const nodeResponse = { ok: true, nodes: [
    { id: 'c3000042', lightCapable: true, state: 'ready', bound: true,
      ledCount: 60, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(status, nodeResponse);
  ui.setState(status);

  await ui.loadNodes();

  const cards = document.getElementById('lightNodeList').children;
  assert.equal(cards.length, 1);
  assert.notEqual(nodeCardTitle(cards[0]), '本地灯光节点');
  assert.equal(ui.getSelectedNodeId(), 'c3000042');
});

test('offline wireless nodes keep their last-known configuration visible', async () => {
  const status = statusWith('continuous');
  status.localLightEnabled = false;
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', lightCapable: true, state: 'offline', bound: true,
      ledCount: 71, controlMode: 1 },
  ] };
  const { document, ui } = loadPageUi(status, nodeResponse);
  ui.setState(status);

  await ui.loadNodes();

  const panel = document.getElementById('nodeControlPanel').html;
  assert.match(panel, /已绑定 · 当前离线/);
  assert.match(panel, /71 颗灯珠 · 已绑定 · 当前离线/);
  assert.doesNotMatch(panel, /data-mode=/);
});

test('renders local and wireless names as text while retaining the stable node id', async () => {
  const status = statusWith('continuous');
  status.localLightName = '<主灯 & "A">';
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '<img src=x onerror=alert(1)>', lightCapable: true,
      state: 'ready', bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(status, nodeResponse);
  ui.setState(status);

  await ui.loadNodes();

  const cards = document.getElementById('lightNodeList').children;
  assert.equal(nodeCardTitle(cards[0]), '<主灯 & "A">');
  assert.equal(nodeCardTitle(cards[1]), '<img src=x onerror=alert(1)>');
  assert.match(cards[1].children[1].children[0].textContent, /51930B93/);
  assert.doesNotMatch(cards[1].html || '', /onerror/);
  cards[1].onclick();
  assert.equal(document.getElementById('nodeControlName').textContent,
    '<img src=x onerror=alert(1)>');
});

test('keeps an unsaved node-name draft across node polling', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '旧名称', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));

  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const input = document.getElementById('nodeNameInput');
  input.value = '尚未保存的新名称';
  input.oninput();

  await ui.loadNodes();

  assert.equal(document.getElementById('nodeNameInput').value, '尚未保存的新名称');
  assert.equal(ui.getDraftValue('51930b93'), '尚未保存的新名称');
});

test('keeps a wireless-node LED-count draft across node polling', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const before = extensionLedEditor(document);
  before.input.value = '123';
  assert.equal(typeof before.input.oninput, 'function');
  before.input.oninput();
  nodeResponse.nodes[0].ledCount = 72;

  await ui.loadNodes();

  const after = extensionLedEditor(document);
  assert.equal(after.input.value, '123');
  const remoteCard = document.getElementById('lightNodeList').children[1];
  assert.match(remoteCard.children[1].children[1].textContent, /72/);
});

test('does not replace the focused node editor during polling', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const before = extensionLedEditor(document);
  assert.equal(typeof before.panel.onfocusin, 'function');
  before.panel.onfocusin();
  before.input.value = '尚在输入法组合态';

  await ui.loadNodes();

  const after = extensionLedEditor(document);
  assert.equal(after.section, before.section);
  assert.equal(after.input.value, '尚在输入法组合态');

  before.panel.onfocusout({ relatedTarget: null });
  nodeResponse.nodes[0].ledCount = 72;
  await ui.loadNodes();

  const refreshed = extensionLedEditor(document);
  assert.notEqual(refreshed.section, before.section);
  assert.equal(refreshed.input.value, '72');
});

test('keeps editable drafts isolated between wireless nodes', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
    { id: 'ffd4b7f1', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 60, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();

  document.getElementById('lightNodeList').children[1].onclick();
  let editor = extensionLedEditor(document);
  editor.input.value = '123';
  editor.input.oninput();

  document.getElementById('lightNodeList').children[2].onclick();
  editor = extensionLedEditor(document);
  assert.equal(editor.input.value, '60');
  editor.input.value = '80';
  editor.input.oninput();

  document.getElementById('lightNodeList').children[1].onclick();
  assert.equal(extensionLedEditor(document).input.value, '123');
  document.getElementById('lightNodeList').children[2].onclick();
  assert.equal(extensionLedEditor(document).input.value, '80');
});

test('does not discard a selected firmware file during node polling', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0, otaCapable: true,
      firmware: '0.1.0-alpha' },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const before = extensionFirmwareEditor(document);
  before.input.files = [{ name: 'firmware.bin' }];
  before.input.onchange();

  nodeResponse.nodes[0].firmware = '0.1.1-alpha';
  await ui.loadNodes();

  const after = extensionFirmwareEditor(document);
  assert.equal(after.section, before.section);
  assert.equal(after.input.files[0].name, 'firmware.bin');
});

test('keeps a directly edited local LED layout across status polling', async () => {
  const status = statusWith('continuous');
  const { document, ui } = loadPageUi(status);
  ui.setState(status);
  ui.renderLightNodes();
  const fields = document.getElementById('layoutFields');
  const input = fields.children[0].querySelector('input');
  assert.equal(input.value, '144');
  input.value = '200';
  input.oninput();

  await ui.loadStatus(false);

  const refreshed = fields.children[0].querySelector('input');
  assert.equal(refreshed.value, '200');
});

test('uses the default name as a placeholder and limits input to 16 code points', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const input = document.getElementById('nodeNameInput');

  assert.equal(input.value, '');
  assert.equal(input.placeholder, '无线灯光节点 · 51930B93');
  input.value = '一二三四五六七八九十一二三四五六七';
  input.oninput();
  assert.equal(Array.from(input.value).length, 16);
  input.value = '😀'.repeat(16);
  input.oninput();
  assert.equal(input.value, '😀'.repeat(16));
});

test('renames an offline node on the Hub without waiting for a BLE receipt', async () => {
  const status = statusWith('continuous');
  status.localLightEnabled = false;
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '旧名称', lightCapable: true, state: 'offline',
      bound: true, ledCount: 71, controlMode: 1 },
  ] };
  const { document, ui } = loadPageUi(status, nodeResponse);
  ui.setState(status);
  await ui.loadNodes();
  ui.activateView('nodes');
  const beforeNodePolls = ui.getFetchCalls().filter(call => call.url === '/api/nodes').length;
  const input = document.getElementById('nodeNameInput');
  input.value = '西侧灯带';
  input.oninput();

  await ui.saveNodeName('51930b93');

  const rename = ui.getFetchCalls().find(call => call.url === '/api/node/name');
  assert.ok(rename);
  assert.equal(new URLSearchParams(rename.options.body).get('name'), '西侧灯带');
  assert.equal(ui.getSelectedRemoteNode().name, '西侧灯带');
  assert.equal(ui.getScopeValue(), '西侧灯带');
  assert.equal(ui.getFetchCalls().filter(call => call.url === '/api/nodes').length,
    beforeNodePolls);
});

test('keeps the node-name draft when the Hub rejects a save', async () => {
  const nodeResponse = { ok: true, nodes: [
    { id: '51930b93', name: '旧名称', lightCapable: true, state: 'ready',
      bound: true, ledCount: 71, controlMode: 0 },
  ] };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse,
    '名称保存失败');
  ui.setState(statusWith('continuous'));
  await ui.loadNodes();
  document.getElementById('lightNodeList').children[1].onclick();
  const input = document.getElementById('nodeNameInput');
  input.value = '保留这份草稿';
  input.oninput();

  await ui.saveNodeName('51930b93');

  assert.equal(ui.getDraftValue('51930b93'), '保留这份草稿');
  assert.equal(document.getElementById('nodeNameInput').value, '保留这份草稿');
  assert.match(document.getElementById('nodeMessage').textContent, /保存失败/);
});

test('an empty local name restores the default display name', async () => {
  const status = statusWith('continuous');
  status.localLightName = '工作台主灯';
  const { document, ui } = loadPageUi(status);
  ui.setState(status);
  ui.renderLightNodes();
  const input = document.getElementById('nodeNameInput');
  input.value = '   ';
  input.oninput();

  await ui.saveNodeName('local-s3');

  assert.equal(ui.getState().localLightName, '');
  assert.equal(nodeCardTitle(document.getElementById('lightNodeList').children[0]),
    '本地灯光节点');
});

test('opens a sixty-second add-node window from the hub system page', async () => {
  const response = { ok: true, nodes: [], knownCount: 0, onlineCount: 0,
    capacity: 4, pairingWindowOpen: false, pairingRemainingMs: 0 };
  const { document, ui } = loadPageUi(statusWith('continuous'), response);

  await ui.openNodePairing();

  const request = ui.getFetchCalls().find(call => call.url === '/api/nodes/pairing');
  assert.ok(request);
  assert.equal(request.options.method, 'POST');
  assert.match(document.getElementById('systemMessage').textContent, /60/);
});

test('offers one full geometry editor for each online light node', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');

  for (const required of [
    'nodeGeometryMarkup',
    'selectedLayoutKey',
    '/api/node/layout',
    '左／中／右分段',
  ]) {
    assert.match(source, new RegExp(required.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')),
      `missing C3 LED count editor: ${required}`);
  }
});

test('offers firmware selection and progress only through the C3 OTA endpoint', () => {
  const source = readFileSync(new URL('../src/SpatialLightPage.cpp', import.meta.url), 'utf8');
  for (const required of [
    'extensionFirmwareFile',
    'uploadExtensionFirmware',
    'pollExtensionFirmware',
    '/api/node/firmware',
    '已绑定且加密的 BLE',
    '首次需要通过 USB 烧录',
  ]) {
    assert.match(source, new RegExp(required.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')),
      `missing C3 firmware update interaction: ${required}`);
  }
});

test('keeps the local node selected, then renders wireless-node physical controls', async () => {
  const nodeResponse = {
    ok: true,
    nodes: [
      { id: 'c3000042', lightCapable: true, state: 'ready', bound: true, ledCount: 60, controlMode: 0 },
      { id: 'c3000099', lightCapable: false, state: 'ready', bound: true, ledCount: 0, controlMode: 0 },
    ],
  };
  const { document, ui } = loadPageUi(statusWith('continuous'), nodeResponse);
  ui.setState(statusWith('continuous'));

  await ui.loadNodes();

  assert.equal(ui.getSelectedNodeId(), 'local-s3');
  const cards = document.getElementById('lightNodeList').children;
  assert.equal(cards.length, 2);
  assert.equal(nodeCardTitle(cards[0]), '本地灯光节点');
  cards[1].onclick();
  assert.equal(ui.getSelectedNodeId(), 'c3000042');
  assert.equal(ui.getSelectedRemoteNode().ledCount, 60);
  assert.match(document.getElementById('nodeControlPanel').html, /灯珠空间映射/);
  assert.match(document.getElementById('nodeControlPanel').html, /这里只管理 C3 的物理配置与固件/);
  assert.doesNotMatch(document.getElementById('nodeControlPanel').html, /data-mode=/);
});

test('makes leftover controls full width after the color pair', () => {
  const { document, ui } = loadPageUi(statusWith('continuous'));
  ui.setState(statusWith('continuous'));
  ui.setSelected('CORNER_PULSE');
  ui.renderParameters();

  const items = document.getElementById('parameterGrid').children;
  assert.equal(items.length, 4);
  assert.equal(items[2].className, 'parameter-item parameter-item--control');
  assert.equal(items[3].className, 'parameter-item parameter-item--control-only');
});
