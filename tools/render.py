#!/usr/bin/env python3
"""Render the pedal face's screenshot and thumbnail from the built bundle.

Run `make` first, then:  python3 tools/render.py
Writes bundle/nhe-multiplay.lv2/modgui/screenshot-multiplay.png and
thumbnail-multiplay.png (rebuild afterwards to copy them into bin/).
Needs playwright (Chromium) and pillow.
"""
import re, os, asyncio
from playwright.async_api import async_playwright
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = os.path.join(ROOT, 'bin', 'nhe-multiplay.lv2', 'modgui')
OUT = os.path.join(ROOT, 'bundle', 'nhe-multiplay.lv2', 'modgui')

html = open(R + '/icon-multiplay.html').read()
css = open(R + '/stylesheet-multiplay.css').read()
html = re.sub(r'\{\{#effect.*?\{\{/effect[^}]*\}\}', '', html, flags=re.S)
html = html.replace('{{{cns}}}', '').replace('{{{ns}}}', '')
css = css.replace('{{{cns}}}', '').replace('{{{ns}}}', '').replace('/resources/', 'file://' + R + '/')

def knob(v):
    return '-%dpx 0' % (round(v / 10 * 64) * 62)

state = {'speed': 3, 'width': 0, 'delay_time': 6, 'regen': 4, 'mix': 5, 'slam_level': 7}
extra = ''.join('.multiplay .mp-%s{background-position:%s}' % (k, knob(v)) for k, v in state.items())
extra += '.multiplay .mp-range{background-position:-200px 0}.multiplay .mp-effect{background-position:-168px 0}'
page = '<html><head><style>body{margin:0;background:transparent}%s%s</style></head><body>%s</body></html>' % (css, extra, html)
tmp = os.path.join(ROOT, 'build', 'face.html')
os.makedirs(os.path.dirname(tmp), exist_ok=True)
open(tmp, 'w').write(page)

async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={'width': 650, 'height': 400})
        await pg.goto('file://' + tmp)
        await pg.wait_for_timeout(300)
        await pg.screenshot(path=os.path.join(ROOT, 'build', 'screenshot_full.png'), omit_background=True)
        await b.close()

asyncio.run(main())
im = Image.open(os.path.join(ROOT, 'build', 'screenshot_full.png')).convert('RGBA')
im.quantize(256, method=Image.Quantize.FASTOCTREE).save(OUT + '/screenshot-multiplay.png', optimize=True)
t = im.copy(); t.thumbnail((256, 64), Image.LANCZOS); t.save(OUT + '/thumbnail-multiplay.png', optimize=True)
print('screenshot and thumbnail written to', OUT)
