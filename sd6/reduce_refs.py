import json
import yaml

def refs_to_dict(filename):
    data = yaml.load(open(filename), Loader=yaml.Loader)
    return {elem['id']: elem['title'] for elem in data['references']}

refs = refs_to_dict('../md/wg21/data/index.yaml')
refs.update(refs_to_dict('../md/wg21_fmt.yaml'))

print(json.dumps(refs))
