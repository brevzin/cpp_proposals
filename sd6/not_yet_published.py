import json
import yaml
import re

papers = open('missing_papers').read().splitlines()
if not papers:
    new_data = []
else:
    data = yaml.load(open('../md/wg21/data/index.yaml'), Loader=yaml.CLoader)
    data = {elem['id']: elem for elem in data['references']}

    new_data = []
    pattern = re.compile('P(?P<num>\d+)R(?P<rev>\d+)')
    for paper in papers:
        m = pattern.match(paper)
        prev_rev = f'P{m["num"]}R{int(m["rev"])-1}'
        elem = data[prev_rev].copy()
        elem['id'] = paper
        elem['citation-label'] = paper
        elem['URL'] = f'https://wg21.link/{paper.lower()}'
        new_data.append(elem)

yaml.Dumper.ignore_aliases = lambda *args: True
print(yaml.safe_dump({'references': new_data}))
