"""Converts guide.bbcode into the GitHub-readable guide.md.

Usage (from docs/steam-guide): python bbcode_to_md.py guide.bbcode guide.md
"""
import re, sys
src, dst = sys.argv[1], sys.argv[2]
text = open(src, encoding='utf-8').read()
parts = re.split(r'^=== SECTION (\d+) (TITLE|BODY):[ \t]*', text, flags=re.M)
sections, i = [], 1
while i < len(parts):
    n, kind, val = parts[i], parts[i + 1], parts[i + 2]
    if kind == 'TITLE': sections.append([val.strip(), ''])
    else: sections[-1][1] = val.strip()
    i += 3

def inline(s):
    s = re.sub(r'\[b\](.*?)\[/b\]', r'**\1**', s)
    s = re.sub(r'\[i\](.*?)\[/i\]', r'*\1*', s)
    s = re.sub(r'\[u\](.*?)\[/u\]', r'**\1**', s)
    s = re.sub(r'\[url=(.*?)\](.*?)\[/url\]', r'[\2](\1)', s)
    s = re.sub(r'(?<![(\[])(https?://[^\s\[\]<>]+)', r'<\1>', s)
    return s

ALT = {
    'compare-bricksc.jpg': 'Neon corridor: original on the left, enhanced on the right',
    'compare-sunstrip.jpg': 'Rooftop walkway: original on the left, enhanced on the right',
    'compare-museum.jpg': 'Museum hallway: original on the left, enhanced on the right',
    'compare-hephtower2.jpg': 'Tower interior: original on the left, enhanced on the right',
    'install-files.jpg': 'The four enhancer files copied next to anox.exe in the Anachronox folder',
    'widescreen-compare.jpg': 'Widescreen off (4:3 with black bars) above widescreen on (full 16:10)',
    'config-editor.jpg': 'config_editor.exe with the pipeline list and settings panel',
}

def blank_before_blocks(lines):
    """Ensure a blank line before lists, tables, fences and images so GitHub renders them."""
    res, fence = [], False
    for l in lines:
        if fence:
            res.append(l)
            if l.startswith('```'): fence = False
            continue
        starts_block = re.match(r'(- |\d+\. |\| |```|!\[)', l)
        same_block = re.match(r'(- |\d+\. |\| )', res[-1]) if res else None
        if starts_block and res and res[-1].strip() and not (same_block and not l.startswith(('```', '!['))):
            res.append('')
        elif res and res[-1].startswith('![') and l.strip():
            res.append('')  # a caption under a picture gets its own paragraph
        if l.startswith('```'): fence = True
        res.append(l)
    return res

def body(b):
    b = re.sub(r'\[code\]\n?(.*?)\n?\[/code\]', lambda m: '\n```\n' + m.group(1).strip('\n') + '\n```\n', b, flags=re.S)
    b = re.sub(r'\{\{IMAGE: (.*?)\}\}', lambda m: f'![{ALT.get(m.group(1), m.group(1))}](images/{m.group(1)})', b)
    out, in_code, olist, table = [], False, None, []
    for line in b.split('\n'):
        if line.startswith('```'):
            in_code = not in_code; out.append(line); continue
        if in_code: out.append(line); continue
        t = line.strip()
        if t in ('[list]', '[/list]'): olist = None if t == '[/list]' else 0; out.append('') if t == '[/list]' else None; continue
        if t == '[olist]': olist = 1; continue
        if t == '[/olist]': olist = None; out.append(''); continue
        if t.startswith('[table'): table = []; continue
        if t == '[/table]':
            out.append('| ' + ' | '.join(table[0]) + ' |'); out.append('|' + ' --- |' * len(table[0]))
            out += ['| ' + ' | '.join(r) + ' |' for r in table[1:]]; table = None; out.append(''); continue
        if table is not None and t.startswith('[tr]'):
            table.append([inline(c) for c in re.findall(r'\[t[hd]\](.*?)\[/t[hd]\]', t)]); continue
        m = re.match(r'\[h(\d)\](.*?)\[/h\d\]', t)
        if m: out.append('#' * (int(m.group(1)) + 1) + ' ' + inline(m.group(2))); continue
        if t.startswith('[*]'):
            item = inline(t[3:])
            if olist: out.append(f'{olist}. {item}'); olist += 1
            else: out.append(f'- {item}')
            continue
        out.append(inline(line))
    md = '\n'.join(blank_before_blocks(out))
    return re.sub(r'\n{3,}', '\n\n', md).strip()

def bold_lines_to_headings(md):
    # A line that is nothing but bold text (Troubleshooting's questions) reads better as a heading.
    return re.sub(r'^\*\*([^*]+[^:*])\*\*\n\n?', r'### \1\n\n', md, flags=re.M)

title = re.search(r'Title:\s*(.*)', text).group(1).strip()
desc = ' '.join(l.strip() for l in re.search(r'Description:\s*(.*?)\n\s*Preview image', text, re.S).group(1).split('\n'))
anchor = lambda s: re.sub(r'[^a-z0-9 -]', '', s.lower()).replace(' ', '-')
res = [f'# {title}', '', '![OpenGL32 Enhancer for Anachronox](images/cover.jpg)', '', f'*{desc}*', '',
       '> This is the readable copy of the Steam Community guide. The Steam version is',
       '> [guide.bbcode](guide.bbcode); [README.md](README.md) explains how to publish it.', '',
       '## Contents', ''] + [f'{k+1}. [{s[0]}](#{anchor(s[0])})' for k, s in enumerate(sections)] + ['']
for t, b in sections: res += [f'## {t}', '', bold_lines_to_headings(body(b)), '']
open(dst, 'w', encoding='utf-8', newline='\n').write('\n'.join(res).rstrip('\n') + '\n')
print(dst, len(sections), 'sections')
