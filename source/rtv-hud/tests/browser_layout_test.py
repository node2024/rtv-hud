"""Verify generic scrolling slots, inherited variable contract and native ID budget."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[1]
p=root/'workshop/panorama/layout/custom_game/rtv_hud/browser_scroll.xml'
layout=ET.parse(p)
capacity=int(re.search(r'BrowserRowCapacity = (\d+)',(root/'src/browser.h').read_text())[1])
ids=[e.get('id') for e in layout.iter() if e.get('id')]
assert len(ids)==len(set(ids)) and len(ids)<1024
rows=[e for e in layout.iter('Button') if e.get('id','').startswith('row_')]
assert len(rows)==capacity
for i,row in enumerate(rows):
 assert row.get('id')==f'row_{i}' and 'collapsed' in row.get('class')
 labels=list(row)
 assert [e.get('text') for e in labels]==['{s:global}','{s:text}']
 assert all(e.tag=='Label' and e.get('id') is None for e in labels)
assert not any(x in ids for x in ['next_page','previous_page','page_label'])
assert not re.search(r'kz_|bkz_|skz_|[0-9]{10}',p.read_text())
assert not list(layout.iter('scripts'))
style=(root/'workshop/panorama/styles/custom_game/rtv_hud/browser_scroll.css').read_text()
assert 'overflow: squish scroll' in style and '.pager' not in style
plugin=(root/'src/plugin.cpp').read_text()
assert 'browser_scroll.vxml_c' in plugin and 'browserCovers' not in plugin
print(f'No-pager generic layout: {capacity} slots, {len(ids)} panel IDs, no embedded maps.')
