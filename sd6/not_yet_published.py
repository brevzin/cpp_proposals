import json
import yaml
import re

data = yaml.load(open('../md/wg21/data/index.yaml'), Loader=yaml.Loader)
data = {elem['id']: elem for elem in data['references']}

new_data = []
pattern = re.compile('P(?P<num>\d+)R(?P<rev>\d+)')
with open('missing_papers') as f:
    for line in f:
        paper = line.strip()
        m = pattern.match(paper)
        prev_rev = f'P{m["num"]}R{int(m["rev"])-1}'
        elem = data[prev_rev].copy()
        elem['id'] = paper
        elem['citation-label'] = paper
        elem['URL'] = f'https://wg21.link/{paper.lower()}'
        new_data.append(elem)

yaml.Dumper.ignore_aliases = lambda *args: True
print(yaml.safe_dump({'references': new_data}))
