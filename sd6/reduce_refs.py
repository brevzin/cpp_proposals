import json
import yaml

def refs_to_dict(filename):
    data = yaml.load(open(filename), Loader=yaml.CLoader)
    return {elem['id']: elem['title'] for elem in data['references']}

refs = refs_to_dict('../md/wg21/data/index.yaml')
refs.update(refs_to_dict('../md/wg21_fmt.yaml'))
refs.update(refs_to_dict('missing.yaml'))

print(json.dumps(refs, indent=4))
