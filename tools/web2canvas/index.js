#!/usr/bin/env node
// web2canvas — render a React/HTML page in a headless browser and convert the
// computed DOM into figmalib canvas.json (the same format fig2json emits and
// figmalib / fapp2godot consume).
//
//   node index.js <url|file.html> [-o out.canvas.json] [--root SELECTOR]
//                 [--viewport WxH] [--browser msedge|chrome] [--wait MS]
//                 [--scale N]
//
// Solid boxes, borders, corner radius, drop shadow and text map to native
// canvas.json nodes. Anything a flat fill can't reproduce — CSS gradients,
// background-images, <img>, inline <svg>, clip-path cut-corners — is captured
// as a per-element PNG (rasterized by the browser) and emitted as an IMAGE
// fill, matching the project's vector-to-texture goal.

const fs = require('fs');
const path = require('path');
const http = require('http');
const { chromium } = require('playwright-core');

// ---- CLI ------------------------------------------------------------------

function parseArgs(argv) {
  const a = { input: null, out: null, root: 'body', vw: 1280, vh: 720,
              browser: 'msedge', wait: 400, scale: 2 };
  for (let i = 2; i < argv.length; i++) {
    const t = argv[i];
    if (t === '-o' || t === '--out') a.out = argv[++i];
    else if (t === '--root') a.root = argv[++i];
    else if (t === '--viewport') { const m = /(\d+)x(\d+)/.exec(argv[++i] || ''); if (m) { a.vw = +m[1]; a.vh = +m[2]; } }
    else if (t === '--browser') a.browser = argv[++i];
    else if (t === '--wait') a.wait = +argv[++i];
    else if (t === '--scale') a.scale = Math.max(1, +argv[++i]);
    else if (!t.startsWith('-')) a.input = t;
  }
  return a;
}

function toUrl(input) {
  if (/^https?:\/\//.test(input) || input.startsWith('file:')) return input;
  return 'file:///' + encodeURI(path.resolve(input).replace(/\\/g, '/'));
}

// Sandboxes often can't reach unpkg/jsdelivr. Serve CDN scripts from locally
// vendored node_modules (byte-identical to the npm tarball → subresource
// integrity hashes still pass), and abort web-font requests so networkidle
// settles (figmalib supplies its own fonts).
const MIME = { '.js': 'application/javascript', '.mjs': 'application/javascript',
               '.jsx': 'application/javascript', '.css': 'text/css', '.json': 'application/json',
               '.map': 'application/json', '.html': 'text/html', '.htm': 'text/html',
               '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg',
               '.gif': 'image/gif', '.webp': 'image/webp', '.svg': 'image/svg+xml',
               '.ttf': 'font/ttf', '.otf': 'font/otf', '.woff': 'font/woff', '.woff2': 'font/woff2',
               '.mp4': 'video/mp4' };

// Babel-in-browser fetches the .jsx modules via XHR, which file:// blocks
// (CORS). Serve the input's directory over a throwaway local HTTP server so
// real React apps load.
function startStaticServer(rootDir) {
  return new Promise((resolve) => {
    const server = http.createServer((req, res) => {
      try {
        const rel = decodeURIComponent((req.url || '/').split('?')[0]);
        const file = path.normalize(path.join(rootDir, rel));
        if (!file.startsWith(rootDir)) { res.writeHead(403); return res.end(); }
        const body = fs.readFileSync(file);
        res.writeHead(200, { 'content-type': MIME[path.extname(file).toLowerCase()] || 'application/octet-stream',
                             'access-control-allow-origin': '*' });
        res.end(body);
      } catch (e) { res.writeHead(404); res.end('not found'); }
    });
    server.listen(0, '127.0.0.1', () => resolve(server));
  });
}
async function setupCdnRoutes(page) {
  const nm = path.join(__dirname, 'node_modules');
  await page.route(/(unpkg\.com|cdn\.jsdelivr\.net\/npm)\//, (route) => {
    try {
      const u = new URL(route.request().url());
      const segs = u.pathname.replace(/^\/(npm\/)?/, '').split('/');
      let pkg, rest;
      if (segs[0].startsWith('@')) { pkg = segs[0] + '/' + segs[1].replace(/@.*/, ''); rest = segs.slice(2).join('/'); }
      else { pkg = segs[0].replace(/@.*/, ''); rest = segs.slice(1).join('/'); }
      const file = path.join(nm, pkg, rest);
      const body = fs.readFileSync(file);
      route.fulfill({ status: 200, contentType: MIME[path.extname(file)] || 'application/octet-stream', body });
    } catch (e) { route.abort(); }
  });
  await page.route(/fonts\.(googleapis|gstatic)\.com/, r => r.abort());
}

// ---- browser-side collector ----------------------------------------------
// Serialized into the page; tags raster elements with data-w2c and returns a
// plain tree. The Node side then screenshots the tagged elements.

function collectorFn(rootSelector) {
  const root = document.querySelector(rootSelector) || document.body;
  let rid = 0;

  function visible(el, cs) {
    if (cs.display === 'none' || cs.visibility === 'hidden') return false;
    if (parseFloat(cs.opacity) === 0) return false;
    const r = el.getBoundingClientRect();
    return r.width >= 1 && r.height >= 1;
  }
  function directText(el) {
    if (el.children.length > 0) return null;
    const t = (el.textContent || '').replace(/\s+/g, ' ').trim();
    return t.length ? t : null;
  }
  function hasRealBg(cs) {
    const bi = cs.backgroundImage;
    return bi && bi !== 'none';  // gradient or url(...)
  }

  function node(el, originX, originY) {
    const cs = getComputedStyle(el);
    if (!visible(el, cs)) return null;
    const r = el.getBoundingClientRect();
    const tag = el.tagName.toLowerCase();
    const text = directText(el);

    // What needs rasterizing?
    const wholeRaster = (tag === 'img' || tag === 'svg' || tag === 'canvas' || tag === 'video');
    const clipped = cs.clipPath && cs.clipPath !== 'none';
    const bgRaster = hasRealBg(cs) || clipped;
    let raster = null, rasterWhole = false, rasterHideKids = false;
    if (wholeRaster) { raster = ++rid; rasterWhole = true; }
    else if (bgRaster) { raster = ++rid; rasterHideKids = el.children.length > 0; }
    if (raster) el.setAttribute('data-w2c', String(raster));

    const out = {
      tag,
      rect: { x: r.left - originX, y: r.top - originY, w: r.width, h: r.height },
      text,
      bg: cs.backgroundColor,
      radius: [cs.borderTopLeftRadius, cs.borderTopRightRadius,
               cs.borderBottomRightRadius, cs.borderBottomLeftRadius].map(v => parseFloat(v) || 0),
      borderW: parseFloat(cs.borderTopWidth) || 0,
      borderColor: cs.borderTopColor,
      borderStyle: cs.borderTopStyle,
      shadow: cs.boxShadow,
      opacity: parseFloat(cs.opacity),
      transform: cs.transform,
      overflow: cs.overflow,
      clipped,
      raster, rasterWhole, rasterHideKids,
      color: cs.color,
      fontFamily: cs.fontFamily,
      fontSize: parseFloat(cs.fontSize) || 0,
      fontWeight: cs.fontWeight,
      fontStyle: cs.fontStyle,
      textAlign: cs.textAlign,
      lineHeight: cs.lineHeight,
      kids: [],
    };

    // Recurse unless the element is rasterized whole (img/svg) or is a text leaf.
    if (!text && !rasterWhole) {
      for (const child of el.children) {
        const n = node(child, originX, originY);
        if (n) out.kids.push(n);
      }
    }
    return out;
  }

  const rr = root.getBoundingClientRect();
  const tree = node(root, rr.left, rr.top);
  return { tree, rootW: rr.width, rootH: rr.height };
}

// Toggle visibility of a tagged element's direct children (for bg-only shots).
function setKidsHiddenFn({ id, hidden }) {
  const el = document.querySelector(`[data-w2c="${id}"]`);
  if (!el) return;
  for (const c of el.children) c.style.visibility = hidden ? 'hidden' : '';
}

// ---- Node-side mapping: collector tree -> canvas.json ---------------------

function parseColor(s) {
  if (!s) return null;
  const m = /rgba?\(([^)]+)\)/.exec(s);
  if (!m) return null;
  const p = m[1].split(',').map(x => x.trim());
  const r = Math.round(parseFloat(p[0])), g = Math.round(parseFloat(p[1])), b = Math.round(parseFloat(p[2]));
  const a = p[3] !== undefined ? parseFloat(p[3]) : 1;
  if (a === 0) return null;
  const hex = '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
  return { hex, alpha: a };
}
function solidPaint(colorStr) {
  const c = parseColor(colorStr);
  if (!c) return null;
  const p = { type: 'SOLID', color: c.hex };
  if (c.alpha < 1) p.opacity = c.alpha;
  return p;
}
function parseShadow(s) {
  if (!s || s === 'none') return null;
  const colorMatch = /rgba?\([^)]+\)|#[0-9a-fA-F]{3,8}/.exec(s);
  const color = colorMatch ? parseColor(colorMatch[0]) : { hex: '#000000', alpha: 0.25 };
  const nums = (s.replace(/rgba?\([^)]+\)/, '').match(/-?\d*\.?\d+px/g) || []).map(parseFloat);
  if (nums.length < 2) return null;
  const [ox = 0, oy = 0, blur = 0, spread = 0] = nums;
  const hex = (color && color.hex) || '#000000';
  return { type: 'DROP_SHADOW',
    color: hex + Math.round((color ? color.alpha : 0.25) * 255).toString(16).padStart(2, '0'),
    offset: { x: ox, y: oy }, radius: blur, spread };
}
function rotationDeg(transform) {
  if (!transform || transform === 'none') return 0;
  const m = /matrix\(([^)]+)\)/.exec(transform);
  if (!m) return 0;
  const v = m[1].split(',').map(parseFloat);
  return Math.atan2(v[1], v[0]) * 180 / Math.PI;
}

let nameCounter = 0;
function mapNode(n, parent) {
  const px = parent ? parent.rect.x : n.rect.x;
  const py = parent ? parent.rect.y : n.rect.y;
  const node = {
    name: n.text ? n.text.slice(0, 24) : (n.tag + '_' + (nameCounter++)),
    transform: { x: parent ? +(n.rect.x - px).toFixed(2) : 0, y: parent ? +(n.rect.y - py).toFixed(2) : 0 },
    size: { x: +n.rect.w.toFixed(2), y: +n.rect.h.toFixed(2) },
  };
  if (!n.raster) {
    const rot = rotationDeg(n.transform);  // raster shots already include rotation
    if (Math.abs(rot) > 0.1) node.transform.rotation = +rot.toFixed(2);
  }

  if (n.text) {
    node.type = 'TEXT';
    node.textData = { characters: n.text };
    const fam = (n.fontFamily || 'Inter').split(',')[0].replace(/['"]/g, '').trim();
    node.fontName = { family: fam };
    node.fontSize = n.fontSize;
    const w = parseInt(n.fontWeight, 10);
    if (!isNaN(w)) node.fontWeight = w;
    if (/px/.test(n.lineHeight)) node.lineHeight = n.lineHeight;
    node.textAlignHorizontal = (n.textAlign || 'left').toUpperCase();
    node.textAutoResize = 'HEIGHT';
    const fp = solidPaint(n.color);
    if (fp) node.fillPaints = [fp];
  } else {
    node.type = 'FRAME';
    const fills = [];
    const solid = solidPaint(n.bg);
    if (solid) fills.push(solid);
    if (n.raster) fills.push({ type: 'IMAGE', image: { filename: `images/w2c_${n.raster}.png` }, scaleMode: 'FILL' });
    if (fills.length) node.fillPaints = fills;

    const sp = (n.borderW > 0 && n.borderStyle !== 'none') ? solidPaint(n.borderColor) : null;
    if (sp) { node.strokePaints = [sp]; node.strokeWeight = n.borderW; node.strokeAlign = 'INSIDE'; }
    // Corner radius is baked into a rasterized shape; only apply to plain frames.
    if (!n.raster) {
      const r = n.radius || [0, 0, 0, 0];
      if (r.some(v => v > 0)) {
        if (r.every(v => v === r[0])) node.cornerRadius = r[0];
        else { node.topLeftRadius = r[0]; node.topRightRadius = r[1]; node.bottomRightRadius = r[2]; node.bottomLeftRadius = r[3]; }
      }
    }
    const eff = parseShadow(n.shadow);
    if (eff) node.effects = [eff];
    node.frameMaskDisabled = !(n.overflow && n.overflow !== 'visible');
  }
  if (n.opacity < 0.999) node.opacity = n.opacity;
  if (n.kids && n.kids.length) node.children = n.kids.map(k => mapNode(k, n));
  return node;
}

// collect raster marks from the tree
function rasterMarks(n, acc) {
  if (n.raster) acc.push({ id: n.raster, hideKids: n.rasterHideKids });
  for (const k of (n.kids || [])) rasterMarks(k, acc);
  return acc;
}

// ---- main -----------------------------------------------------------------

(async () => {
  const a = parseArgs(process.argv);
  if (!a.input) { console.error('usage: web2canvas <url|file.html> [-o out] [--root SEL] [--viewport WxH] [--browser msedge|chrome] [--scale N]'); process.exit(2); }
  const out = a.out || a.input.replace(/\.[^.]+$/, '') + '.canvas.json';
  const outDir = path.dirname(path.resolve(out));
  const imagesDir = path.join(outDir, 'images');

  console.log(`launching ${a.browser} ...`);
  const browser = await chromium.launch({ channel: a.browser, headless: true });
  const page = await browser.newPage({ viewport: { width: a.vw, height: a.vh }, deviceScaleFactor: a.scale });
  await setupCdnRoutes(page);

  // Serve local files over HTTP so XHR-based loaders (Babel-in-browser) work.
  let server = null, pageUrl;
  if (/^https?:\/\//.test(a.input)) {
    pageUrl = a.input;
  } else {
    const abs = path.resolve(a.input);
    server = await startStaticServer(path.dirname(abs));
    pageUrl = `http://127.0.0.1:${server.address().port}/${encodeURIComponent(path.basename(abs))}`;
  }
  console.log(`loading ${pageUrl}`);
  await page.goto(pageUrl, { waitUntil: 'networkidle' }).catch(() => {});
  await page.waitForTimeout(a.wait);
  await page.screenshot({ path: out.replace(/\.canvas\.json$|\.json$/, '') + '.web.png' }).catch(() => {});

  const { tree, rootW, rootH } = await page.evaluate(collectorFn, a.root);
  if (!tree) { await browser.close(); console.error('FAIL: nothing collected from root ' + a.root); process.exit(1); }

  // Rasterize tagged elements -> images/w2c_<id>.png
  const marks = rasterMarks(tree, []);
  if (marks.length) fs.mkdirSync(imagesDir, { recursive: true });
  let shot = 0;
  for (const m of marks) {
    const loc = page.locator(`[data-w2c="${m.id}"]`);
    try {
      if (m.hideKids) await page.evaluate(setKidsHiddenFn, { id: m.id, hidden: true });
      await loc.screenshot({ path: path.join(imagesDir, `w2c_${m.id}.png`), omitBackground: true });
      shot++;
    } catch (e) { /* element not screenshot-able; leave the fill out */ }
    finally { if (m.hideKids) await page.evaluate(setKidsHiddenFn, { id: m.id, hidden: false }); }
  }
  await browser.close();
  if (server) server.close();

  nameCounter = 0;
  const rootFrame = mapNode(tree, null);
  rootFrame.name = rootFrame.name || 'Page';
  rootFrame.scrollDirection = 'VERTICAL';
  const doc = {
    document: { type: 'DOCUMENT', children: [{ type: 'CANVAS', name: 'Page 1', children: [rootFrame] }] },
    styles: {},
  };
  fs.writeFileSync(out, JSON.stringify(doc, null, 2));
  console.log(`RESULT: OK  ${rootW}x${rootH}  ${shot}/${marks.length} rasters -> ${out}`);
})().catch(e => { console.error('FAIL:', e.message); process.exit(1); });
