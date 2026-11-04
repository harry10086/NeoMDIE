with open('NeoMDIE-aero-split.svg', 'r', encoding='utf-8') as f:
    svg = f.read()

svg_scaled = svg.replace('width="512px" height="512px"', 'width="100%" height="100%"')

with open('NeoMDIE.svg', 'w', encoding='utf-8') as f:
    f.write(svg_scaled)

with open('NeoMDIE-aero-split.svg', 'w', encoding='utf-8') as f:
    f.write(svg_scaled)

print('Updated SVG files with 100% width/height')
