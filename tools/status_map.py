"""Status map: docs/status-map/data.toml -> docs/status-map/map.svg (README picture) + detail.md (table).

usage: python tools/status_map.py

Each panel is drawn as bands, one per group; an item is a box whose width follows its weight (never narrower than its
label). Colours: native = green, partial = amber (hatched), pending = grey. The panel score counts native as 1 and
partial as 1/2, weighted. The SVG follows the viewer's light/dark theme.
"""
import os
import tomllib
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(ROOT, 'docs', 'status-map')
W, PAD, LABEL_W, BOX_H, GAP = 900, 16, 120, 30, 4
CHAR_W = 6.6  # average width of a 11.5 px label character
SCORE = {'native': 1.0, 'partial': 0.5, 'pending': 0.0}
EMOJI = {'native': '✅ native', 'partial': '🔧 partial', 'pending': '⏳ pending'}

STYLE = """<style>
  .bg { fill: #ffffff; } .line { stroke: #d0d7de; } .t1 { fill: #1f2328; } .t2 { fill: #59636e; }
  .native { fill: #1a7f37; } .partial { fill: #d4a017; } .pending { fill: #c4c8ce; }
  .on-native, .on-partial { fill: #ffffff; } .on-pending { fill: #1f2328; } .hatch { stroke: #7a5200; }
  @media (prefers-color-scheme: dark) {
    .bg { fill: #0d1117; } .line { stroke: #30363d; } .t1 { fill: #e6edf3; } .t2 { fill: #9198a1; }
    .native { fill: #238636; } .pending { fill: #4b525c; } .on-pending { fill: #e6edf3; }
  }
</style>
<defs><pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
<line class="hatch" x1="0" y1="0" x2="0" y2="6" stroke-width="1.5" stroke-opacity=".5"/></pattern></defs>"""


def score(items):
    total = sum(i['weight'] for i in items)
    return 100.0 * sum(i['weight'] * SCORE[i['state']] for i in items) / total


def widths(items, avail):
    """Widths proportional to weight, but at least wide enough for each label."""
    need = [len(i['short']) * CHAR_W + 12 for i in items]
    fixed = set()
    while True:
        free = avail - GAP * (len(items) - 1) - sum(need[k] for k in fixed)
        wsum = sum(i['weight'] for k, i in enumerate(items) if k not in fixed)
        out = [need[k] if k in fixed else i['weight'] * free / wsum for k, i in enumerate(items)]
        grow = {k for k in range(len(items)) if k not in fixed and out[k] < need[k]}
        if not grow:
            return out
        fixed |= grow


def svg(data):
    parts, y = [], 0
    all_items = []
    body = []
    y = 52
    for panel in data['panel']:
        items = [i for g in panel['group'] for i in g['item']]
        all_items += items
        body.append(f'<text class="t1" x="{PAD}" y="{y + 16}" font-size="15" font-weight="600">{escape(panel["title"])}'
                    f'<tspan class="t2" font-weight="400"> · {score(items):.0f}% done</tspan></text>')
        y += 28
        for g in panel['group']:
            body.append(f'<text class="t2" x="{PAD}" y="{y + BOX_H / 2 + 4}" font-size="12.5" font-weight="600">'
                        f'{escape(g["name"])}</text>')
            x = PAD + LABEL_W
            for item, w in zip(g['item'], widths(g['item'], W - PAD - x)):
                tip = f'{item["name"]} ({item["state"]})'
                body.append(f'<g><title>{escape(tip)}</title>'
                            f'<rect class="{item["state"]}" x="{x:.1f}" y="{y}" width="{w:.1f}" height="{BOX_H}" rx="4"/>')
                if item['state'] == 'partial':
                    body.append(f'<rect fill="url(#hatch)" x="{x:.1f}" y="{y}" width="{w:.1f}" height="{BOX_H}" rx="4"/>')
                body.append(f'<text class="on-{item["state"]}" x="{x + w / 2:.1f}" y="{y + BOX_H / 2 + 4}" '
                            f'font-size="11.5" text-anchor="middle">{escape(item["short"])}</text></g>')
                x += w + GAP
            y += BOX_H + 8
        y += 14
    legend_y = y + 4
    lx = PAD
    for state, label in [('native', 'native'), ('partial', 'partial / in progress'), ('pending', 'pending')]:
        body.append(f'<rect class="{state}" x="{lx}" y="{legend_y - 10}" width="12" height="12" rx="2"/>')
        if state == 'partial':
            body.append(f'<rect fill="url(#hatch)" x="{lx}" y="{legend_y - 10}" width="12" height="12" rx="2"/>')
        body.append(f'<text class="t2" x="{lx + 17}" y="{legend_y}" font-size="12">{label}</text>')
        lx += 17 + len(label) * CHAR_W + 24
    h = legend_y + 16
    total = score(all_items)
    head = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{h}" viewBox="0 0 {W} {h}" '
            "font-family=\"-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif\" role=\"img\">",
            f'<title>Status: {total:.0f}% done</title>', STYLE,
            f'<rect class="bg" width="{W}" height="{h}" rx="6"/>',
            f'<text class="t1" x="{PAD}" y="32" font-size="22" font-weight="600">Status'
            f'<tspan class="t2" font-size="15" font-weight="400"> · {total:.0f}% overall</tspan></text>',
            f'<line class="line" x1="{PAD}" y1="42" x2="{W - PAD}" y2="42" stroke-width="1"/>']
    return '\n'.join(head + body + ['</svg>', ''])


def detail(data):
    out = ['# Status detail', '',
           'Generated from `data.toml` by `tools/status_map.py`; do not edit by hand.', '']
    for panel in data['panel']:
        items = [i for g in panel['group'] for i in g['item']]
        out += [f'## {panel["title"]} ({score(items):.0f}% done)', '', '| Group | Component | State |', '|---|---|---|']
        for g in panel['group']:
            for i in g['item']:
                out.append(f'| {g["name"]} | {i["name"]} | {EMOJI[i["state"]]} |')
        out.append('')
    return '\n'.join(out)


def main():
    with open(os.path.join(DIR, 'data.toml'), 'rb') as f:
        data = tomllib.load(f)
    for path, text in [('map.svg', svg(data)), ('detail.md', detail(data))]:
        with open(os.path.join(DIR, path), 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    print('wrote', os.path.join(DIR, 'map.svg'), 'and detail.md')


if __name__ == '__main__':
    main()
