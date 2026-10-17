#!/usr/bin/env python3
"""Build a one-file interactive preview of the pedal face, before installing.

    python3 tools/preview/build_preview.py bundle/nhe-multiplay.lv2 <mod-ui>/html/js/modgui.js preview.html

<mod-ui> is a checkout of https://github.com/mod-audio/mod-ui (git clone --depth 1).
The page runs the face on mod-ui's own widget code (modgui.js, jQuery 1.9.1) with a
settings panel grouped by the plugin's LV2 port groups and buttons that change values
from outside the face (as a hardware footswitch or MIDI would). Open it in a browser;
it loads jQuery and fonts from the web. Needs: pip install rdflib.
"""
import base64, json, os, re, sys
import rdflib
B = sys.argv[1]; MODGUI_JS = sys.argv[2]; OUT = sys.argv[3]
L = rdflib.Namespace('http://lv2plug.in/ns/lv2core#'); PG = rdflib.Namespace('http://lv2plug.in/ns/ext/port-groups#')
g = rdflib.Graph()
for fn in os.listdir(B):
    if fn.endswith('.ttl') and fn != 'presets.ttl':
        g.parse(os.path.join(B, fn), format='turtle')
groups = {str(s): str(g.value(s, L['name'])) for s in g.subjects(rdflib.RDF.type, PG['InputGroup'])}
ports = []
for p in g.objects(None, L['port']):
    if (p, rdflib.RDF.type, L['ControlPort']) not in g or (p, rdflib.RDF.type, L['InputPort']) not in g:
        continue
    props = [str(o).split('#')[-1] for o in g.objects(p, L['portProperty'])]
    sps = sorted(({'value': float(g.value(s, rdflib.RDF.value)), 'label': str(g.value(s, rdflib.RDFS.label))}
                  for s in g.objects(p, L['scalePoint'])), key=lambda d: d['value'])
    unit = g.value(p, rdflib.URIRef('http://lv2plug.in/ns/extensions/units#unit'))
    ports.append({'index': int(g.value(p, L['index'])), 'symbol': str(g.value(p, L['symbol'])), 'name': str(g.value(p, L['name'])),
                  'ranges': {'minimum': float(g.value(p, L['minimum'])), 'maximum': float(g.value(p, L['maximum'])), 'default': float(g.value(p, L['default']))},
                  'properties': props, 'scalePoints': sps, 'unit': str(unit).split('#')[-1] if unit else '',
                  'group': groups.get(str(g.value(p, PG['group'])), ''), 'bypass': g.value(p, L['designation']) is not None})
ports.sort(key=lambda d: d['index'])
R = os.path.join(B, 'modgui')
def data_uri(fn):
    mt = 'image/jpeg' if fn.endswith('.jpg') else 'image/png'
    return 'data:%s;base64,%s' % (mt, base64.b64encode(open(os.path.join(R, fn), 'rb').read()).decode())
css = open(os.path.join(R, 'stylesheet-multiplay.css')).read().replace('{{{cns}}}', '').replace('{{{ns}}}', '')
css = re.sub(r'url\(/resources/([^)]+)\)', lambda m: 'url(%s)' % data_uri(m.group(1)), css)
html = open(os.path.join(R, 'icon-multiplay.html')).read()
html = re.sub(r'<div class="mod-pedal-(input|output)">.*?\{\{/effect\.ports\.audio\.\w+\}\}\s*</div>', '', html, flags=re.S)
html = html.replace('{{{cns}}}', '').replace('{{{ns}}}', '')
assert '{{' not in html
script = open(os.path.join(R, 'script-multiplay.js')).read()
modgui = open(os.path.join(os.path.dirname(MODGUI_JS), 'lib', 'sprintf-0.6.js')).read() + '\n' + open(MODGUI_JS).read()
assert '</script' not in modgui and '</script' not in script
tpl = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'page.html')).read()
page = (tpl.replace('/*FACE_CSS*/', css).replace('<!--FACE_HTML-->', html)
           .replace('/*MODGUI_JS*/', modgui)
           .replace('/*PORTS_JSON*/null', json.dumps(ports))
           .replace('/*FACE_SCRIPT*/null', json.dumps(script))
           .replace('/*GROUP_ORDER*/null', json.dumps(['Delay', 'Modulation', 'Mix', 'Expression'])))
open(OUT, 'w').write(page)
print(len(page), 'bytes;', len(ports), 'ports;', sorted(set(p['group'] for p in ports)))
