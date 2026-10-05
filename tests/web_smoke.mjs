// Browser smoke test: loads build/site in headless Chromium, starts the game with the keyboard and
// saves screenshots. Fails on any page error.
//   python3 -m http.server 8765 -d build/site &   then   node tests/web_smoke.mjs [OUT_DIR]
import { chromium } from 'playwright';

const out = process.argv[2] || '.';
const url = process.env.URL || 'http://localhost:8765/';
const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required', '--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
const errors = [];
page.on('pageerror', e => errors.push(String(e)));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); else console.log('console:', m.text()); });
await page.goto(url);
await page.screenshot({ path: `${out}/web_start.png` });
await page.click('#play');
await page.waitForSelector('#start.hidden', { state: 'attached', timeout: 30000 });
await page.waitForTimeout(1500);
await page.screenshot({ path: `${out}/web_launcher.png` });
const key = async (k, ms = 120) => { await page.keyboard.down(k); await page.waitForTimeout(ms); await page.keyboard.up(k); };
await key('z'); await page.waitForTimeout(1500);
await page.screenshot({ path: `${out}/web_title.png` });
await key('z'); await page.waitForTimeout(800);
await key('z'); await page.waitForTimeout(600);
await page.keyboard.down('ArrowRight'); await page.waitForTimeout(1200);
await key('z', 300); await page.waitForTimeout(200);
await page.screenshot({ path: `${out}/web_play.png` });
await page.keyboard.up('ArrowRight');
await page.setViewportSize({ width: 900, height: 900 });   // a tall window: letterboxed
await page.waitForTimeout(500);
await page.screenshot({ path: `${out}/web_tall.png` });
const fps = await page.evaluate(() => new Promise(r => { let n = 0; const t0 = performance.now(); const f = () => { n++; if (performance.now() - t0 < 2000) requestAnimationFrame(f); else r(n / 2); }; requestAnimationFrame(f); }));
console.log('frames per second (headless, software GL):', fps.toFixed(1));
await browser.close();
if (errors.length) { console.log('ERRORS:\n' + errors.join('\n')); process.exit(1); }
console.log('web smoke test passed');
